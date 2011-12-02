// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket_private_shared.h"

#include <string.h>
#include <new>
#include <string>
#include <vector>

#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(TCPSocketPrivateShared);

// TODO(ygorshenin): get rid of using external server in tests,
// http://crbug.com/105863
const char* const TestTCPSocketPrivateShared::kHost = "www.google.com";

TestTCPSocketPrivateShared::TestTCPSocketPrivateShared(
    TestingInstance* instance)
    : TestCase(instance), tcp_socket_private_interface_(NULL) {
}

bool TestTCPSocketPrivateShared::Init() {
  tcp_socket_private_interface_ =
      reinterpret_cast<PPB_TCPSocket_Private const*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_TCPSOCKET_PRIVATE_INTERFACE));
  if (!tcp_socket_private_interface_)
    instance_->AppendError("TCPSocketPrivate interface not available");
  return tcp_socket_private_interface_ && InitTestingInterface();
}

void TestTCPSocketPrivateShared::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(GetAddress, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Connect, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(Reconnect, filter);
}

void TestTCPSocketPrivateShared::QuitMessageLoop() {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

std::string TestTCPSocketPrivateShared::CreateSocket(PP_Resource* socket) {
  *socket = tcp_socket_private_interface_->Create(instance_->pp_instance());
  if (0 == *socket)
    return "PPB_TCPSocket_Private::Create failed";
  if (!tcp_socket_private_interface_->IsTCPSocket(*socket))
    return "PPB_TCPSocket_Private::IsTCPSocket failed";
  PASS();
}

std::string TestTCPSocketPrivateShared::SyncConnect(PP_Resource socket,
                                                    const char* host,
                                                    int port) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  int32_t rv = tcp_socket_private_interface_->Connect(
      socket, host, port,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());

  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::Connect force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::Connect", rv);
  PASS();
}

std::string TestTCPSocketPrivateShared::SyncConnectWithNetAddress(
    PP_Resource socket, const PP_NetAddress_Private& addr) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  int32_t rv = tcp_socket_private_interface_->ConnectWithNetAddress(
      socket, &addr,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());

  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError(
        "PPB_TCPSocket_Private::ConnectWithNetAddress force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::ConnectWithNetAddress", rv);
  PASS();
}

std::string TestTCPSocketPrivateShared::SyncSSLHandshake(PP_Resource socket,
                                                         const char* host,
                                                         int port) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  int32_t rv = tcp_socket_private_interface_->SSLHandshake(
      socket, host, port,
      static_cast<pp::CompletionCallback>(callback).pp_completion_callback());

  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_TCPSocket_Private::SSLHandshake force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_TCPSocket_Private::SSLHandshake", rv);
  PASS();
}

std::string TestTCPSocketPrivateShared::SyncRead(PP_Resource socket,
                                                 char* buffer,
                                                 int32_t num_bytes,
                                                 int32_t* bytes_read) {
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

  *bytes_read = rv;
  PASS();
}

std::string TestTCPSocketPrivateShared::SyncWrite(PP_Resource socket,
                                                  const char* buffer,
                                                  int32_t num_bytes,
                                                  int32_t* bytes_wrote) {
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

  *bytes_wrote = rv;
  PASS();
}

std::string TestTCPSocketPrivateShared::CheckHTTPResponse(
    PP_Resource socket,
    const std::string& request,
    const std::string& response) {
  int32_t rv;
  std::string error_message;

  error_message = SyncWrite(socket, request.c_str(), request.size(), &rv);
  if (!error_message.empty())
    return error_message;

  std::vector<char> response_buffer(response.size());
  error_message = SyncRead(socket, &response_buffer[0], response.size(), &rv);
  if (!error_message.empty())
    return error_message;

  std::string actual_response(&response_buffer[0], rv);

  if (response != actual_response)
    return "CheckHTTPResponse failed, expected: " + response +
        ", actual: " + actual_response;
  PASS();
}

std::string TestTCPSocketPrivateShared::TestCreate() {
  PP_Resource socket = tcp_socket_private_interface_->Create(0);
  if (0 != socket)
    return "PPB_TCPSocket_Private::Create returns valid socket " \
        "for invalid instance";

  return CreateSocket(&socket);
}

std::string TestTCPSocketPrivateShared::TestGetAddress() {
  PP_Resource socket;
  std::string error_message;

  error_message = CreateSocket(&socket);
  if (!error_message.empty())
    return error_message;

  error_message = SyncConnect(socket, kHost, kPort);
  if (!error_message.empty())
    return error_message;

  PP_NetAddress_Private local_address, remote_address;

  if (PP_TRUE != tcp_socket_private_interface_->GetLocalAddress(
          socket, &local_address))
    return "PPB_TCPSocketPrivate::GetLocalAddress failed";
  if (PP_TRUE != tcp_socket_private_interface_->GetRemoteAddress(
          socket, &remote_address))
    return "PPB_TCPSocketPrivate::GetRemoteAddress failed";

  tcp_socket_private_interface_->Disconnect(socket);

  PASS();
}

std::string TestTCPSocketPrivateShared::TestConnect() {
  PP_Resource socket;
  std::string error_message;

  error_message = CreateSocket(&socket);
  if (!error_message.empty())
    return error_message;
  error_message = SyncConnect(socket, kHost, kPort);
  if (!error_message.empty())
    return error_message;
  error_message = SyncSSLHandshake(socket, kHost, kPort);
  if (!error_message.empty())
    return error_message;
  error_message =
      CheckHTTPResponse(socket, "GET /robots.txt\r\n", "HTTP/1.0 200 OK");
  if (!error_message.empty())
    return error_message;
  tcp_socket_private_interface_->Disconnect(socket);

  PASS();
}

std::string TestTCPSocketPrivateShared::TestReconnect() {
  PP_Resource socket;
  std::string error_message;

  error_message = CreateSocket(&socket);
  if (!error_message.empty())
    return error_message;
  error_message = SyncConnect(socket, kHost, kPort);
  if (!error_message.empty())
    return error_message;
  error_message = SyncSSLHandshake(socket, kHost, kPort);
  if (!error_message.empty())
    return error_message;

  PP_NetAddress_Private remote_address;
  if (PP_TRUE !=
      tcp_socket_private_interface_->GetRemoteAddress(socket,
                                                      &remote_address))
    return "PPB_TCPSocketPrivate::GetRemoteAddress failed";
  tcp_socket_private_interface_->Disconnect(socket);

  error_message = CreateSocket(&socket);
  if (!error_message.empty())
    return error_message;
  error_message = SyncConnectWithNetAddress(socket, remote_address);
  if (!error_message.empty())
    return error_message;
  error_message = SyncSSLHandshake(socket, kHost, kPort);
  if (!error_message.empty())
    return error_message;
  error_message =
      CheckHTTPResponse(socket, "GET /robots.txt\r\n", "HTTP/1.0 200 OK");
  if (!error_message.empty())
    return error_message;
  tcp_socket_private_interface_->Disconnect(socket);

  PASS();
}
