// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/sockaddr_storage.h"
#include "net/quic/core/crypto/quic_random.h"
#include "net/quic/core/quic_bug_tracker.h"
#include "net/quic/core/quic_connection.h"
#include "net/quic/core/quic_data_reader.h"
#include "net/quic/core/quic_flags.h"
#include "net/quic/core/quic_protocol.h"
#include "net/quic/core/quic_server_id.h"
#include "net/quic/core/spdy_utils.h"
#include "net/tools/quic/quic_epoll_alarm_factory.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_socket_utils.h"
#include "net/tools/quic/spdy_balsa_utils.h"

#ifndef SO_RXQ_OVFL
#define SO_RXQ_OVFL 40
#endif

// TODO(rtenneti): Add support for MMSG_MORE.
#define MMSG_MORE 0

using base::StringPiece;
using base::StringToInt;
using std::string;
using std::vector;

namespace net {

const int kEpollFlags = EPOLLIN | EPOLLOUT | EPOLLET;

QuicClient::QuicClient(IPEndPoint server_address,
                       const QuicServerId& server_id,
                       const QuicVersionVector& supported_versions,
                       EpollServer* epoll_server,
                       std::unique_ptr<ProofVerifier> proof_verifier)
    : QuicClient(server_address,
                 server_id,
                 supported_versions,
                 QuicConfig(),
                 epoll_server,
                 std::move(proof_verifier)) {}

QuicClient::QuicClient(IPEndPoint server_address,
                       const QuicServerId& server_id,
                       const QuicVersionVector& supported_versions,
                       const QuicConfig& config,
                       EpollServer* epoll_server,
                       std::unique_ptr<ProofVerifier> proof_verifier)
    : QuicClientBase(
          server_id,
          supported_versions,
          config,
          new QuicEpollConnectionHelper(epoll_server, QuicAllocator::SIMPLE),
          new QuicEpollAlarmFactory(epoll_server),
          std::move(proof_verifier)),
      epoll_server_(epoll_server),
      initialized_(false),
      packets_dropped_(0),
      overflow_supported_(false),
      packet_reader_(new QuicPacketReader()) {
  set_server_address(server_address);
}

QuicClient::~QuicClient() {
  if (connected()) {
    session()->connection()->CloseConnection(
        QUIC_PEER_GOING_AWAY, "Client being torn down",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  }

  CleanUpAllUDPSockets();
}

bool QuicClient::Initialize() {
  QuicClientBase::Initialize();

  set_num_sent_client_hellos(0);
  set_num_stateless_rejects_received(0);
  set_connection_error(QUIC_NO_ERROR);

  // If an initial flow control window has not explicitly been set, then use the
  // same values that Chrome uses.
  const uint32_t kSessionMaxRecvWindowSize = 15 * 1024 * 1024;  // 15 MB
  const uint32_t kStreamMaxRecvWindowSize = 6 * 1024 * 1024;    //  6 MB
  if (config()->GetInitialStreamFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config()->SetInitialStreamFlowControlWindowToSend(kStreamMaxRecvWindowSize);
  }
  if (config()->GetInitialSessionFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config()->SetInitialSessionFlowControlWindowToSend(
        kSessionMaxRecvWindowSize);
  }

  epoll_server_->set_timeout_in_us(50 * 1000);

  if (!CreateUDPSocketAndBind()) {
    return false;
  }

  epoll_server_->RegisterFD(GetLatestFD(), this, kEpollFlags);
  initialized_ = true;
  return true;
}

bool QuicClient::CreateUDPSocketAndBind() {
  int fd =
      QuicSocketUtils::CreateUDPSocket(server_address(), &overflow_supported_);
  if (fd < 0) {
    return false;
  }

  IPEndPoint client_address;
  if (bind_to_address().size() != 0) {
    client_address = IPEndPoint(bind_to_address(), local_port());
  } else if (server_address().GetSockAddrFamily() == AF_INET) {
    client_address = IPEndPoint(IPAddress::IPv4AllZeros(), local_port());
  } else {
    IPAddress any6 = IPAddress::IPv6AllZeros();
    client_address = IPEndPoint(any6, local_port());
  }

  sockaddr_storage raw_addr;
  socklen_t raw_addr_len = sizeof(raw_addr);
  CHECK(client_address.ToSockAddr(reinterpret_cast<sockaddr*>(&raw_addr),
                                  &raw_addr_len));
  int rc =
      bind(fd, reinterpret_cast<const sockaddr*>(&raw_addr), sizeof(raw_addr));
  if (rc < 0) {
    LOG(ERROR) << "Bind failed: " << strerror(errno);
    return false;
  }

  SockaddrStorage storage;
  if (getsockname(fd, storage.addr, &storage.addr_len) != 0 ||
      !client_address.FromSockAddr(storage.addr, storage.addr_len)) {
    LOG(ERROR) << "Unable to get self address.  Error: " << strerror(errno);
  }

  fd_address_map_[fd] = client_address;

  return true;
}

bool QuicClient::Connect() {
  // Attempt multiple connects until the maximum number of client hellos have
  // been sent.
  while (!connected() &&
         GetNumSentClientHellos() <= QuicCryptoClientStream::kMaxClientHellos) {
    StartConnect();
    while (EncryptionBeingEstablished()) {
      WaitForEvents();
    }
    if (FLAGS_enable_quic_stateless_reject_support && connected()) {
      // Resend any previously queued data.
      ResendSavedData();
    }
    if (session() != nullptr &&
        session()->error() != QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
      // We've successfully created a session but we're not connected, and there
      // is no stateless reject to recover from.  Give up trying.
      break;
    }
  }
  if (!connected() &&
      GetNumSentClientHellos() > QuicCryptoClientStream::kMaxClientHellos &&
      session() != nullptr &&
      session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    // The overall connection failed due too many stateless rejects.
    set_connection_error(QUIC_CRYPTO_TOO_MANY_REJECTS);
  }
  return session()->connection()->connected();
}

void QuicClient::StartConnect() {
  DCHECK(initialized_);
  DCHECK(!connected());

  QuicPacketWriter* writer = CreateQuicPacketWriter();

  if (connected_or_attempting_connect()) {
    // If the last error was not a stateless reject, then the queued up data
    // does not need to be resent.
    if (session()->error() != QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
      ClearDataToResend();
    }
    // Before we destroy the last session and create a new one, gather its stats
    // and update the stats for the overall connection.
    UpdateStats();
  }

  CreateQuicClientSession(new QuicConnection(
      GetNextConnectionId(), server_address(), helper(), alarm_factory(),
      writer,
      /* owns_writer= */ false, Perspective::IS_CLIENT, supported_versions()));

  // Reset |writer()| after |session()| so that the old writer outlives the old
  // session.
  set_writer(writer);
  session()->Initialize();
  session()->CryptoConnect();
  set_connected_or_attempting_connect(true);
}

void QuicClient::Disconnect() {
  DCHECK(initialized_);

  if (connected()) {
    session()->connection()->CloseConnection(
        QUIC_PEER_GOING_AWAY, "Client disconnecting",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  }

  ClearDataToResend();

  CleanUpAllUDPSockets();

  initialized_ = false;
}

void QuicClient::CleanUpUDPSocket(int fd) {
  CleanUpUDPSocketImpl(fd);
  fd_address_map_.erase(fd);
}

void QuicClient::CleanUpAllUDPSockets() {
  for (std::pair<int, IPEndPoint> fd_address : fd_address_map_) {
    CleanUpUDPSocketImpl(fd_address.first);
  }
  fd_address_map_.clear();
}

void QuicClient::CleanUpUDPSocketImpl(int fd) {
  if (fd > -1) {
    epoll_server_->UnregisterFD(fd);
    int rc = close(fd);
    DCHECK_EQ(0, rc);
  }
}

void QuicClient::SendRequest(const SpdyHeaderBlock& headers,
                             StringPiece body,
                             bool fin) {
  QuicClientPushPromiseIndex::TryHandle* handle;
  QuicAsyncStatus rv = push_promise_index()->Try(headers, this, &handle);
  if (rv == QUIC_SUCCESS)
    return;

  if (rv == QUIC_PENDING) {
    // May need to retry request if asynchronous rendezvous fails.
    AddPromiseDataToResend(headers, body, fin);
    return;
  }

  QuicSpdyClientStream* stream = CreateReliableClientStream();
  if (stream == nullptr) {
    QUIC_BUG << "stream creation failed!";
    return;
  }
  stream->SendRequest(headers.Clone(), body, fin);
  // Record this in case we need to resend.
  MaybeAddDataToResend(headers, body, fin);
}

void QuicClient::SendRequestAndWaitForResponse(const SpdyHeaderBlock& headers,
                                               StringPiece body,
                                               bool fin) {
  SendRequest(headers, body, fin);
  while (WaitForEvents()) {
  }
}

void QuicClient::SendRequestsAndWaitForResponse(
    const vector<string>& url_list) {
  for (size_t i = 0; i < url_list.size(); ++i) {
    SpdyHeaderBlock headers;
    if (!SpdyUtils::PopulateHeaderBlockFromUrl(url_list[i], &headers)) {
      QUIC_BUG << "Unable to create request";
      continue;
    }
    SendRequest(headers, "", true);
  }
  while (WaitForEvents()) {
  }
}

bool QuicClient::WaitForEvents() {
  DCHECK(connected());

  epoll_server_->WaitForEventsAndExecuteCallbacks();
  base::RunLoop().RunUntilIdle();

  DCHECK(session() != nullptr);
  if (!connected() &&
      session()->error() == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT) {
    DCHECK(FLAGS_enable_quic_stateless_reject_support);
    DVLOG(1) << "Detected stateless reject while waiting for events.  "
             << "Attempting to reconnect.";
    Connect();
  }

  return session()->num_active_requests() != 0;
}

bool QuicClient::MigrateSocket(const IPAddress& new_host) {
  if (!connected()) {
    return false;
  }

  CleanUpUDPSocket(GetLatestFD());

  set_bind_to_address(new_host);
  if (!CreateUDPSocketAndBind()) {
    return false;
  }

  epoll_server_->RegisterFD(GetLatestFD(), this, kEpollFlags);
  session()->connection()->SetSelfAddress(GetLatestClientAddress());

  QuicPacketWriter* writer = CreateQuicPacketWriter();
  set_writer(writer);
  session()->connection()->SetQuicPacketWriter(writer, false);

  return true;
}

void QuicClient::OnEvent(int fd, EpollEvent* event) {
  DCHECK_EQ(fd, GetLatestFD());

  if (event->in_events & EPOLLIN) {
    bool more_to_read = true;
    while (connected() && more_to_read) {
      more_to_read = packet_reader_->ReadAndDispatchPackets(
          GetLatestFD(), QuicClient::GetLatestClientAddress().port(),
          false /* potentially_small_mtu */, *helper()->GetClock(), this,
          overflow_supported_ ? &packets_dropped_ : nullptr);
    }
  }
  if (connected() && (event->in_events & EPOLLOUT)) {
    writer()->SetWritable();
    session()->connection()->OnCanWrite();
  }
  if (event->in_events & EPOLLERR) {
    DVLOG(1) << "Epollerr";
  }
}

QuicPacketWriter* QuicClient::CreateQuicPacketWriter() {
  return new QuicDefaultPacketWriter(GetLatestFD());
}

const IPEndPoint QuicClient::GetLatestClientAddress() const {
  if (fd_address_map_.empty()) {
    return IPEndPoint();
  }

  return fd_address_map_.back().second;
}

int QuicClient::GetLatestFD() const {
  if (fd_address_map_.empty()) {
    return -1;
  }

  return fd_address_map_.back().first;
}

void QuicClient::ProcessPacket(const IPEndPoint& self_address,
                               const IPEndPoint& peer_address,
                               const QuicReceivedPacket& packet) {
  session()->connection()->ProcessUdpPacket(self_address, peer_address, packet);
}

}  // namespace net
