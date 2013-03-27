// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Google-specific helper for QuicConnection which uses
// EpollAlarm for alarms, and used an int fd_ for writing data.

#ifndef NET_TOOLS_QUIC_QUIC_EPOLL_CONNECTION_HELPER_H_
#define NET_TOOLS_QUIC_QUIC_EPOLL_CONNECTION_HELPER_H_

#include <sys/types.h>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/tools/quic/quic_epoll_clock.h"
#include "net/tools/quic/quic_packet_writer.h"

namespace net {

class AckAlarm;
class EpollServer;
class QuicRandom;
class RetransmissionAlarm;
class SendAlarm;
class TimeoutAlarm;

class QuicEpollConnectionHelper : public QuicConnectionHelperInterface {
 public:
  QuicEpollConnectionHelper(int fd, EpollServer* eps);
  QuicEpollConnectionHelper(QuicPacketWriter* writer, EpollServer* eps);
  virtual ~QuicEpollConnectionHelper();

  // QuicEpollConnectionHelperInterface
  virtual void SetConnection(QuicConnection* connection) OVERRIDE;
  virtual const QuicClock* GetClock() const OVERRIDE;
  virtual QuicRandom* GetRandomGenerator() OVERRIDE;
  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() OVERRIDE;
  virtual bool IsWriteBlocked(int error) OVERRIDE;
  virtual void SetRetransmissionAlarm(QuicTime::Delta delay) OVERRIDE;
  virtual void SetSendAlarm(QuicTime alarm_time) OVERRIDE;
  virtual void SetTimeoutAlarm(QuicTime::Delta delay) OVERRIDE;
  virtual bool IsSendAlarmSet() OVERRIDE;
  virtual void UnregisterSendAlarmIfRegistered() OVERRIDE;
  virtual void SetAckAlarm(QuicTime::Delta delay) OVERRIDE;
  virtual void ClearAckAlarm() OVERRIDE;

  EpollServer* epoll_server() { return epoll_server_; }

 private:
  friend class QuicConnectionPeer;

  QuicPacketWriter* writer_;  // Not owned
  EpollServer* epoll_server_;  // Not owned.
  int fd_;

  // An alarm which fires if we've hit a timeout on sending an ack.
  scoped_ptr<AckAlarm> ack_alarm_;

  scoped_ptr<RetransmissionAlarm> retransmission_alarm_;

  // An alarm which fires when the connection may have timed out.
  scoped_ptr<TimeoutAlarm> timeout_alarm_;

  // An alarm that is scheduled when the sent scheduler requires a
  // a delay before sending packets and fires when the packet may be sent.
  scoped_ptr<SendAlarm> send_alarm_;

  QuicConnection* connection_;
  const QuicEpollClock clock_;
  QuicRandom* random_generator_;

  DISALLOW_COPY_AND_ASSIGN(QuicEpollConnectionHelper);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_EPOLL_CONNECTION_HELPER_H_
