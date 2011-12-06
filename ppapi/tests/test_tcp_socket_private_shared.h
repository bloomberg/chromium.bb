// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_SHARED_H_
#define PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_SHARED_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestTCPSocketPrivateShared : public TestCase {
 public:
  explicit TestTCPSocketPrivateShared(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void QuitMessageLoop();

 private:
  static const char* const kHost;
  static const int kPort = 443;

  std::string CreateSocket(PP_Resource* socket);
  std::string SyncConnect(PP_Resource socket, const char* host, int port);
  std::string SyncConnectWithNetAddress(PP_Resource socket,
                                        const PP_NetAddress_Private& addr);
  std::string SyncSSLHandshake(PP_Resource socket, const char* host, int port);
  std::string SyncRead(PP_Resource socket, char* buffer, int32_t num_bytes,
                       int32_t* bytes_read);
  std::string SyncWrite(PP_Resource socket, const char* buffer,
                        int32_t num_bytes, int32_t* bytes_wrote);
  std::string CheckHTTPResponse(PP_Resource socket,
                                const std::string& request,
                                const std::string& response);

  std::string TestCreate();
  std::string TestGetAddress();
  std::string TestConnect();
  std::string TestReconnect();

  const PPB_TCPSocket_Private* tcp_socket_private_interface_;
};

#endif  // PPAPI_TESTS_TEST_TCP_SOCKET_PRIVATE_SHARED_H_
