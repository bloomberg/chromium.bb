// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_server_socket_private.h"

#include <cstddef>
#include <cstring>
#include <vector>

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

using pp::NetAddressPrivate;
using pp::TCPSocketPrivate;

namespace {

const uint16_t kPortScanFrom = 1024;
const uint16_t kPortScanTo = 4096;

}  // namespace

REGISTER_TEST_CASE(TCPServerSocketPrivate);

TestTCPServerSocketPrivate::TestTCPServerSocketPrivate(
    TestingInstance* instance)
    : TestCase(instance),
      core_interface_(NULL),
      tcp_server_socket_private_interface_(NULL),
      tcp_socket_private_interface_(NULL),
      port_(0) {
}

bool TestTCPServerSocketPrivate::Init() {
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  if (!core_interface_)
    instance_->AppendError("PPB_Core interface not available");
  tcp_server_socket_private_interface_ =
      static_cast<const PPB_TCPServerSocket_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE));
  if (!tcp_server_socket_private_interface_) {
    instance_->AppendError(
        "PPB_TCPServerSocket_Private interface not available");
  }

  tcp_socket_private_interface_ =
      static_cast<const PPB_TCPSocket_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_TCPSOCKET_PRIVATE_INTERFACE));
  if (!tcp_socket_private_interface_)
    instance_->AppendError("PPB_TCPSocket_Private interface not available");

  bool tcp_socket_private_is_available = TCPSocketPrivate::IsAvailable();
  if (!tcp_socket_private_is_available)
    instance_->AppendError("PPB_TCPSocket_Private interface not available");

  bool net_address_private_is_available = NetAddressPrivate::IsAvailable();
  if (!net_address_private_is_available)
    instance_->AppendError("PPB_NetAddress_Private interface not available");

  bool init_host_port = GetLocalHostPort(
      instance_->pp_instance(), &host_, &port_);
  if (!init_host_port)
    instance_->AppendError("Can't init host and port");

  return core_interface_ &&
      tcp_server_socket_private_interface_ &&
      tcp_socket_private_is_available &&
      net_address_private_is_available &&
      init_host_port &&
      CheckTestingInterface();
}

void TestTCPServerSocketPrivate::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Create, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Listen, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Backlog, filter);
}

std::string TestTCPServerSocketPrivate::GetLocalAddress(
    PP_NetAddress_Private* address) {
  TCPSocketPrivate socket(instance_);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket.Connect(host_.c_str(), port_, callback);
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

std::string TestTCPServerSocketPrivate::SyncRead(PP_Resource socket,
                                                 char* buffer,
                                                 int32_t num_bytes) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  int32_t rv = tcp_socket_private_interface_->Read(
      socket, buffer, num_bytes,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());

  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Read force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();

  if (num_bytes != rv)
    return ReportError("PPB_TCPSocket_Private::Read", rv);

  PASS();
}

std::string TestTCPServerSocketPrivate::SyncWrite(PP_Resource socket,
                                                  const char* buffer,
                                                  int32_t num_bytes) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = tcp_socket_private_interface_->Write(
      socket, buffer, num_bytes,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());

  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Write force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (num_bytes != rv)
    return ReportError("PPB_TCPSocket_Private::Write", rv);

  PASS();
}

std::string TestTCPServerSocketPrivate::SyncConnect(PP_Resource socket,
                                                    const char* host,
                                                    uint16_t port) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = tcp_socket_private_interface_->Connect(
      socket,
      host_.c_str(),
      port,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Connect force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::Connect", rv);

  PASS();
}

void TestTCPServerSocketPrivate::ForceConnect(PP_Resource socket,
                                              const char* host,
                                              uint16_t port) {
  std::string error_message;
  do {
    error_message = SyncConnect(socket, host, port);
  } while (!error_message.empty());
}

