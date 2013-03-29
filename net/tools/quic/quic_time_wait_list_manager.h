// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Handles packets for guids in time wait state by discarding the packet and
// sending the clients a public reset packet with exponential backoff.

#ifndef NET_TOOLS_QUIC_QUIC_TIME_WAIT_LIST_MANAGER_H_
#define NET_TOOLS_QUIC_QUIC_TIME_WAIT_LIST_MANAGER_H_

#include <deque>

#include "base/hash_tables.h"
#include "base/string_piece.h"
#include "net/quic/quic_blocked_writer_interface.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/quic/quic_epoll_clock.h"
#include "net/tools/quic/quic_packet_writer.h"

namespace net {

class GuidCleanUpAlarm;

// Maintains a list of all guids that have been recently closed. A guid lives in
// this state for kTimeWaitPeriod. All packets received for guids in this state
// are handed over to the QuicTimeWaitListManager by the QuicDispatcher. It also
// decides whether we should send a public reset packet to the client which sent
// a packet with the guid in time wait state and sends it when appropriate.
// After the guid expires its time wait period, a new connection/session will be
// created if a packet is received for this guid.
class QuicTimeWaitListManager : public QuicBlockedWriterInterface,
                                public QuicFramerVisitorInterface {
 public:
  // writer - the entity that writes to the socket. (Owned by the dispatcher)
  // epoll_server - used to run clean up alarms. (Owned by the dispatcher)
  QuicTimeWaitListManager(QuicPacketWriter* writer,
                          EpollServer* epoll_server);
  virtual ~QuicTimeWaitListManager();

  // Adds the given guid to time wait state for kTimeWaitPeriod. Henceforth,
  // any packet bearing this guid should not be processed while the guid remains
  // in this list. Public reset packets are sent to the clients by the time wait
  // list manager that send packets to guids in this state. DCHECKs that guid is
  // not already on the list.
  void AddGuidToTimeWait(QuicGuid guid);

  // Returns true if the guid is in time wait state, false otherwise. Packets
  // received for this guid should not lead to creation of new QuicSessions.
  bool IsGuidInTimeWait(QuicGuid guid) const;

  // Called when a packet is received for a guid that is in time wait state.
  // Sends a public reset packet to the client which sent this guid. Sending
  // of the public reset packet is throttled by using exponential back off.
  // DCHECKs for the guid to be in time wait state.
  // virtual to override in tests.
  virtual void ProcessPacket(const IPEndPoint& server_address,
                             const IPEndPoint& client_address,
                             QuicGuid guid,
                             const QuicEncryptedPacket& packet);

  // Called by the dispatcher when the underlying socket becomes writable again,
  // since we might need to send pending public reset packets which we didn't
  // send because the underlying socket was write blocked.
  virtual bool OnCanWrite() OVERRIDE;

  // Used to delete guid entries that have outlived their time wait period.
  void CleanUpOldGuids();

  // FramerVisitorInterface
  virtual void OnError(QuicFramer* framer) OVERRIDE;
  virtual bool OnProtocolVersionMismatch(
      QuicVersionTag received_version) OVERRIDE;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnPacket() OVERRIDE {}
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE {}
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& /*packet*/) OVERRIDE {}

  virtual void OnPacketComplete() OVERRIDE {}
  // The following methods should never get called because we always return
  // false from OnPacketHeader(). We never need to process body of a packet.
  virtual void OnRevivedPacket() OVERRIDE {}
  virtual void OnFecProtectedPayload(base::StringPiece payload) OVERRIDE {}
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {}
  virtual void OnAckFrame(const QuicAckFrame& frame) OVERRIDE {}
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE {}
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE {}
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame & frame) OVERRIDE {}
  virtual void OnGoAwayFrame(
      const QuicGoAwayFrame& frame) OVERRIDE {}
  virtual void OnFecData(const QuicFecData& fec) OVERRIDE {}

 protected:
  // Exposed for tests.
  bool is_write_blocked() const { return is_write_blocked_; }

  // Decides if public reset packet should be sent for this guid based on the
  // number of received pacekts.
  bool ShouldSendPublicReset(int received_packet_count);

  // Exposed for tests.
  const QuicTime::Delta time_wait_period() const { return kTimeWaitPeriod_; }

 private:
  // Stores the guid and the time it was added to time wait state.
  struct GuidAddTime;
  // Internal structure to store pending public reset packets.
  class QueuedPacket;

  // Creates a public reset packet and sends it or queues it to be sent later.
  void SendPublicReset(const IPEndPoint& server_address,
                       const IPEndPoint& client_address,
                       QuicGuid guid,
                       QuicPacketSequenceNumber rejected_sequence_number);

  // Either sends the packet and deletes it or makes pending_packets_queue_ the
  // owner of the packet.
  void SendOrQueuePacket(QueuedPacket* packet);

  // Should only be called when write_blocked_ == false. We only care if the
  // writing was unsuccessful because the socket got blocked, which can be
  // tested using write_blocked_ == true. In case of all other errors we drop
  // the packet. Hence, we return void.
  void WriteToWire(QueuedPacket* packet);

  // Register the alarm with the epoll server to wake up at appropriate time.
  void SetGuidCleanUpAlarm();

  // A map from a recently closed guid to the number of packets received after
  // the termination of the connection bound to the guid.
  base::hash_map<QuicGuid, int> guid_map_;
  typedef base::hash_map<QuicGuid, int>::iterator GuidMapIterator;

  // Maintains a list of GuidAddTime elements which it owns, in the
  // order they should be deleted.
  std::deque<GuidAddTime*> time_ordered_guid_list_;

  // Pending public reset packets that need to be sent out to the client
  // when we are given a chance to write by the dispatcher.
  std::deque<QueuedPacket*> pending_packets_queue_;

  // Used to parse incoming packets.
  QuicFramer framer_;

  // Server and client address of the last packet processed.
  IPEndPoint server_address_;
  IPEndPoint client_address_;

  // Used to schedule alarms to delete old guids which have been in the list for
  // too long. Owned by the dispatcher.
  EpollServer* epoll_server_;

  // Time period for which guids should remain in time wait state.
  const QuicTime::Delta kTimeWaitPeriod_;

  // Alarm registered with the epoll server to clean up guids that have out
  // lived their duration in time wait state.
  scoped_ptr<GuidCleanUpAlarm> guid_clean_up_alarm_;

  // Clock to efficiently measure approximate time from the epoll server.
  QuicEpollClock clock_;

  // Interface that writes given buffer to the socket. Owned by the dispatcher.
  QuicPacketWriter* writer_;

  // True if the underlying udp socket is write blocked, i.e will return EAGAIN
  // on sendmsg.
  bool is_write_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicTimeWaitListManager);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_TIME_WAIT_LIST_MANAGER_H_
