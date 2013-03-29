// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <vector>

#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/net_address_private.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_udp_socket_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(UDPSocketPrivate);

namespace {

const uint16_t kPortScanFrom = 1024;
const uint16_t kPortScanTo = 4096;

const size_t kEnglishAlphabetSize = 'z' - 'a' + 1;

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
  RUN_TEST_FORCEASYNC_AND_NOT(Broadcast, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(SetSocketFeatureErrors, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(QueuedRequests, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(SequentialRequests, filter);
}

std::string TestUDPSocketPrivate::GetLocalAddress(
    PP_NetAddress_Private* address) {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket.Connect(host_.c_str(), port_, callback.GetCallback());
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

std::string TestUDPSocketPrivate::SetBroadcastOptions(
    pp::UDPSocketPrivate* socket) {
  int32_t rv = socket->SetSocketFeature(PP_UDPSOCKETFEATURE_ADDRESS_REUSE,
                                        pp::Var(true));
  if (rv != PP_OK)
    return ReportError("PPB_UDPSocket_Private::SetSocketFeature", rv);

  rv = socket->SetSocketFeature(PP_UDPSOCKETFEATURE_BROADCAST, pp::Var(true));
  if (rv != PP_OK)
    return ReportError("PPB_UDPSocket_Private::SetSocketFeature", rv);

  PASS();
}

std::string TestUDPSocketPrivate::BindUDPSocket(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private* address) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = socket->Bind(address, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::Bind force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("PPB_UDPSocket_Private::Bind", rv);
  PASS();
}

std::string TestUDPSocketPrivate::LookupPortAndBindUDPSocket(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private *address) {
  PP_NetAddress_Private base_address;
  ASSERT_SUBTEST_SUCCESS(GetLocalAddress(&base_address));

  bool is_free_port_found = false;
  for (uint16_t port = kPortScanFrom; port < kPortScanTo; ++port) {
    if (!pp::NetAddressPrivate::ReplacePort(base_address, port, address))
      return "PPB_NetAddress_Private::ReplacePort: Failed";
    if (BindUDPSocket(socket, address).empty()) {
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
  int32_t rv = socket->Bind(address, callback.GetCallback());
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

std::string TestUDPSocketPrivate::ReadSocket(
    pp::UDPSocketPrivate* socket,
    PP_NetAddress_Private* address,
    size_t size,
    std::string* message,
    PP_NetAddress_Private* recvfrom_address,
    bool is_queued_recvfrom) {
  std::vector<char> buffer(size);
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = PP_ERROR_FAILED;
  if (is_queued_recvfrom) {
    rv = socket->RecvFrom(&buffer[0], size, recvfrom_address,
                          callback.GetCallback());
  } else {
    rv = socket->RecvFrom(&buffer[0], size, callback.GetCallback());
  }
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::RecvFrom force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv < 0 || size != static_cast<size_t>(rv))
    return ReportError("PPB_UDPSocket_Private::RecvFrom", rv);
  message->assign(buffer.begin(), buffer.end());
  if (!is_queued_recvfrom && recvfrom_address)
    ASSERT_TRUE(socket->GetRecvFromAddress(recvfrom_address));
  PASS();
}

std::string TestUDPSocketPrivate::PassMessage(
    pp::UDPSocketPrivate* target,
    pp::UDPSocketPrivate* source,
    PP_NetAddress_Private* address,
    const std::string& message,
    PP_NetAddress_Private* recvfrom_address,
    bool is_queued_recvfrom) {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  int32_t rv = source->SendTo(message.c_str(), message.size(), address,
                              callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("PPB_UDPSocket_Private::SendTo force_async", rv);

  std::string str;
  ASSERT_SUBTEST_SUCCESS(ReadSocket(target, address, message.size(),
                                    &str, recvfrom_address,
                                    is_queued_recvfrom));
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv < 0 || message.size() != static_cast<size_t>(rv))
    return ReportError("PPB_UDPSocket_Private::SendTo", rv);
  ASSERT_EQ(message, str);
  PASS();
}

std::string TestUDPSocketPrivate::TestConnect() {
  pp::UDPSocketPrivate server_socket(instance_), client_socket(instance_);
  PP_NetAddress_Private server_address, client_address;

  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&server_socket,
                                                    &server_address));
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&client_socket,
                                                    &client_address));
  const std::string message = "Simple message that will be sent via UDP";
  PP_NetAddress_Private recv_from_address;
  ASSERT_SUBTEST_SUCCESS(PassMessage(&server_socket, &client_socket,
                                     &server_address,
                                     message,
                                     &recv_from_address,
                                     true));
  ASSERT_TRUE(pp::NetAddressPrivate::AreEqual(recv_from_address,
                                              client_address));
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

std::string TestUDPSocketPrivate::TestBroadcast() {
  const uint8_t broadcast_ip[4] = { 0xff, 0xff, 0xff, 0xff };

  pp::UDPSocketPrivate server1(instance_), server2(instance_);

  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&server1));
  ASSERT_SUBTEST_SUCCESS(SetBroadcastOptions(&server2));
  PP_NetAddress_Private server_address;
  ASSERT_TRUE(pp::NetAddressPrivate::GetAnyAddress(false, &server_address));
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server1, &server_address));
  // Fill port field of |server_address|.
  ASSERT_TRUE(server1.GetBoundAddress(&server_address));
  ASSERT_SUBTEST_SUCCESS(BindUDPSocket(&server2, &server_address));

  const uint16_t port = pp::NetAddressPrivate::GetPort(server_address);
  PP_NetAddress_Private broadcast_address;
  ASSERT_TRUE(pp::NetAddressPrivate::CreateFromIPv4Address(
      broadcast_ip, port, &broadcast_address));

  std::string message;
  const std::string first_message = "first message";
  const std::string second_message = "second_message";

  ASSERT_SUBTEST_SUCCESS(PassMessage(&server1, &server2,
                                     &broadcast_address,
                                     first_message, NULL, true));
  // |first_message| also arrived to |server2|.
  ASSERT_SUBTEST_SUCCESS(ReadSocket(&server2, &broadcast_address,
                                    first_message.size(), &message, NULL,
                                    true));
  ASSERT_EQ(first_message, message);

  ASSERT_SUBTEST_SUCCESS(PassMessage(&server2, &server1,
                                     &broadcast_address,
                                     second_message, NULL, true));
  // |second_message| also arrived to |server1|.
  ASSERT_SUBTEST_SUCCESS(ReadSocket(&server1, &broadcast_address,
                                    second_message.size(), &message, NULL,
                                    true));
  ASSERT_EQ(second_message, message);

  server1.Close();
  server2.Close();
  PASS();
}

