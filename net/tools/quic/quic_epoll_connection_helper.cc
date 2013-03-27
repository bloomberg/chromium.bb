// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_connection_helper.h"

#include <errno.h>
#include <sys/socket.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/quic_random.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/quic/quic_socket_utils.h"

namespace net {

// This alarm will be scheduled any time a data-bearing packet is sent out.
// When the alarm goes off, the connection checks to see if the oldest packets
// have been acked, and retransmit them if they have not.
class RetransmissionAlarm : public EpollAlarm {
 public:
  explicit RetransmissionAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual int64 OnAlarm() {
    EpollAlarm::OnAlarm();
    // This is safe because this code is Google3 specific, and the
    // Google3 QuicTime's epoch is the unix epoch.
    return connection_->OnRetransmissionTimeout().Subtract(
        QuicTime::Zero()).ToMicroseconds();
  }

 private:
  QuicConnection* connection_;
};

// An alarm that is scheduled when the sent scheduler requires a
// a delay before sending packets and fires when the packet may be sent.
class SendAlarm : public EpollAlarm {
 public:
  explicit SendAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual int64 OnAlarm() {
    EpollAlarm::OnAlarm();
    connection_->OnCanWrite();
    // Never reschedule the alarm, since OnCanWrite does that.
    return 0;
  }

 private:
  QuicConnection* connection_;
};

// An alarm which fires when the connection may have timed out.
class TimeoutAlarm : public EpollAlarm {
 public:
  explicit TimeoutAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual int64 OnAlarm() {
    EpollAlarm::OnAlarm();
    connection_->CheckForTimeout();
    // Never reschedule the alarm, since CheckForTimeout does that.
    return 0;
  }

 private:
  QuicConnection* connection_;
};

// An alarm that is scheduled to send an ack if a timeout occurs.
class AckAlarm : public EpollAlarm {
 public:
  explicit AckAlarm(QuicConnection* connection)
      : connection_(connection) {
  }

  virtual int64 OnAlarm() {
    EpollAlarm::OnAlarm();
    connection_->SendAck();
    return 0;
  }

 private:
  QuicConnection* connection_;
};

QuicEpollConnectionHelper::QuicEpollConnectionHelper(
  int fd, EpollServer* epoll_server)
    : writer_(NULL),
      epoll_server_(epoll_server),
      fd_(fd),
      connection_(NULL),
      clock_(epoll_server),
      random_generator_(QuicRandom::GetInstance()) {
}

QuicEpollConnectionHelper::QuicEpollConnectionHelper(QuicPacketWriter* writer,
                                                     EpollServer* epoll_server)
    : writer_(writer),
      epoll_server_(epoll_server),
      fd_(-1),
      connection_(NULL),
      clock_(epoll_server),
      random_generator_(QuicRandom::GetInstance()) {
}

QuicEpollConnectionHelper::~QuicEpollConnectionHelper() {
}

void QuicEpollConnectionHelper::SetConnection(QuicConnection* connection) {
  DCHECK(!connection_);
  connection_ = connection;
  timeout_alarm_.reset(new TimeoutAlarm(connection));
  send_alarm_.reset(new SendAlarm(connection));
  ack_alarm_.reset(new AckAlarm(connection));
  retransmission_alarm_.reset(new RetransmissionAlarm(connection_));
}

const QuicClock* QuicEpollConnectionHelper::GetClock() const {
  return &clock_;
}

QuicRandom* QuicEpollConnectionHelper::GetRandomGenerator() {
  return random_generator_;
}

int QuicEpollConnectionHelper::WritePacketToWire(
    const QuicEncryptedPacket& packet,
    int* error) {
  if (connection_->ShouldSimulateLostPacket()) {
    DLOG(INFO) << "Dropping packet due to fake packet loss.";
    *error = 0;
    return packet.length();
  }

  // If we have a writer, delgate the write to it.
  if (writer_) {
    return writer_->WritePacket(packet.data(), packet.length(),
                                connection_->self_address().address(),
                                connection_->peer_address(),
                                connection_,
                                error);
  } else {
    return QuicSocketUtils::WritePacket(
        fd_, packet.data(), packet.length(),
        connection_->self_address().address(),
        connection_->peer_address(),
        error);
  }
}

bool QuicEpollConnectionHelper::IsWriteBlockedDataBuffered() {
  return false;
}

bool QuicEpollConnectionHelper::IsWriteBlocked(int error) {
  return error == EAGAIN || error == EWOULDBLOCK;
}

void QuicEpollConnectionHelper::SetRetransmissionAlarm(QuicTime::Delta delay) {
  if (!retransmission_alarm_->registered()) {
    epoll_server_->RegisterAlarmApproximateDelta(delay.ToMicroseconds(),
                                                 retransmission_alarm_.get());
  }
}

void QuicEpollConnectionHelper::SetAckAlarm(QuicTime::Delta delay) {
  if (!ack_alarm_->registered()) {
    epoll_server_->RegisterAlarmApproximateDelta(
        delay.ToMicroseconds(), ack_alarm_.get());
  }
}

void QuicEpollConnectionHelper::ClearAckAlarm() {
  ack_alarm_->UnregisterIfRegistered();
}

void QuicEpollConnectionHelper::SetSendAlarm(QuicTime alarm_time) {
  send_alarm_->UnregisterIfRegistered();
  epoll_server_->RegisterAlarm(
      alarm_time.Subtract(QuicTime::Zero()).ToMicroseconds(),
      send_alarm_.get());
}

void QuicEpollConnectionHelper::SetTimeoutAlarm(QuicTime::Delta delay) {
  timeout_alarm_->UnregisterIfRegistered();
  epoll_server_->RegisterAlarmApproximateDelta(delay.ToMicroseconds(),
                                               timeout_alarm_.get());
}

bool QuicEpollConnectionHelper::IsSendAlarmSet() {
  return send_alarm_->registered();
}

void QuicEpollConnectionHelper::UnregisterSendAlarmIfRegistered() {
  send_alarm_->UnregisterIfRegistered();
}

}  // namespace net
