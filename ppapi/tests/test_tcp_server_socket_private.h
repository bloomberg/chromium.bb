// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TCP_SERVER_SOCKET_PRIVATE_H_
#define PPAPI_TESTS_TEST_TCP_SERVER_SOCKET_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/c/private/ppb_tcp_server_socket_private.h"
#include "ppapi/c/private/ppb_tcp_socket_private.h"
#include "ppapi/tests/test_case.h"

class TestTCPServerSocketPrivate : public TestCase {
 public:
  explicit TestTCPServerSocketPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string GetLocalAddress(PP_NetAddress_Private* address);
  std::string SyncRead(PP_Resource socket, char* buffer, int32_t num_bytes);
  std::string SyncWrite(PP_Resource socket,
                        const char* buffer,
                        int32_t num_bytes);
  std::string SyncConnect(PP_Resource socket, const char* host, uint16_t port);
  void ForceConnect(PP_Resource socket, const char* host, uint16_t port);
  std::string SyncListen(PP_Resource socket, uint16_t* port, int32_t backlog);

  bool IsSocketsConnected(PP_Resource lhs, PP_Resource rhs);
  std::string SendMessage(PP_Resource dst,
                          PP_Resource src,
                          const char* message);
  std::string TestConnectedSockets(PP_Resource lhs, PP_Resource rhs);

  std::string CheckIOOfConnectedSockets(PP_Resource src, PP_Resource dst);
  std::string CheckAddressesOfConnectedSockets(PP_Resource lhs,
                                               PP_Resource rhs);

  std::string TestCreate();
  std::string TestListen();
  std::string TestBacklog();

  const PPB_Core* core_interface_;
  const PPB_TCPServerSocket_Private* tcp_server_socket_private_interface_;
  const PPB_TCPSocket_Private* tcp_socket_private_interface_;
  std::string host_;
  uint16_t port_;
};

#endif  // PPAPI_TESTS_TEST_TCP_SERVER_SOCKET_PRIVATE_H_
