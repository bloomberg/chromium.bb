// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_server_socket_private.h"

#include <vector>

#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/private/tcp_server_socket_private.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

using pp::NetAddressPrivate;
using pp::TCPServerSocketPrivate;
using pp::TCPSocketPrivate;

namespace {

const uint16_t kPortScanFrom = 1024;
const uint16_t kPortScanTo = 4096;

}  // namespace

REGISTER_TEST_CASE(TCPServerSocketPrivate);

TestTCPServerSocketPrivate::TestTCPServerSocketPrivate(
    TestingInstance* instance) : TestCase(instance) {
}

bool TestTCPServerSocketPrivate::Init() {
  bool tcp_server_socket_private_is_available =
      TCPServerSocketPrivate::IsAvailable();
  if (!tcp_server_socket_private_is_available) {
    instance_->AppendError(
        "PPB_TCPServerSocket_Private interface not available");
  }

  bool tcp_socket_private_is_available = TCPSocketPrivate::IsAvailable();
  if (!tcp_socket_private_is_available)
    instance_->AppendError("PPB_TCPSocket_Private interface not available");

  bool net_address_private_is_available = NetAddressPrivate::IsAvailable();
  if (!net_address_private_is_available)
    instance_->AppendError("PPB_NetAddress_Private interface not available");

  bool init_host_port = GetLocalHostPort(instance_->pp_instance(),
                                         &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return tcp_server_socket_private_is_available &&
      tcp_socket_private_is_available &&
      net_address_private_is_available &&
      init_host_port &&
      CheckTestingInterface() &&
      EnsureRunningOverHTTP();
}

void TestTCPServerSocketPrivate::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Listen, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Backlog, filter);
}

