// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_dispatcher.h"

#include <errno.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_utils.h"
#include "net/tools/quic/quic_default_packet_writer.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_packet_writer_wrapper.h"
#include "net/tools/quic/quic_socket_utils.h"

namespace net {

namespace tools {

using base::StringPiece;
using std::make_pair;

class DeleteSessionsAlarm : public EpollAlarm {
 public:
  explicit DeleteSessionsAlarm(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {
  }

  virtual int64 OnAlarm() OVERRIDE {
    EpollAlarm::OnAlarm();
    dispatcher_->DeleteSessions();
    return 0;
  }

 private:
  QuicDispatcher* dispatcher_;
};

class QuicDispatcher::QuicFramerVisitor : public QuicFramerVisitorInterface {
 public:
  explicit QuicFramerVisitor(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}

  // QuicFramerVisitorInterface implementation
  virtual void OnPacket() OVERRIDE {}
  virtual bool OnUnauthenticatedPublicHeader(
      const QuicPacketPublicHeader& header) OVERRIDE {
    return dispatcher_->OnUnauthenticatedPublicHeader(header);
  }
  virtual bool OnUnauthenticatedHeader(
      const QuicPacketHeader& header) OVERRIDE {
    dispatcher_->OnUnauthenticatedHeader(header);
    return false;
  }
  virtual void OnError(QuicFramer* framer) OVERRIDE {
    DVLOG(1) << QuicUtils::ErrorToString(framer->error());
  }

  // The following methods should never get called because we always return
  // false from OnUnauthenticatedHeader().  As a result, we never process the
  // payload of the packet.
  virtual bool OnProtocolVersionMismatch(
      QuicVersion /*received_version*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& /*packet*/) OVERRIDE {
    DCHECK(false);
  }
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& /*packet*/) OVERRIDE {
    DCHECK(false);
  }
  virtual void OnPacketComplete() OVERRIDE {
    DCHECK(false);
  }
  virtual bool OnPacketHeader(const QuicPacketHeader& /*header*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual void OnRevivedPacket() OVERRIDE {
    DCHECK(false);
  }
  virtual void OnFecProtectedPayload(StringPiece /*payload*/) OVERRIDE {
    DCHECK(false);
  }
  virtual bool OnStreamFrame(const QuicStreamFrame& /*frame*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual bool OnAckFrame(const QuicAckFrame& /*frame*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& /*frame*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& /*frame*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame & /*frame*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& /*frame*/) OVERRIDE {
    DCHECK(false);
    return false;
  }
  virtual void OnFecData(const QuicFecData& /*fec*/) OVERRIDE {
    DCHECK(false);
  }

