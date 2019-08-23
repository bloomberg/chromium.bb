// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_TEST_MOCK_UDP_SOCKET_H_
#define PLATFORM_TEST_MOCK_UDP_SOCKET_H_

#include <algorithm>
#include <memory>
#include <queue>

#include "gmock/gmock.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace platform {

class MockUdpSocket : public UdpSocket {
 public:
  class MockClient : public UdpSocket::Client {
   public:
    MOCK_METHOD2(OnError, void(UdpSocket*, Error));
    MOCK_METHOD2(OnSendError, void(UdpSocket*, Error));
    MOCK_METHOD2(OnReadInternal, void(UdpSocket*, const ErrorOr<UdpPacket>&));

    void OnRead(UdpSocket* socket, ErrorOr<UdpPacket> packet) override {
      OnReadInternal(socket, packet);
    }
  };

  static std::unique_ptr<MockUdpSocket> CreateDefault(
      Version version = Version::kV4);

  MockUdpSocket(TaskRunner* task_runner,
                Client* client,
                Version version = Version::kV4);
  ~MockUdpSocket() override;

  bool IsIPv4() const override;
  bool IsIPv6() const override;
  IPEndpoint GetLocalEndpoint() const override;

  void EnqueueBindResult(Error error) { bind_errors_.push(error); }
  void EnqueueSendResult(Error error) { send_errors_.push(error); }

  // UdpSocket overrides
  void Bind() override;
  void SendMessage(const void* data,
                   size_t length,
                   const IPEndpoint& dest) override;

  MOCK_METHOD1(SetMulticastOutboundInterface, Error(NetworkInterfaceIndex));
  MOCK_METHOD2(JoinMulticastGroup,
               Error(const IPAddress&, NetworkInterfaceIndex));
  MOCK_METHOD1(SetDscp, Error(DscpMode));

  size_t bind_queue_size() { return bind_errors_.size(); }
  size_t send_queue_size() { return send_errors_.size(); }

  MockUdpSocket::MockClient* client() { return client_.get(); }

 private:
  Version version_;

  // Queues for the response to calls above
  std::queue<Error> bind_errors_;
  std::queue<Error> send_errors_;

  // Fake implementations to be set by CreateDefault().
  std::unique_ptr<FakeTaskRunner> task_runner_;
  std::unique_ptr<MockUdpSocket::MockClient> client_;
  std::unique_ptr<FakeClock> clock_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_TEST_MOCK_UDP_SOCKET_H_
