// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_time_wait_list_manager.h"

#include <errno.h>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"

namespace net {

namespace {

// Time period for which the guid should live in time wait state..
const int kTimeWaitSeconds = 5;

}  // namespace

// A very simple alarm that just informs the QuicTimeWaitListManager to clean
// up old guids. This alarm should be unregistered and deleted before the
// QuicTimeWaitListManager is deleted.
class GuidCleanUpAlarm : public EpollAlarm {
 public:
  explicit GuidCleanUpAlarm(QuicTimeWaitListManager* time_wait_list_manager)
      : time_wait_list_manager_(time_wait_list_manager) {
  }

  virtual int64 OnAlarm() {
    EpollAlarm::OnAlarm();
    time_wait_list_manager_->CleanUpOldGuids();
    // Let the time wait manager register the alarm at appropriate time.
    return 0;
  }

 private:
  // Not owned.
  QuicTimeWaitListManager* time_wait_list_manager_;
};

struct QuicTimeWaitListManager::GuidAddTime {
  GuidAddTime(QuicGuid guid, const QuicTime& time)
      : guid(guid),
        time_added(time) {
  }

  QuicGuid guid;
  QuicTime time_added;
};

// This class stores pending public reset packets to be sent to clients.
// server_address - server address on which a packet what was received for
//                  a guid in time wait state.
// client_address - address of the client that sent that packet. Needed to send
//                  the public reset packet back to the client.
// packet - the pending public reset packet that is to be sent to the client.
//          created instance takes the ownership of this packet.
class QuicTimeWaitListManager::QueuedPacket {
 public:
  QueuedPacket(const IPEndPoint& server_address,
               const IPEndPoint& client_address,
               QuicEncryptedPacket* packet)
      : server_address_(server_address),
        client_address_(client_address),
        packet_(packet) {
  }

  const IPEndPoint& server_address() const { return server_address_; }
  const IPEndPoint& client_address() const { return client_address_; }
  QuicEncryptedPacket* packet() { return packet_.get(); }

 private:
  const IPEndPoint server_address_;
  const IPEndPoint client_address_;
  scoped_ptr<QuicEncryptedPacket> packet_;

