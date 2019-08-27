// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

MockUdpSocket::MockUdpSocket(TaskRunner* task_runner,
                             Client* client,
                             Version version)
    : UdpSocket(task_runner, client), version_(version) {}

// Ensure that the destructors for the unique_ptr objects are called in
// the correct order to avoid OSP_CHECK failures.
MockUdpSocket::~MockUdpSocket() {
  CloseIfOpen();

  task_runner_.reset();
  clock_.reset();
  client_.reset();
}

bool MockUdpSocket::IsIPv4() const {
  return version_ == UdpSocket::Version::kV4;
}

bool MockUdpSocket::IsIPv6() const {
  return version_ == UdpSocket::Version::kV6;
}

IPEndpoint MockUdpSocket::GetLocalEndpoint() const {
  return IPEndpoint{};
}

void MockUdpSocket::Bind() {
  OSP_CHECK(bind_errors_.size()) << "No Bind responses queued.";
  ProcessConfigurationMethod(&bind_errors_);
}

void MockUdpSocket::SetMulticastOutboundInterface(
    NetworkInterfaceIndex interface) {
  OSP_CHECK(set_multicast_outbound_interface_errors_.size())
      << "No SetMulticastOutboundInterface responses queued.";
  ProcessConfigurationMethod(&set_multicast_outbound_interface_errors_);
}

void MockUdpSocket::JoinMulticastGroup(const IPAddress& address,
                                       NetworkInterfaceIndex interface) {
  OSP_CHECK(join_multicast_group_errors_.size())
      << "No JoinMulticastGroup responses queued.";
  ProcessConfigurationMethod(&join_multicast_group_errors_);
}

void MockUdpSocket::SetDscp(DscpMode mode) {
  OSP_CHECK(set_dscp_errors_.size()) << "No SetDscp responses queued.";
  ProcessConfigurationMethod(&set_dscp_errors_);
}

void MockUdpSocket::ProcessConfigurationMethod(std::queue<Error>* errors) {
  Error error = errors->front();
  errors->pop();

  if (!error.ok()) {
    client_->OnError(this, std::move(error));
  }
}

void MockUdpSocket::SendMessage(const void* data,
                                size_t length,
                                const IPEndpoint& dest) {
  OSP_CHECK(send_errors_.size()) << "No SendMessage responses queued.";
  Error error = send_errors_.front();
  send_errors_.pop();

  if (!error.ok()) {
    client_->OnSendError(this, std::move(error));
  }
}

// static
std::unique_ptr<MockUdpSocket> MockUdpSocket::CreateDefault(
    UdpSocket::Version version) {
  std::unique_ptr<FakeClock> clock = std::make_unique<FakeClock>(Clock::now());
  std::unique_ptr<FakeTaskRunner> task_runner =
      std::make_unique<FakeTaskRunner>(clock.get());
  std::unique_ptr<MockUdpSocket::MockClient> client =
      std::make_unique<MockUdpSocket::MockClient>();

  std::unique_ptr<MockUdpSocket> socket =
      std::make_unique<MockUdpSocket>(task_runner.get(), client.get(), version);
  socket->clock_.swap(clock);
  socket->client_.swap(client);
  socket->task_runner_.swap(task_runner);

  return socket;
}

}  // namespace platform
}  // namespace openscreen