std::string TestUDPSocketPrivate::TestSetSocketFeatureErrors() {
  pp::UDPSocketPrivate socket(instance_);
  // Try to pass incorrect feature name.
  int32_t rv = socket.SetSocketFeature(PP_UDPSOCKETFEATURE_COUNT,
                                       pp::Var(true));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, rv);

  // Try to pass incorrect feature value's type.
  rv = socket.SetSocketFeature(PP_UDPSOCKETFEATURE_ADDRESS_REUSE, pp::Var(1));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, rv);
  PASS();
}

std::string TestUDPSocketPrivate::TestQueuedRequests() {
  const size_t kMessageSize = 256;
  const size_t kNumMessages = 256;
  const int32_t kSocketBufferSize =
      static_cast<int32_t>(4 * kMessageSize * kNumMessages);

  pp::UDPSocketPrivate server_socket(instance_), client_socket(instance_);
  PP_NetAddress_Private server_address, client_address;

  server_socket.SetSocketFeature(PP_UDPSOCKETFEATURE_RECV_BUFFER_SIZE,
                                 pp::Var(kSocketBufferSize));
  client_socket.SetSocketFeature(PP_UDPSOCKETFEATURE_SEND_BUFFER_SIZE,
                                 pp::Var(kSocketBufferSize));
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&server_socket,
                                                    &server_address));
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&client_socket,
                                                    &client_address));

  std::vector<std::string> messages(kNumMessages);
  for (size_t i = 0; i < kNumMessages; ++i)
    messages[i].resize(kMessageSize, 'a' + i % kEnglishAlphabetSize);

  std::vector<TestCompletionCallback*> sendto_callbacks(kNumMessages);
  std::vector<int32_t> sendto_rv(kNumMessages);

  std::vector<std::vector<char> > buffers(kNumMessages);
  std::vector<TestCompletionCallback*> recvfrom_callbacks(kNumMessages);
  std::vector<int32_t> recvfrom_rv(kNumMessages);

  for (size_t i = 0; i < kNumMessages; ++i) {
    sendto_callbacks[i] = new TestCompletionCallback(instance_->pp_instance(),
                                                     force_async_);
    recvfrom_callbacks[i] = new TestCompletionCallback(instance_->pp_instance(),
                                                       force_async_);
  }

  for (size_t i = 0; i < kNumMessages; ++i) {
    buffers[i].resize(messages[i].size());
    recvfrom_rv[i] = server_socket.RecvFrom(
        &buffers[i][0],
        static_cast<int32_t>(messages[i].size()),
        NULL,
        recvfrom_callbacks[i]->GetCallback());
    if (force_async_ && recvfrom_rv[i] != PP_OK_COMPLETIONPENDING) {
      return ReportError("PPB_UDPSocket_Private::RecvFrom force_async",
                         recvfrom_rv[i]);
    }
  }

  for (size_t i = 0; i < kNumMessages; ++i) {
    sendto_rv[i] = client_socket.SendTo(messages[i].c_str(),
                                        messages[i].size(),
                                        &server_address,
                                        sendto_callbacks[i]->GetCallback());
    if (force_async_ && sendto_rv[i] != PP_OK_COMPLETIONPENDING) {
      return ReportError("PPB_UDPSocket_Private::SendTo force_async",
                         sendto_rv[i]);
    }
  }

  for (size_t i = 0; i < kNumMessages; ++i) {
    if (recvfrom_rv[i] == PP_OK_COMPLETIONPENDING)
      recvfrom_rv[i] = recvfrom_callbacks[i]->WaitForResult();
    if (recvfrom_rv[i] < 0 ||
        messages[i].size() != static_cast<size_t>(recvfrom_rv[i])) {
      return ReportError("PPB_UDPSocket_Private::RecvFrom", recvfrom_rv[i]);
    }

    if (sendto_rv[i] == PP_OK_COMPLETIONPENDING)
      sendto_rv[i] = sendto_callbacks[i]->WaitForResult();
    if (sendto_rv[i] < 0 ||
        messages[i].size() != static_cast<size_t>(sendto_rv[i])) {
      return ReportError("PPB_UDPSocket_Private::SendTo", sendto_rv[i]);
    }
    ASSERT_EQ(messages[i], std::string(buffers[i].begin(), buffers[i].end()));
  }

  for (size_t i = 0; i < kNumMessages; ++i) {
    delete sendto_callbacks[i];
    delete recvfrom_callbacks[i];
  }

  server_socket.Close();
  client_socket.Close();
  PASS();
}

std::string TestUDPSocketPrivate::TestSequentialRequests() {
  const size_t kMessageSize = 256;
  const size_t kNumMessages = 256;

  pp::UDPSocketPrivate server_socket(instance_), client_socket(instance_);
  PP_NetAddress_Private server_address, client_address;

  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&server_socket,
                                                    &server_address));
  ASSERT_SUBTEST_SUCCESS(LookupPortAndBindUDPSocket(&client_socket,
                                                    &client_address));
  std::vector<std::string> messages(kNumMessages);
  for (size_t i = 0; i < kNumMessages; ++i)
    messages[i].resize(kMessageSize, 'a' + i % kEnglishAlphabetSize);

  for (size_t i = 0; i < kNumMessages; ++i) {
    ASSERT_SUBTEST_SUCCESS(PassMessage(&server_socket,
                                       &client_socket,
                                       &server_address,
                                       messages[i], NULL, false));
  }

  server_socket.Close();
  client_socket.Close();
  PASS();
}
