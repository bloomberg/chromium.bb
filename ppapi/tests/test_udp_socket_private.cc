// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_udp_socket_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UDPSocketPrivate);

namespace {

const uint16_t kPortScanFrom = 1024;
const uint16_t kPortScanTo = 4096;

}  // namespace

TestUDPSocketPrivate::TestUDPSocketPrivate(
    TestingInstance* instance)
    : TestCase(instance) {
}

bool TestUDPSocketPrivate::Init() {
  bool tcp_socket_private_is_available = pp::TCPSocketPrivate::IsAvailable();
  if (!tcp_socket_private_is_available)
    instance_->AppendError("PPB_TCPSocket_Private interface not available");

  bool udp_socket_private_is_available = pp::UDPSocketPrivate::IsAvailable();
  if (!udp_socket_private_is_available)
    instance_->AppendError("PPB_UDPSocket_Private interface not available");

  bool net_address_private_is_available = pp::NetAddressPrivate::IsAvailable();
  if (!net_address_private_is_available)
    instance_->AppendError("PPB_NetAddress_Private interface not available");

  bool init_host_port = GetLocalHostPort(instance_->pp_instance(),
                                         &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return tcp_socket_private_is_available &&
      udp_socket_private_is_available &&
      net_address_private_is_available &&
      init_host_port &&
      CheckTestingInterface() &&
      EnsureRunningOverHTTP();
}

void TestUDPSocketPrivate::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Connect, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ConnectFailure, filter);
}

std::string TestUDPSocketPrivate::GetLocalAddress(
    PP_NetAddress_Private* address) {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket.Connect(host_.c_str(), port_, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Connect force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::Connect", rv);
  if (!socket.GetLocalAddress(address))
    return "PPB_TCPSocket_Private::GetLocalAddress: Failed";
  socket.Disconnect();
  PASS();
}

std::string TestUDPSocketPrivate::BindUDPSocket(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private *address) {
  PP_NetAddress_Private base_address;
  ASSERT_SUBTEST_SUCCESS(GetLocalAddress(&base_address));

  bool is_free_port_found = false;
  for (uint16_t port = kPortScanFrom; port < kPortScanTo; ++port) {
    if (!pp::NetAddressPrivate::ReplacePort(base_address, port, address))
      return "PPB_NetAddress_Private::ReplacePort: Failed";
    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = socket->Bind(address, callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_UDPSocket_Private::Bind force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv == PP_OK) {
      is_free_port_found = true;
      break;
    }
  }
  if (!is_free_port_found)
    return "Can't find available port";
  if (!socket->GetBoundAddress(address))
    return "PPB_UDPSocket_Private::GetBoundAddress: Failed";
  PASS();
}

std::string TestUDPSocketPrivate::BindUDPSocketFailure(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private *address) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket->Bind(address, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::Bind force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv == PP_OK)
    return ReportError("PPB_UDPSocket_Private::Bind", rv);
  if (socket->GetBoundAddress(address))
      return "PPB_UDPSocket_Private::GetBoundAddress: Failed";
  PASS();
}

std::string TestUDPSocketPrivate::TestConnect() {
  pp::UDPSocketPrivate server_socket(instance_), client_socket(instance_);
  PP_NetAddress_Private server_address, client_address;

  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server_socket, &server_address));
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&client_socket, &client_address));

  static const char* const kMessage =
      "Simple message that will be sent via UDP";
  static const size_t kMessageBufferSize = 1024;
  char message_buffer[kMessageBufferSize];

  TestCompletionCallback write_callback(instance_->pp_instance(), force_async_);
  int32_t write_rv = client_socket.SendTo(kMessage, strlen(kMessage),
                                          &server_address,
                                          write_callback);
  if (force_async_ && write_rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::SendTo force_async", write_rv);

  TestCompletionCallback read_callback(instance_->pp_instance(), force_async_);
  int32_t read_rv = server_socket.RecvFrom(message_buffer, strlen(kMessage),
                                           read_callback);
  if (force_async_ && read_rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::RecvFrom force_async", read_rv);

  if (read_rv == PP_OK_COMPLETIONPENDING)
    read_rv = read_callback.WaitForResult();
  if (read_rv < 0 || strlen(kMessage) != static_cast<size_t>(read_rv))
    return ReportError("PPB_UDPSocket_Private::RecvFrom", read_rv);

  if (write_rv == PP_OK_COMPLETIONPENDING)
    write_rv = write_callback.WaitForResult();
  if (write_rv < 0 || strlen(kMessage) != static_cast<size_t>(write_rv))
    return ReportError("PPB_UDPSocket_Private::SendTo", write_rv);

  PP_NetAddress_Private recv_from_address;
  ASSERT_TRUE(server_socket.GetRecvFromAddress(&recv_from_address));
  ASSERT_TRUE(pp::NetAddressPrivate::AreEqual(recv_from_address,
                                              client_address));
  ASSERT_EQ(0, strncmp(kMessage, message_buffer, strlen(kMessage)));

  server_socket.Close();
  client_socket.Close();

  if (server_socket.GetBoundAddress(&server_address))
    return "PPB_UDPSocket_Private::GetBoundAddress: expected Failure";

  PASS();
}

std::string TestUDPSocketPrivate::TestConnectFailure() {
  pp::UDPSocketPrivate socket(instance_);
  PP_NetAddress_Private invalid_address = {};

  std::string error_message = BindUDPSocketFailure(&socket, &invalid_address);
  if (!error_message.empty())
    return error_message;

  PASS();
}