 private:
  QuicDispatcher* dispatcher_;
};

QuicDispatcher::QuicDispatcher(const QuicConfig& config,
                               const QuicCryptoServerConfig& crypto_config,
                               const QuicVersionVector& supported_versions,
                               EpollServer* epoll_server)
    : config_(config),
      crypto_config_(crypto_config),
      delete_sessions_alarm_(new DeleteSessionsAlarm(this)),
      epoll_server_(epoll_server),
      helper_(new QuicEpollConnectionHelper(epoll_server_)),
      supported_versions_(supported_versions),
      current_packet_(NULL),
      framer_(supported_versions, /*unused*/ QuicTime::Zero(), true),
      framer_visitor_(new QuicFramerVisitor(this)) {
  framer_.set_visitor(framer_visitor_.get());
}

QuicDispatcher::~QuicDispatcher() {
  STLDeleteValues(&session_map_);
  STLDeleteElements(&closed_session_list_);
}

void QuicDispatcher::Initialize(int fd) {
  DCHECK(writer_ == NULL);
  writer_.reset(CreateWriterWrapper(CreateWriter(fd)));
  time_wait_list_manager_.reset(
      new QuicTimeWaitListManager(writer_.get(), this,
                                  epoll_server(), supported_versions()));
}

void QuicDispatcher::ProcessPacket(const IPEndPoint& server_address,
                                   const IPEndPoint& client_address,
                                   const QuicEncryptedPacket& packet) {
  current_server_address_ = server_address;
  current_client_address_ = client_address;
  current_packet_ = &packet;
  // ProcessPacket will cause the packet to be dispatched in
  // OnUnauthenticatedPublicHeader, or sent to the time wait list manager
  // in OnAuthenticatedHeader.
  framer_.ProcessPacket(packet);
  // TODO(rjshade): Return a status describing if/why a packet was dropped,
  //                and log somehow.  Maybe expose as a varz.
}

bool QuicDispatcher::OnUnauthenticatedPublicHeader(
    const QuicPacketPublicHeader& header) {
  QuicSession* session = NULL;

  QuicGuid guid = header.guid;
  SessionMap::iterator it = session_map_.find(guid);
  if (it == session_map_.end()) {
    if (header.reset_flag) {
      return false;
    }
    if (time_wait_list_manager_->IsGuidInTimeWait(guid)) {
      return HandlePacketForTimeWait(header);
    }

    // Ensure the packet has a version negotiation bit set before creating a new
    // session for it.  All initial packets for a new connection are required to
    // have the flag set.  Otherwise it may be a stray packet.
    if (header.version_flag) {
      session = CreateQuicSession(guid, current_server_address_,
                                  current_client_address_);
    }

    if (session == NULL) {
      DVLOG(1) << "Failed to create session for " << guid;
      // Add this guid fo the time-wait state, to safely reject future packets.

      if (header.version_flag &&
          !framer_.IsSupportedVersion(header.versions.front())) {
        // TODO(ianswett): Produce a no-version version negotiation packet.
        return false;
      }

      // Use the version in the packet if possible, otherwise assume the latest.
      QuicVersion version = header.version_flag ? header.versions.front() :
          supported_versions_.front();
      time_wait_list_manager_->AddGuidToTimeWait(guid, version, NULL);
      DCHECK(time_wait_list_manager_->IsGuidInTimeWait(guid));
      return HandlePacketForTimeWait(header);
    }
    DVLOG(1) << "Created new session for " << guid;
    session_map_.insert(make_pair(guid, session));
  } else {
    session = it->second;
  }

  session->connection()->ProcessUdpPacket(
      current_server_address_, current_client_address_, *current_packet_);

  // Do not parse the packet further.  The session will process it completely.
  return false;
}

void QuicDispatcher::OnUnauthenticatedHeader(const QuicPacketHeader& header) {
  DCHECK(time_wait_list_manager_->IsGuidInTimeWait(header.public_header.guid));
  time_wait_list_manager_->ProcessPacket(current_server_address_,
                                         current_client_address_,
                                         header.public_header.guid,
                                         header.packet_sequence_number);
}

void QuicDispatcher::CleanUpSession(SessionMap::iterator it) {
  QuicConnection* connection = it->second->connection();
  QuicEncryptedPacket* connection_close_packet =
          connection->ReleaseConnectionClosePacket();
  write_blocked_list_.erase(connection);
  time_wait_list_manager_->AddGuidToTimeWait(it->first,
                                             connection->version(),
                                             connection_close_packet);
  session_map_.erase(it);
}

void QuicDispatcher::DeleteSessions() {
  STLDeleteElements(&closed_session_list_);
}

bool QuicDispatcher::OnCanWrite() {
  // We got an EPOLLOUT: the socket should not be blocked.
  writer_->SetWritable();

  // Give each writer one attempt to write.
  int num_writers = write_blocked_list_.size();
  for (int i = 0; i < num_writers; ++i) {
    if (write_blocked_list_.empty()) {
      break;
    }
    QuicBlockedWriterInterface* writer = write_blocked_list_.begin()->first;
    write_blocked_list_.erase(write_blocked_list_.begin());
    bool can_write_more = writer->OnCanWrite();
    if (writer_->IsWriteBlocked()) {
      // We were unable to write.  Wait for the next EPOLLOUT.
      // In this case, the session would have been added to the blocked list
      // up in WritePacket.
      return false;
    }
    // The socket is not blocked but the writer has ceded work.  Add it to the
    // end of the list.
    if (can_write_more) {
      write_blocked_list_.insert(make_pair(writer, true));
    }
  }

  // We're not write blocked.  Return true if there's more work to do.
  return !write_blocked_list_.empty();
}

void QuicDispatcher::Shutdown() {
  while (!session_map_.empty()) {
    QuicSession* session = session_map_.begin()->second;
    session->connection()->SendConnectionClose(QUIC_PEER_GOING_AWAY);
    // Validate that the session removes itself from the session map on close.
    DCHECK(session_map_.empty() || session_map_.begin()->second != session);
  }
  DeleteSessions();
}

void QuicDispatcher::OnConnectionClosed(QuicGuid guid, QuicErrorCode error) {
  SessionMap::iterator it = session_map_.find(guid);
  if (it == session_map_.end()) {
    LOG(DFATAL) << "GUID " << guid << " does not exist in the session map.  "
                << "Error: " << QuicUtils::ErrorToString(error);
    return;
  }

  DLOG_IF(INFO, error != QUIC_NO_ERROR) << "Closing connection (" << guid
                                        << ") due to error: "
                                        << QuicUtils::ErrorToString(error);

  if (closed_session_list_.empty()) {
    epoll_server_->RegisterAlarmApproximateDelta(
        0, delete_sessions_alarm_.get());
  }
  closed_session_list_.push_back(it->second);
  CleanUpSession(it);
}

void QuicDispatcher::OnWriteBlocked(QuicBlockedWriterInterface* writer) {
  DCHECK(writer_->IsWriteBlocked());
  write_blocked_list_.insert(make_pair(writer, true));
}

QuicPacketWriter* QuicDispatcher::CreateWriter(int fd) {
  return new QuicDefaultPacketWriter(fd);
}

QuicPacketWriterWrapper* QuicDispatcher::CreateWriterWrapper(
    QuicPacketWriter* writer) {
  return new QuicPacketWriterWrapper(writer);
}

QuicSession* QuicDispatcher::CreateQuicSession(
    QuicGuid guid,
    const IPEndPoint& server_address,
    const IPEndPoint& client_address) {
  QuicServerSession* session = new QuicServerSession(
      config_,
      CreateQuicConnection(guid, server_address, client_address),
      this);
  session->InitializeSession(crypto_config_);
  return session;
}

QuicConnection* QuicDispatcher::CreateQuicConnection(
    QuicGuid guid,
    const IPEndPoint& server_address,
    const IPEndPoint& client_address) {
  return new QuicConnection(guid, client_address, helper_.get(), writer_.get(),
                            true, supported_versions_);
}

void QuicDispatcher::set_writer(QuicPacketWriter* writer) {
  writer_->set_writer(writer);
}

bool QuicDispatcher::HandlePacketForTimeWait(
    const QuicPacketPublicHeader& header) {
  if (header.reset_flag) {
    // Public reset packets do not have sequence numbers, so ignore the packet.
    return false;
  }

  // Switch the framer to the correct version, so that the sequence number can
  // be parsed correctly.
  framer_.set_version(time_wait_list_manager_->GetQuicVersionFromGuid(
      header.guid));

  // Continue parsing the packet to extract the sequence number.  Then
  // send it to the time wait manager in OnUnathenticatedHeader.
  return true;
}

}  // namespace tools
}  // namespace net
