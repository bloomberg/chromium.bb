// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_time_wait_list_manager.h"

#include <errno.h>

#include "base/containers/hash_tables.h"
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
#include "net/tools/quic/quic_server_session.h"

using base::StringPiece;
using std::make_pair;

namespace net {
namespace tools {

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

  virtual int64 OnAlarm() OVERRIDE {
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
    QuicServerSessionVisitor* visitor,
    EpollServer* epoll_server,
    const QuicVersionVector& supported_versions)
    : epoll_server_(epoll_server),
      kTimeWaitPeriod_(QuicTime::Delta::FromSeconds(kTimeWaitSeconds)),
      guid_clean_up_alarm_(new GuidCleanUpAlarm(this)),
      clock_(epoll_server_),
      writer_(writer),
      visitor_(visitor) {
  SetGuidCleanUpAlarm();
}

QuicTimeWaitListManager::~QuicTimeWaitListManager() {
  guid_clean_up_alarm_->UnregisterIfRegistered();
  STLDeleteElements(&time_ordered_guid_list_);
  STLDeleteElements(&pending_packets_queue_);
  for (GuidMapIterator it = guid_map_.begin(); it != guid_map_.end(); ++it) {
    delete it->second.close_packet;
  }
}

void QuicTimeWaitListManager::AddGuidToTimeWait(
    QuicGuid guid,
    QuicVersion version,
    QuicEncryptedPacket* close_packet) {
  DCHECK(!IsGuidInTimeWait(guid));
  // Initialize the guid with 0 packets received.
  GuidData data(0, version, close_packet);
  guid_map_.insert(make_pair(guid, data));
  time_ordered_guid_list_.push_back(new GuidAddTime(guid,
                                                    clock_.ApproximateNow()));
  DCHECK(IsGuidInTimeWait(guid));
}

bool QuicTimeWaitListManager::IsGuidInTimeWait(QuicGuid guid) const {
  return guid_map_.find(guid) != guid_map_.end();
}

QuicVersion QuicTimeWaitListManager::GetQuicVersionFromGuid(QuicGuid guid) {
  GuidMapIterator it = guid_map_.find(guid);
  DCHECK(it != guid_map_.end());
  return (it->second).version;
}

bool QuicTimeWaitListManager::OnCanWrite() {
  while (!pending_packets_queue_.empty()) {
    QueuedPacket* queued_packet = pending_packets_queue_.front();
    if (WriteToWire(queued_packet)) {
      pending_packets_queue_.pop_front();
      delete queued_packet;
    } else {
      break;
    }
  }

  return !writer_->IsWriteBlocked();
}

void QuicTimeWaitListManager::ProcessPacket(
    const IPEndPoint& server_address,
    const IPEndPoint& client_address,
    QuicGuid guid,
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(IsGuidInTimeWait(guid));
  // TODO(satyamshekhar): Think about handling packets from different client
  // addresses.
  GuidMapIterator it = guid_map_.find(guid);
  DCHECK(it != guid_map_.end());
  // Increment the received packet count.
  ++((it->second).num_packets);
  if (!ShouldSendResponse((it->second).num_packets)) {
    return;
  }
  if (it->second.close_packet) {
     QueuedPacket* queued_packet =
         new QueuedPacket(server_address,
                          client_address,
                          it->second.close_packet->Clone());
     // Takes ownership of the packet.
     SendOrQueuePacket(queued_packet);
  } else {
    SendPublicReset(server_address, client_address, guid, sequence_number);
  }
}

// Returns true if the number of packets received for this guid is a power of 2
// to throttle the number of public reset packets we send to a client.
bool QuicTimeWaitListManager::ShouldSendResponse(int received_packet_count) {
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
      QuicFramer::BuildPublicResetPacket(packet));
  // Takes ownership of the packet.
  SendOrQueuePacket(queued_packet);
}

// Either sends the packet and deletes it or makes pending queue the
// owner of the packet.
void QuicTimeWaitListManager::SendOrQueuePacket(QueuedPacket* packet) {
  if (WriteToWire(packet)) {
    delete packet;
  } else {
    // pending_packets_queue takes the ownership of the queued packet.
    pending_packets_queue_.push_back(packet);
  }
}

bool QuicTimeWaitListManager::WriteToWire(QueuedPacket* queued_packet) {
  if (writer_->IsWriteBlocked()) {
    visitor_->OnWriteBlocked(this);
    return false;
  }
  WriteResult result = writer_->WritePacket(
      queued_packet->packet()->data(),
      queued_packet->packet()->length(),
      queued_packet->server_address().address(),
      queued_packet->client_address(),
      this);
  if (result.status == WRITE_STATUS_BLOCKED) {
    // If blocked and unbuffered, return false to retry sending.
    DCHECK(writer_->IsWriteBlocked());
    visitor_->OnWriteBlocked(this);
    return writer_->IsWriteBlockedDataBuffered();
  } else if (result.status == WRITE_STATUS_ERROR) {
    LOG(WARNING) << "Received unknown error while sending reset packet to "
                 << queued_packet->client_address().ToString() << ": "
                 << strerror(result.error_code);
  }
  return true;
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
    GuidMapIterator it = guid_map_.find(oldest_guid->guid);
    DCHECK(it != guid_map_.end());
    delete it->second.close_packet;
    guid_map_.erase(oldest_guid->guid);
    time_ordered_guid_list_.pop_front();
    delete oldest_guid;
  }
  SetGuidCleanUpAlarm();
}

}  // namespace tools
}  // namespace net