std::string TestTCPServerSocketPrivate::SyncListen(PP_Resource socket,
                                                   uint16_t* port,
                                                   int32_t backlog) {
  PP_NetAddress_Private base_address, local_address;
  ASSERT_SUBTEST_SUCCESS(GetLocalAddress(&base_address));

  // TODO (ygorshenin): find more efficient way to select available
  // ports.
  bool is_free_port_found = false;
  for (uint16_t p = kPortScanFrom; p < kPortScanTo; ++p) {
    if (!NetAddressPrivate::ReplacePort(base_address, p, &local_address))
      return ReportError("PPB_NetAddress_Private::ReplacePort", 0);

    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = tcp_server_socket_private_interface_->Listen(
        socket,
        &local_address,
        backlog,
        static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPServerSocket_Private::Listen force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv == PP_OK) {
      *port = p;
      is_free_port_found = true;
      break;
    }
  }

  if (!is_free_port_found)
    return "Can't find available port";
  PASS();
}

bool TestTCPServerSocketPrivate::IsSocketsConnected(
    PP_Resource lhs,
    PP_Resource rhs) {
  PP_NetAddress_Private lhs_local_addr, lhs_remote_addr;

  if (!tcp_socket_private_interface_->GetLocalAddress(lhs, &lhs_local_addr))
    return false;
  if (!tcp_socket_private_interface_->GetRemoteAddress(lhs, &lhs_remote_addr))
    return false;

  PP_NetAddress_Private rhs_local_addr, rhs_remote_addr;
  if (!tcp_socket_private_interface_->GetLocalAddress(rhs, &rhs_local_addr))
    return false;
  if (!tcp_socket_private_interface_->GetRemoteAddress(rhs, &rhs_remote_addr))
    return false;

  return NetAddressPrivate::AreEqual(lhs_local_addr, rhs_remote_addr) &&
      NetAddressPrivate::AreEqual(lhs_remote_addr, rhs_local_addr);
}

std::string TestTCPServerSocketPrivate::SendMessage(PP_Resource dst,
                                                    PP_Resource src,
                                                    const char* message) {
  const size_t message_size = strlen(message);

  ASSERT_SUBTEST_SUCCESS(SyncWrite(src, message, message_size));

  std::vector<char> message_buffer(message_size);
  ASSERT_SUBTEST_SUCCESS(SyncRead(dst, &message_buffer[0], message_size));
  ASSERT_EQ(0, strncmp(message, &message_buffer[0], message_size));
  PASS();
}

std::string TestTCPServerSocketPrivate::TestConnectedSockets(PP_Resource lhs,
                                                             PP_Resource rhs) {
  static const char* const kMessage = "simple message";
  ASSERT_SUBTEST_SUCCESS(SendMessage(lhs, rhs, kMessage));
  ASSERT_SUBTEST_SUCCESS(SendMessage(rhs, lhs, kMessage));
  PASS();
}

std::string TestTCPServerSocketPrivate::TestCreate() {
  PP_Resource server_socket;

  server_socket = tcp_server_socket_private_interface_->Create(0);
  ASSERT_EQ(0, server_socket);
  core_interface_->ReleaseResource(server_socket);

  server_socket =
      tcp_server_socket_private_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(server_socket != 0);
  ASSERT_TRUE(tcp_server_socket_private_interface_->IsTCPServerSocket(
      server_socket));
  core_interface_->ReleaseResource(server_socket);
  PASS();
}

