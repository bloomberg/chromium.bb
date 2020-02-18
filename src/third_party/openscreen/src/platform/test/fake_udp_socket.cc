// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/fake_udp_socket.h"

namespace openscreen {
namespace platform {

FakeUdpSocket::FakeUdpSocket(TaskRunner* task_runner,
                             Client* client,
                             Version version)
    : client_(client), version_(version) {}

// Ensure that the destructors for the unique_ptr objects are called in
// the correct order to avoid OSP_CHECK failures.
FakeUdpSocket::~FakeUdpSocket() {
  fake_task_runner_.reset();
  fake_clock_.reset();
  fake_client_.reset();
}

bool FakeUdpSocket::IsIPv4() const {
  return version_ == UdpSocket::Version::kV4;
}

bool FakeUdpSocket::IsIPv6() const {
  return version_ == UdpSocket::Version::kV6;
}

IPEndpoint FakeUdpSocket::GetLocalEndpoint() const {
  return IPEndpoint{};
}

void FakeUdpSocket::Bind() {
  OSP_CHECK(bind_errors_.size()) << "No Bind responses queued.";
  ProcessConfigurationMethod(&bind_errors_);
}

void FakeUdpSocket::SetMulticastOutboundInterface(
    NetworkInterfaceIndex interface) {
  OSP_CHECK(set_multicast_outbound_interface_errors_.size())
      << "No SetMulticastOutboundInterface responses queued.";
  ProcessConfigurationMethod(&set_multicast_outbound_interface_errors_);
}

void FakeUdpSocket::JoinMulticastGroup(const IPAddress& address,
                                       NetworkInterfaceIndex interface) {
  OSP_CHECK(join_multicast_group_errors_.size())
      << "No JoinMulticastGroup responses queued.";
  ProcessConfigurationMethod(&join_multicast_group_errors_);
}

void FakeUdpSocket::SetDscp(DscpMode mode) {
  OSP_CHECK(set_dscp_errors_.size()) << "No SetDscp responses queued.";
  ProcessConfigurationMethod(&set_dscp_errors_);
}

void FakeUdpSocket::ProcessConfigurationMethod(std::queue<Error>* errors) {
  Error error = errors->front();
  errors->pop();

  if (!error.ok()) {
    client_->OnError(this, std::move(error));
  }
}

void FakeUdpSocket::SendMessage(const void* data,
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
std::unique_ptr<FakeUdpSocket> FakeUdpSocket::CreateDefault(
    UdpSocket::Version version) {
  std::unique_ptr<FakeClock> clock = std::make_unique<FakeClock>(Clock::now());
  // TODO: Revisit this, since a FakeTaskRunner is being created here, but no
  // part of FakeUdpSocket makes use of a TaskRunner?
  std::unique_ptr<FakeTaskRunner> task_runner =
      std::make_unique<FakeTaskRunner>(clock.get());
  std::unique_ptr<FakeUdpSocket::MockClient> client =
      std::make_unique<FakeUdpSocket::MockClient>();

  std::unique_ptr<FakeUdpSocket> socket =
      std::make_unique<FakeUdpSocket>(task_runner.get(), client.get(), version);
  socket->fake_clock_.swap(clock);
  socket->fake_client_.swap(client);
  socket->fake_task_runner_.swap(task_runner);

  return socket;
}

}  // namespace platform
}  // namespace openscreen
