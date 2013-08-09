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
namespace tools {

namespace {

class QuicEpollAlarm : public QuicAlarm {
 public:
  QuicEpollAlarm(EpollServer* epoll_server,
                 QuicAlarm::Delegate* delegate)
      : QuicAlarm(delegate),
        epoll_server_(epoll_server),
        epoll_alarm_impl_(this) {}

 protected:
  virtual void SetImpl() OVERRIDE {
    DCHECK(deadline().IsInitialized());
    epoll_server_->RegisterAlarm(
        deadline().Subtract(QuicTime::Zero()).ToMicroseconds(),
        &epoll_alarm_impl_);
  }

  virtual void CancelImpl() OVERRIDE {
    DCHECK(!deadline().IsInitialized());
    epoll_alarm_impl_.UnregisterIfRegistered();
  }

 private:
  class EpollAlarmImpl : public EpollAlarm {
   public:
    explicit EpollAlarmImpl(QuicEpollAlarm* alarm) : alarm_(alarm) {}

    virtual int64 OnAlarm() OVERRIDE {
      EpollAlarm::OnAlarm();
      alarm_->Fire();
      // Fire will take care of registering the alarm, if needed.
      return 0;
    }

   private:
    QuicEpollAlarm* alarm_;
  };

  EpollServer* epoll_server_;
  EpollAlarmImpl epoll_alarm_impl_;
};

}  // namespace

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

QuicAlarm* QuicEpollConnectionHelper::CreateAlarm(
    QuicAlarm::Delegate* delegate) {
  return new QuicEpollAlarm(epoll_server_, delegate);
}

}  // namespace tools
}  // namespace net
