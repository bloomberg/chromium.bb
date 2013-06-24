// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UDP_SOCKET_H_
#define PPAPI_TESTS_TEST_UDP_SOCKET_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/tests/test_case.h"

namespace pp {
class UDPSocket;
}

class TestUDPSocket: public TestCase {
 public:
  explicit TestUDPSocket(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string GetLocalAddress(pp::NetAddress* address);
  std::string SetBroadcastOptions(pp::UDPSocket* socket);
  std::string BindUDPSocket(pp::UDPSocket* socket,
                            const pp::NetAddress& address);
  std::string LookupPortAndBindUDPSocket(pp::UDPSocket* socket,
                                         pp::NetAddress* address);
  std::string ReadSocket(pp::UDPSocket* socket,
                         pp::NetAddress* address,
                         size_t size,
                         std::string* message);
  std::string PassMessage(pp::UDPSocket* target,
                          pp::UDPSocket* source,
                          const pp::NetAddress& target_address,
                          const std::string& message,
                          pp::NetAddress* recvfrom_address);

  std::string TestReadWrite();
  std::string TestBroadcast();
  std::string TestSetOption();

  pp::NetAddress address_;
};

#endif  // PPAPI_TESTS_TEST_UDP_SOCKET_H_