std::string TestTCPServerSocketPrivate::GetLocalAddress(
    PP_NetAddress_Private* address) {
  TCPSocketPrivate socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket.Connect(host_.c_str(), port_, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Connect force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::Connect", rv);
  if (!socket.GetLocalAddress(address))
    return ReportError("PPB_TCPSocket_Private::GetLocalAddress", 0);
  socket.Disconnect();
  PASS();
}

std::string TestTCPServerSocketPrivate::SyncRead(TCPSocketPrivate* socket,
                                                 char* buffer,
                                                 size_t num_bytes) {
  while (num_bytes > 0) {
    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = socket->Read(buffer, num_bytes, callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPSocket_Private::Read force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return ReportError("PPB_TCPSocket_Private::Read", rv);
    buffer += rv;
    num_bytes -= rv;
  }
  PASS();
}

std::string TestTCPServerSocketPrivate::SyncWrite(TCPSocketPrivate* socket,
                                                  const char* buffer,
                                                  size_t num_bytes) {
  while (num_bytes > 0) {
    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = socket->Write(buffer, num_bytes, callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPSocket_Private::Write force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return ReportError("PPB_TCPSocket_Private::Write", rv);
    buffer += rv;
    num_bytes -= rv;
  }
  PASS();
}

std::string TestTCPServerSocketPrivate::SyncConnect(
    TCPSocketPrivate* socket,
    PP_NetAddress_Private* address) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket->ConnectWithNetAddress(address, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Connect force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::Connect", rv);
  PASS();
}

void TestTCPServerSocketPrivate::ForceConnect(TCPSocketPrivate* socket,
                                              PP_NetAddress_Private* address) {
  std::string error_message;
  do {
    error_message = SyncConnect(socket, address);
  } while (!error_message.empty());
}

std::string TestTCPServerSocketPrivate::SyncListen(
    TCPServerSocketPrivate* socket,
    PP_NetAddress_Private* address,
    int32_t backlog) {
  PP_NetAddress_Private base_address;
  ASSERT_SUBTEST_SUCCESS(GetLocalAddress(&base_address));

  // TODO (ygorshenin): find more efficient way to select available
  // ports.
  bool is_free_port_found = false;
  for (uint16_t port = kPortScanFrom; port < kPortScanTo; ++port) {
    if (!NetAddressPrivate::ReplacePort(base_address, port, address))
      return ReportError("PPB_NetAddress_Private::ReplacePort", 0);

    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = socket->Listen(address, backlog, callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPServerSocket_Private::Listen force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv == PP_OK) {
      is_free_port_found = true;
      break;
    }
  }

  if (!is_free_port_found)
    return "Can't find available port";
  PASS();
}

std::string TestTCPServerSocketPrivate::TestListen() {
  static const int kBacklog = 2;

  TCPServerSocketPrivate server_socket(instance_);

  PP_NetAddress_Private address;
  ASSERT_SUBTEST_SUCCESS(SyncListen(&server_socket, &address, kBacklog));

  TestCompletionCallback accept_callback(instance_->pp_instance(),
                                         force_async_);
  PP_Resource resource;
  int32_t accept_rv = server_socket.Accept(&resource,
                                           accept_callback.GetCallback());

  TCPSocketPrivate client_socket(instance_);
  ForceConnect(&client_socket, &address);

  if (force_async_ && accept_rv != PP_OK_COMPLETIONPENDING) {
    return ReportError("PPB_TCPServerSocket_Private::Accept force_async",
                       accept_rv);
  }
  if (accept_rv == PP_OK_COMPLETIONPENDING)
    accept_rv = accept_callback.WaitForResult();
  if (accept_rv != PP_OK)
    return ReportError("PPB_TCPServerSocket_Private::Accept", accept_rv);

  ASSERT_TRUE(resource != 0);
  TCPSocketPrivate accepted_socket(pp::PassRef(), resource);

  const char kSentByte = 'a';
  ASSERT_SUBTEST_SUCCESS(SyncWrite(&client_socket,
                                   &kSentByte,
                                   sizeof(kSentByte)));

  char received_byte;
  ASSERT_SUBTEST_SUCCESS(SyncRead(&accepted_socket,
                                  &received_byte,
                                  sizeof(received_byte)));
  ASSERT_EQ(kSentByte, received_byte);

  accepted_socket.Disconnect();
  client_socket.Disconnect();
  server_socket.StopListening();

  PASS();
}

std::string TestTCPServerSocketPrivate::TestBacklog() {
  static const size_t kBacklog = 5;

  TCPServerSocketPrivate server_socket(instance_);

  PP_NetAddress_Private address;
  ASSERT_SUBTEST_SUCCESS(SyncListen(&server_socket, &address, 2 * kBacklog));

  std::vector<TCPSocketPrivate*> client_sockets(kBacklog);
  std::vector<TestCompletionCallback*> connect_callbacks(kBacklog);
  std::vector<int32_t> connect_rv(kBacklog);
  for (size_t i = 0; i < kBacklog; ++i) {
    client_sockets[i] = new TCPSocketPrivate(instance_);
    connect_callbacks[i] = new TestCompletionCallback(instance_->pp_instance(),
                                                      force_async_);
    connect_rv[i] = client_sockets[i]->ConnectWithNetAddress(
        &address,
        connect_callbacks[i]->GetCallback());
    if (force_async_ && connect_rv[i] != PP_OK_COMPLETIONPENDING) {
      return ReportError("PPB_TCPSocket_Private::Connect force_async",
                         connect_rv[i]);
    }
  }

  std::vector<PP_Resource> resources(kBacklog);
  std::vector<TCPSocketPrivate*> accepted_sockets(kBacklog);
  for (size_t i = 0; i < kBacklog; ++i) {
    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = server_socket.Accept(&resources[i], callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPServerSocket_Private::Accept force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("PPB_TCPServerSocket_Private::Accept", rv);

    ASSERT_TRUE(resources[i] != 0);
    accepted_sockets[i] = new TCPSocketPrivate(pp::PassRef(), resources[i]);
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    if (connect_rv[i] == PP_OK_COMPLETIONPENDING)
      connect_rv[i] = connect_callbacks[i]->WaitForResult();
    if (connect_rv[i] != PP_OK)
      return ReportError("PPB_TCPSocket_Private::Connect", connect_rv[i]);
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    const char byte = 'a' + i;
    ASSERT_SUBTEST_SUCCESS(SyncWrite(client_sockets[i], &byte, sizeof(byte)));
  }

  bool byte_received[kBacklog] = {};
  for (size_t i = 0; i < kBacklog; ++i) {
    char byte;
    ASSERT_SUBTEST_SUCCESS(SyncRead(accepted_sockets[i], &byte, sizeof(byte)));
    const size_t index = byte - 'a';
    ASSERT_FALSE(byte_received[index]);
    byte_received[index] = true;
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    client_sockets[i]->Disconnect();
    delete client_sockets[i];
    delete connect_callbacks[i];
    accepted_sockets[i]->Disconnect();
    delete accepted_sockets[i];
  }

  server_socket.StopListening();
  PASS();
}