std::string TestTCPServerSocketPrivate::TestListen() {
  static const int kBacklog = 2;

  PP_Resource server_socket =
      tcp_server_socket_private_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(server_socket != 0);

  uint16_t port;
  ASSERT_SUBTEST_SUCCESS(SyncListen(server_socket, &port, kBacklog));

  TestCompletionCallback accept_callback(instance_->pp_instance(),
                                         force_async_);
  PP_Resource accepted_socket;
  int32_t accept_rv = tcp_server_socket_private_interface_->Accept(
      server_socket,
      &accepted_socket,
      static_cast<pp::CompletionCallback>(
          accept_callback).pp_completion_callback());

  TCPSocketPrivate client_socket(instance_);

  ForceConnect(client_socket.pp_resource(), host_.c_str(), port);

  if (force_async_ && accept_rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPServerSocket_Private::Accept force_async",
                       accept_rv);
  if (accept_rv == PP_OK_COMPLETIONPENDING)
    accept_rv = accept_callback.WaitForResult();
  if (accept_rv != PP_OK)
    return ReportError("PPB_TCPServerSocket_Private::Accept", accept_rv);

  ASSERT_TRUE(accepted_socket != 0);
  ASSERT_TRUE(tcp_socket_private_interface_->IsTCPSocket(accepted_socket));

  ASSERT_TRUE(IsSocketsConnected(client_socket.pp_resource(), accepted_socket));
  ASSERT_SUBTEST_SUCCESS(TestConnectedSockets(client_socket.pp_resource(),
                                              accepted_socket));

  tcp_socket_private_interface_->Disconnect(accepted_socket);
  client_socket.Disconnect();
  tcp_server_socket_private_interface_->StopListening(server_socket);

  core_interface_->ReleaseResource(accepted_socket);
  core_interface_->ReleaseResource(server_socket);
  PASS();
}

std::string TestTCPServerSocketPrivate::TestBacklog() {
  static const size_t kBacklog = 5;

  PP_Resource server_socket =
      tcp_server_socket_private_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(server_socket != 0);

  uint16_t port;
  ASSERT_SUBTEST_SUCCESS(SyncListen(server_socket, &port, 2 * kBacklog));

  std::vector<TCPSocketPrivate*> client_sockets(kBacklog);
  std::vector<TestCompletionCallback*> connect_callbacks(kBacklog);
  std::vector<int32_t> connect_rv(kBacklog);
  for (size_t i = 0; i < kBacklog; ++i) {
    client_sockets[i] = new TCPSocketPrivate(instance_);
    connect_callbacks[i] = new TestCompletionCallback(instance_->pp_instance(),
                                                      force_async_);
    connect_rv[i] = client_sockets[i]->Connect(host_.c_str(),
                                               port,
                                               *connect_callbacks[i]);
    if (force_async_ && connect_rv[i] != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPSocket_Private::Connect force_async",
                         connect_rv[i]);
  }

  std::vector<PP_Resource> accepted_sockets(kBacklog);
  for (size_t i = 0; i < kBacklog; ++i) {
    TestCompletionCallback callback(instance_->pp_instance(), force_async_);
    int32_t rv = tcp_server_socket_private_interface_->Accept(
        server_socket,
        &accepted_sockets[i],
        static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("PPB_TCPServerSocket_Private::Accept force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("PPB_TCPServerSocket_Private::Accept", rv);

    ASSERT_TRUE(accepted_sockets[i] != 0);
    ASSERT_TRUE(tcp_socket_private_interface_->IsTCPSocket(
        accepted_sockets[i]));
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    if (connect_rv[i] == PP_OK_COMPLETIONPENDING)
      connect_rv[i] = connect_callbacks[i]->WaitForResult();
    if (connect_rv[i] != PP_OK)
      return ReportError("PPB_TCPSocket_Private::Connect", connect_rv[i]);
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    bool found = false;
    for (size_t j = 0; j < kBacklog && !found; ++j)
      if (IsSocketsConnected(client_sockets[i]->pp_resource(),
                             accepted_sockets[j])) {
        TestConnectedSockets(client_sockets[i]->pp_resource(),
                             accepted_sockets[j]);
        found = true;
      }
    ASSERT_TRUE(found);
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    tcp_socket_private_interface_->Disconnect(accepted_sockets[i]);
    core_interface_->ReleaseResource(accepted_sockets[i]);
  }

  for (size_t i = 0; i < kBacklog; ++i) {
    client_sockets[i]->Disconnect();
    delete client_sockets[i];
    delete connect_callbacks[i];
  }

  tcp_server_socket_private_interface_->StopListening(server_socket);
  core_interface_->ReleaseResource(server_socket);
  PASS();
}
