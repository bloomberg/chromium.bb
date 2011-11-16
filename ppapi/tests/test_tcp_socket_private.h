// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_TCP_SOCKET_PRIVATE_H_
#define PAPPI_TESTS_TEST_TCP_SOCKET_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

// TODO(viettrungluu): A rename is in progress and this reduces the amount of
// code that'll have to be changed.
namespace pp {
namespace flash {
class TCPSocket;
}
}
typedef pp::flash::TCPSocket TCPSocketPrivate;

class TestTCPSocketPrivate : public TestCase {
 public:
  explicit TestTCPSocketPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBasic();
  std::string TestReadWrite();
  std::string TestConnectAddress();

  int32_t ReadFirstLineFromSocket(TCPSocketPrivate* socket, std::string* s);
  int32_t WriteStringToSocket(TCPSocketPrivate* socket, const std::string& s);

  std::string host_;
  uint16_t port_;
};

#endif  // PAPPI_TESTS_TEST_TCP_SOCKET_PRIVATE_H_
