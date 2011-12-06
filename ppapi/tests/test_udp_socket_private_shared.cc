// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_udp_socket_private_shared.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UDPSocketPrivateShared);

// TODO(ygorshenin): get rid of using external server in tests,
// http://crbug.com/105863
const char* const TestUDPSocketPrivateShared::kHost = "www.google.com";

TestUDPSocketPrivateShared::TestUDPSocketPrivateShared(
    TestingInstance* instance)
    : TestCase(instance),
      tcp_socket_private_interface_(NULL),
      udp_socket_private_interface_(NULL) {
}

bool TestUDPSocketPrivateShared::Init() {
  tcp_socket_private_interface_ =
      reinterpret_cast<PPB_TCPSocket_Private const*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_TCPSOCKET_PRIVATE_INTERFACE));
  if (!tcp_socket_private_interface_)
    instance_->AppendError("TCPSocketPrivate interface not available");

  udp_socket_private_interface_ =
      reinterpret_cast<PPB_UDPSocket_Private const*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_UDPSOCKET_PRIVATE_INTERFACE));
  if (!udp_socket_private_interface_)
    instance_->AppendError("UDPSocketPrivate interface not available");

  return tcp_socket_private_interface_ && udp_socket_private_interface_ &&
      InitTestingInterface();
}

void TestUDPSocketPrivateShared::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Connect, filter);
}

void TestUDPSocketPrivateShared::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

std::string TestUDPSocketPrivateShared::GenerateNetAddress(
    PP_Resource* socket, PP_NetAddress_Private* address) {
   *socket = tcp_socket_private_interface_->Create(instance_->pp_instance());
   if (0 == *socket)
     return "PPB_TCPSocket_Private::Create failed";

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = tcp_socket_private_interface_->Connect(
      *socket, kHost, kPort,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Connect force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::Connect", rv);

  rv = tcp_socket_private_interface_->GetLocalAddress(*socket, address);
  if (rv != PP_TRUE)
    return ReportError("PPB_TCPSocket_Private::GetLocalAddress", rv);

  PASS();
}

std::string TestUDPSocketPrivateShared::CreateAndBindUDPSocket(
    const PP_NetAddress_Private *address,
    PP_Resource *socket) {
  *socket = udp_socket_private_interface_->Create(instance_->pp_instance());
  if (0 == *socket)
    return "PPB_UDPSocket_Private::Create failed";
  if (!udp_socket_private_interface_->IsUDPSocket(*socket))
    return "PPB_UDPSocket_Private::IsUDPSocket failed";

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = udp_socket_private_interface_->Bind(
      *socket, address,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());

  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::Bind force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_UDPSocket_Private::Bind", rv);

  PASS();
}

std::string TestUDPSocketPrivateShared::TestCreate() {
  PP_Resource udp_socket;
  std::string error_message;

  udp_socket = udp_socket_private_interface_->Create(0);
  if (0 != udp_socket)
    return "PPB_UDPSocket_Private::Create returns valid socket " \
        "for invalid instance";

  udp_socket = udp_socket_private_interface_->Create(instance_->pp_instance());
  if (0 == udp_socket)
    return "PPB_UDPSocket_Private::Create failed";
  if (!udp_socket_private_interface_->IsUDPSocket(udp_socket))
    return "PPB_UDPSocket_Private::IsUDPSocket failed";

  PASS();
}

std::string TestUDPSocketPrivateShared::TestConnect() {
  PP_NetAddress_Private server_address, client_address;
  PP_Resource tcp_socket_server, tcp_socket_client;
  std::string error_message;

  error_message = GenerateNetAddress(&tcp_socket_server, &server_address);
  if (!error_message.empty())
    return error_message;
  error_message = GenerateNetAddress(&tcp_socket_client, &client_address);
  if (error_message.empty())
    return error_message;

  PP_Resource socket_server, socket_client;
  error_message = CreateAndBindUDPSocket(&server_address, &socket_server);
  if (error_message.empty())
    return error_message;
  error_message = CreateAndBindUDPSocket(&client_address, &socket_client);
  if (error_message.empty())
    return error_message;

  static const char* const kMessage =
      "Simple message that will be sent via UDP";
  static const size_t kMessageBufferSize = 1024;
  char message_buffer[kMessageBufferSize];

  TestCompletionCallback write_cb(instance_->pp_instance(), force_async_);
  int32_t write_rv = udp_socket_private_interface_->SendTo(
      socket_client,
      kMessage,
      strlen(kMessage),
      &server_address,
      static_cast<pp::CompletionCallback>(write_cb).pp_completion_callback());
  if (force_async_ && write_rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::SendTo force_async", write_rv);

  TestCompletionCallback read_cb(instance_->pp_instance(), force_async_);
  int32_t read_rv = udp_socket_private_interface_->RecvFrom(
      socket_server,
      message_buffer,
      strlen(kMessage),
      static_cast<pp::CompletionCallback>(read_cb).pp_completion_callback());
  if (force_async_ && read_rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::RecvFrom force_async", read_rv);

  if (read_rv == PP_OK_COMPLETIONPENDING)
    read_rv = read_cb.WaitForResult();
  if (read_rv < 0 || strlen(kMessage) != static_cast<size_t>(read_rv))
    return ReportError("PPB_UDPSocket_Private::RecvFrom", read_rv);

  if (write_rv == PP_OK_COMPLETIONPENDING)
    write_rv = write_cb.WaitForResult();
  if (write_rv < 0 || strlen(kMessage) != static_cast<size_t>(write_rv))
    return ReportError("PPB_UDPSocket_Private::SendTo", write_rv);

  ASSERT_EQ(0, strncmp(kMessage, message_buffer, strlen(kMessage)));

  udp_socket_private_interface_->Close(socket_server);
  udp_socket_private_interface_->Close(socket_client);

  tcp_socket_private_interface_->Disconnect(tcp_socket_server);
  tcp_socket_private_interface_->Disconnect(tcp_socket_client);

  PASS();
}