  DISALLOW_COPY_AND_ASSIGN(QueuedPacket);
};

QuicTimeWaitListManager::QuicTimeWaitListManager(
    QuicPacketWriter* writer,
    EpollServer* epoll_server)
    : framer_(kQuicVersion1,
              QuicDecrypter::Create(kNULL),
              QuicEncrypter::Create(kNULL),
              true),
      epoll_server_(epoll_server),
      kTimeWaitPeriod_(QuicTime::Delta::FromSeconds(kTimeWaitSeconds)),
      guid_clean_up_alarm_(new GuidCleanUpAlarm(this)),
      clock_(epoll_server),
      writer_(writer),
      is_write_blocked_(false) {
  framer_.set_visitor(this);
  SetGuidCleanUpAlarm();
}

QuicTimeWaitListManager::~QuicTimeWaitListManager() {
  guid_clean_up_alarm_->UnregisterIfRegistered();
  STLDeleteElements(&time_ordered_guid_list_);
  STLDeleteElements(&pending_packets_queue_);
}

void QuicTimeWaitListManager::AddGuidToTimeWait(QuicGuid guid) {
  DCHECK(!IsGuidInTimeWait(guid));
  // Initialize the guid with 0 packets received.
  guid_map_.insert(std::make_pair(guid, 0));
  time_ordered_guid_list_.push_back(new GuidAddTime(guid,
                                                    clock_.ApproximateNow()));
}

bool QuicTimeWaitListManager::IsGuidInTimeWait(QuicGuid guid) const {
  return guid_map_.find(guid) != guid_map_.end();
}

void QuicTimeWaitListManager::ProcessPacket(
    const IPEndPoint& server_address,
    const IPEndPoint& client_address,
    QuicGuid guid,
    const QuicEncryptedPacket& packet) {
  DCHECK(IsGuidInTimeWait(guid));
  server_address_ = server_address;
  client_address_ = client_address;
  // TODO(satyamshekhar): Also store the version of protocol for and
  // update the protocol version of the framer before processing.
  framer_.ProcessPacket(packet);
}

bool QuicTimeWaitListManager::OnCanWrite() {
  is_write_blocked_ = false;
  while (!is_write_blocked_ && !pending_packets_queue_.empty()) {
    QueuedPacket* queued_packet = pending_packets_queue_.front();
    WriteToWire(queued_packet);
    if (!is_write_blocked_) {
      pending_packets_queue_.pop_front();
      delete queued_packet;
    }
  }

  return !is_write_blocked_;
}

void QuicTimeWaitListManager::OnError(QuicFramer* framer) {
  DLOG(INFO) << QuicUtils::ErrorToString(framer->error());
}

bool QuicTimeWaitListManager::OnProtocolVersionMismatch(
    QuicVersionTag received_version) {
  // Drop such packets whose version don't match.
  return false;
}

bool QuicTimeWaitListManager::OnPacketHeader(const QuicPacketHeader& header) {
  // TODO(satyamshekhar): Think about handling packets from different client
  // addresses.
  GuidMapIterator it = guid_map_.find(header.public_header.guid);
  DCHECK(it != guid_map_.end());
  // Increment the received packet count.
  ++(it->second);
  if (ShouldSendPublicReset(it->second)) {
    // We don't need the packet anymore. Just tell the client what sequence
    // number we rejected.
    SendPublicReset(server_address_,
                    client_address_,
                    header.public_header.guid,
                    header.packet_sequence_number);
  }
  // Never process the body of the packet in time wait state.
  return false;
}

// Returns true if the number of packets received for this guid is a power of 2
// to throttle the number of public reset packets we send to a client.
bool QuicTimeWaitListManager::ShouldSendPublicReset(int received_packet_count) {
  return (received_packet_count & (received_packet_count - 1)) == 0;
}

void QuicTimeWaitListManager::SendPublicReset(
    const IPEndPoint& server_address,
    const IPEndPoint& client_address,
    QuicGuid guid,
    QuicPacketSequenceNumber rejected_sequence_number) {
  QuicPublicResetPacket packet;
  packet.public_header.guid = guid;
  packet.public_header.reset_flag = true;
  packet.public_header.version_flag = false;
  packet.rejected_sequence_number = rejected_sequence_number;
  // TODO(satyamshekhar): generate a valid nonce for this guid.
  packet.nonce_proof = 1010101;
  QueuedPacket* queued_packet = new QueuedPacket(
      server_address,
      client_address,
      framer_.ConstructPublicResetPacket(packet));
  // Takes ownership of the packet.
  SendOrQueuePacket(queued_packet);
}

// Either sends the packet and deletes it or makes pending queue the
// owner of the packet.
void QuicTimeWaitListManager::SendOrQueuePacket(QueuedPacket* packet) {
  if (!is_write_blocked_) {
    // TODO(satyamshekhar): Handle packets that fail due to error other than
    // EAGAIN or EWOULDBLOCK.
    WriteToWire(packet);
  }

  if (is_write_blocked_) {
    // pending_packets_queue takes the ownership of the queued packet.
    pending_packets_queue_.push_back(packet);
  } else {
    delete packet;
  }
}

void QuicTimeWaitListManager::WriteToWire(QueuedPacket* queued_packet) {
  DCHECK(!is_write_blocked_);
  int error;
  int rc = writer_->WritePacket(queued_packet->packet()->data(),
                                queued_packet->packet()->length(),
                                queued_packet->server_address().address(),
                                queued_packet->client_address(),
                                this,
                                &error);

  if (rc == -1) {
    if (error == EAGAIN || error == EWOULDBLOCK) {
      is_write_blocked_ = true;
    } else {
      LOG(WARNING) << "Received unknown error while sending reset packet:"
                   << strerror(error);
    }
  }
}

void QuicTimeWaitListManager::SetGuidCleanUpAlarm() {
  guid_clean_up_alarm_->UnregisterIfRegistered();
  int64 next_alarm_interval;
  if (!time_ordered_guid_list_.empty()) {
    GuidAddTime* oldest_guid = time_ordered_guid_list_.front();
    QuicTime now = clock_.ApproximateNow();
    DCHECK(now.Subtract(oldest_guid->time_added) < kTimeWaitPeriod_);
    next_alarm_interval = oldest_guid->time_added
        .Add(kTimeWaitPeriod_)
        .Subtract(now)
        .ToMicroseconds();
  } else {
    // No guids added so none will expire before kTimeWaitPeriod_.
    next_alarm_interval = kTimeWaitPeriod_.ToMicroseconds();
  }

  epoll_server_->RegisterAlarmApproximateDelta(next_alarm_interval,
                                               guid_clean_up_alarm_.get());
}

void QuicTimeWaitListManager::CleanUpOldGuids() {
  QuicTime now = clock_.ApproximateNow();
  while (time_ordered_guid_list_.size() > 0) {
    DCHECK_EQ(time_ordered_guid_list_.size(), guid_map_.size());
    GuidAddTime* oldest_guid = time_ordered_guid_list_.front();
    if (now.Subtract(oldest_guid->time_added) < kTimeWaitPeriod_) {
      break;
    }
    // This guid has lived its age, retire it now.
    guid_map_.erase(oldest_guid->guid);
    time_ordered_guid_list_.pop_front();
    delete oldest_guid;
  }
  SetGuidCleanUpAlarm();
}

}  // namespace net
