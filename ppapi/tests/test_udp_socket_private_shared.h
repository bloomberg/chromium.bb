// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_SHARED_H_
#define PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_SHARED_H_

#include "ppapi/cpp/private/tcp_socket_private.h"
#include "ppapi/cpp/private/udp_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestUDPSocketPrivateShared : public TestCase {
 public:
  explicit TestUDPSocketPrivateShared(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  void QuitMessageLoop();

 private:
  static const char* const kHost;
  static const int kPort = 80;

  // Creates tcp_socket and connects to www.google.com:80. After that,
  // stores into |address| local address and returns created
  // tcp_socket. This is a way to create |PP_NetAddress_Private|
  // structure filled with local IP and some free port.
  std::string GenerateNetAddress(PP_Resource* socket,
                                 PP_NetAddress_Private* address);
  std::string CreateAndBindUDPSocket(const PP_NetAddress_Private *address,
                                     PP_Resource *socket);

  std::string TestCreate();
  std::string TestConnect();

  const PPB_TCPSocket_Private* tcp_socket_private_interface_;
  const PPB_UDPSocket_Private* udp_socket_private_interface_;
};

#endif  // PPAPI_TESTS_TEST_UDP_SOCKET_PRIVATE_SHARED_H_
