// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket.h"

#include "ppapi/cpp/dev/tcp_socket_dev.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

namespace {

// Validates the first line of an HTTP response.
bool ValidateHttpResponse(const std::string& s) {
  // Just check that it begins with "HTTP/" and ends with a "\r\n".
  return s.size() >= 5 &&
         s.substr(0, 5) == "HTTP/" &&
         s.substr(s.size() - 2) == "\r\n";
}

}  // namespace

REGISTER_TEST_CASE(TCPSocket);

TestTCPSocket::TestTCPSocket(TestingInstance* instance) : TestCase(instance) {
}

bool TestTCPSocket::Init() {
  if (!pp::TCPSocket_Dev::IsAvailable())
    return false;

  // We need something to connect to, so we connect to the HTTP server whence we
  // came. Grab the host and port.
  if (!EnsureRunningOverHTTP())
    return false;

  std::string host;
  uint16_t port = 0;
  if (!GetLocalHostPort(instance_->pp_instance(), &host, &port))
    return false;

  if (!ResolveHost(instance_->pp_instance(), host, port, &addr_))
    return false;

  return true;
}

void TestTCPSocket::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestTCPSocket, Connect, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, ReadWrite, filter);
  RUN_CALLBACK_TEST(TestTCPSocket, SetOption, filter);
}

std::string TestTCPSocket::TestConnect() {
  pp::TCPSocket_Dev socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  pp::NetAddress_Dev local_addr, remote_addr;
  local_addr = socket.GetLocalAddress();
  remote_addr = socket.GetRemoteAddress();

  ASSERT_NE(0, local_addr.pp_resource());
  ASSERT_NE(0, remote_addr.pp_resource());
  ASSERT_TRUE(EqualNetAddress(addr_, remote_addr));

  socket.Close();

  PASS();
}

std::string TestTCPSocket::TestReadWrite() {
  pp::TCPSocket_Dev socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(socket.Connect(addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));

  // Read up to the first \n and check that it looks like valid HTTP response.
  std::string s;
  ASSERT_EQ(PP_OK, ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  PASS();
}

std::string TestTCPSocket::TestSetOption() {
  pp::TCPSocket_Dev socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), callback_type());

  cb.WaitForResult(
      socket.SetOption(PP_TCPSOCKET_OPTION_NO_DELAY, true, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_ERROR_FAILED, cb.result());

  cb.WaitForResult(socket.Connect(addr_, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  cb.WaitForResult(
      socket.SetOption(PP_TCPSOCKET_OPTION_NO_DELAY, true, cb.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(cb);
  ASSERT_EQ(PP_OK, cb.result());

  PASS();
}

int32_t TestTCPSocket::ReadFirstLineFromSocket(pp::TCPSocket_Dev* socket,
                                               std::string* s) {
  char buffer[1000];

  s->clear();
  // Make sure we don't just hang if |Read()| spews.
  while (s->size() < 10000) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    int32_t rv = socket->Read(buffer, sizeof(buffer), cb.GetCallback());
    if (callback_type() == PP_REQUIRED && rv != PP_OK_COMPLETIONPENDING)
      return PP_ERROR_FAILED;
    cb.WaitForResult(rv);
    if (cb.result() < 0)
      return cb.result();
    if (cb.result() == 0)
      return PP_ERROR_FAILED;  // Didn't get a \n-terminated line.
    s->reserve(s->size() + cb.result());
    for (int32_t i = 0; i < cb.result(); i++) {
      s->push_back(buffer[i]);
      if (buffer[i] == '\n')
        return PP_OK;
    }
  }
  return PP_ERROR_FAILED;
}

int32_t TestTCPSocket::WriteStringToSocket(pp::TCPSocket_Dev* socket,
                                           const std::string& s) {
  const char* buffer = s.data();
  size_t written = 0;
  while (written < s.size()) {
    TestCompletionCallback cb(instance_->pp_instance(), callback_type());
    int32_t rv = socket->Write(buffer + written, s.size() - written,
                               cb.GetCallback());
    if (callback_type() == PP_REQUIRED && rv != PP_OK_COMPLETIONPENDING)
      return PP_ERROR_FAILED;
    cb.WaitForResult(rv);
    if (cb.result() < 0)
      return cb.result();
    if (cb.result() == 0)
      return PP_ERROR_FAILED;
    written += cb.result();
  }
  if (written != s.size())
    return PP_ERROR_FAILED;
  return PP_OK;
}
