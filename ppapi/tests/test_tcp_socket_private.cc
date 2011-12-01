// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_tcp_socket_private.h"

#include <stdlib.h>

#include "ppapi/c/dev/ppb_url_util_dev.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/tests/test_utils.h"

namespace {

// Validates the first line of an HTTP response.
bool ValidateHttpResponse(const std::string& s) {
  // Just check that it begins with "HTTP/" and ends with a "\r\n".
  return s.size() >= 5 &&
         s.substr(0, 5) == "HTTP/" &&
         s.substr(s.size() - 2) == "\r\n";
}

}  // namespace

REGISTER_TEST_CASE(TCPSocketPrivate);

TestTCPSocketPrivate::TestTCPSocketPrivate(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestTCPSocketPrivate::Init() {
  if (!pp::TCPSocketPrivate::IsAvailable())
    return false;

  // This test currently only works out-of-process (since the API is really only
  // implemented in that case).
  const PPB_Testing_Dev* testing = GetTestingInterface();
  if (!testing)
    return false;
  if (!testing->IsOutOfProcess())
    return false;

  // We need something to connect to, so we connect to the HTTP server whence we
  // came. Grab the host and port.
  if (!EnsureRunningOverHTTP())
    return false;

  PP_URLComponents_Dev components;
  pp::Var pp_url = pp::URLUtil_Dev::Get()->GetDocumentURL(*instance_,
                                                          &components);
  if (!pp_url.is_string())
    return false;
  std::string url = pp_url.AsString();

  // Double-check that we're running on HTTP.
  if (components.scheme.len < 0)
    return false;
  if (url.substr(components.scheme.begin, components.scheme.len) != "http")
    return false;

  // Get host.
  if (components.host.len < 0)
    return false;
  host_ = url.substr(components.host.begin, components.host.len);

  // Get port (it's optional).
  port_ = 80;  // Default value.
  if (components.port.len > 0) {
    int i = atoi(url.substr(components.port.begin,
                            components.port.len).c_str());
    if (i < 0 || i > 65535)
      return false;
    port_ = static_cast<uint16_t>(i);
  }

  return true;
}

void TestTCPSocketPrivate::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Basic, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ReadWrite, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ConnectAddress, filter);
}

std::string TestTCPSocketPrivate::TestBasic() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), force_async_);

  int32_t rv = socket.Connect(host_.c_str(), port_, cb);
  ASSERT_TRUE(!force_async_ || rv == PP_OK_COMPLETIONPENDING);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = cb.WaitForResult();
  ASSERT_EQ(PP_OK, rv);

  PP_NetAddress_Private unused;
  // TODO(viettrungluu): check the values somehow.
  ASSERT_TRUE(socket.GetLocalAddress(&unused));
  ASSERT_TRUE(socket.GetRemoteAddress(&unused));

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestReadWrite() {
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), force_async_);

  int32_t rv = socket.Connect(host_.c_str(), port_, cb);
  ASSERT_TRUE(!force_async_ || rv == PP_OK_COMPLETIONPENDING);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = cb.WaitForResult();
  ASSERT_EQ(PP_OK, rv);

  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));

  // Read up to the first \n and check that it looks like valid HTTP response.
  std::string s;
  ASSERT_EQ(PP_OK, ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  socket.Disconnect();

  PASS();
}

std::string TestTCPSocketPrivate::TestConnectAddress() {
  PP_NetAddress_Private address;

  // First, bring up a connection and grab the address.
  {
    pp::TCPSocketPrivate socket(instance_);
    TestCompletionCallback cb(instance_->pp_instance(), force_async_);
    int32_t rv = socket.Connect(host_.c_str(), port_, cb);
    ASSERT_TRUE(!force_async_ || rv == PP_OK_COMPLETIONPENDING);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = cb.WaitForResult();
    ASSERT_EQ(PP_OK, rv);
    ASSERT_TRUE(socket.GetRemoteAddress(&address));
    // Omit the |Disconnect()| here to make sure we don't crash if we just let
    // the resource be destroyed.
  }

  // Connect to that address.
  pp::TCPSocketPrivate socket(instance_);
  TestCompletionCallback cb(instance_->pp_instance(), force_async_);
  int32_t rv = socket.ConnectWithNetAddress(&address, cb);
  ASSERT_TRUE(!force_async_ || rv == PP_OK_COMPLETIONPENDING);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = cb.WaitForResult();
  ASSERT_EQ(PP_OK, rv);

  // Make sure we can read/write to it properly (see |TestReadWrite()|).
  ASSERT_EQ(PP_OK, WriteStringToSocket(&socket, "GET / HTTP/1.0\r\n\r\n"));
  std::string s;
  ASSERT_EQ(PP_OK, ReadFirstLineFromSocket(&socket, &s));
  ASSERT_TRUE(ValidateHttpResponse(s));

  socket.Disconnect();

  PASS();
}

// TODO(viettrungluu): Try testing SSL somehow.

int32_t TestTCPSocketPrivate::ReadFirstLineFromSocket(
    pp::TCPSocketPrivate* socket,
    std::string* s) {
  char buffer[10000];

  s->clear();
  // Make sure we don't just hang if |Read()| spews.
  while (s->size() < 1000000) {
    TestCompletionCallback cb(instance_->pp_instance(), force_async_);
    int32_t rv = socket->Read(buffer, sizeof(buffer), cb);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return PP_ERROR_FAILED;
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = cb.WaitForResult();
    if (rv < 0)
      return rv;
    if (rv == 0)
      return PP_ERROR_FAILED;  // Didn't get a \n-terminated line.
    s->reserve(s->size() + rv);
    for (int32_t i = 0; i < rv; i++) {
      s->push_back(buffer[i]);
      if (buffer[i] == '\n')
        return PP_OK;
    }
  }
  return PP_ERROR_FAILED;
}

int32_t TestTCPSocketPrivate::WriteStringToSocket(pp::TCPSocketPrivate* socket,
                                                  const std::string& s) {
  const char* buffer = s.data();
  size_t written = 0;
  while (written < s.size()) {
    TestCompletionCallback cb(instance_->pp_instance(), force_async_);
    int32_t rv = socket->Write(buffer + written, s.size() - written, cb);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return PP_ERROR_FAILED;
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = cb.WaitForResult();
    if (rv < 0)
      return rv;
    if (rv == 0)
      return PP_ERROR_FAILED;
    written += rv;
  }
  if (written != s.size())
    return PP_ERROR_FAILED;
  return PP_OK;
}
