// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TRANSPORT_H_
#define PPAPI_TESTS_TEST_TRANSPORT_H_

#include <string>

#include "ppapi/c/dev/ppb_transport_dev.h"
#include "ppapi/tests/test_case.h"

struct PPB_Transport_Dev;

namespace pp {
class Transport_Dev;
}  // namespace pp

class TestTransport : public TestCase {
 public:
  explicit TestTransport(TestingInstance* instance)
      : TestCase(instance),
        transport1_(NULL),
        transport2_(NULL) {
  }
  virtual ~TestTransport();


  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string InitTargets(PP_TransportType type);
  std::string Connect();
  std::string Clean();

  std::string TestCreate();
  std::string TestConnect();
  std::string TestSetProperty();
  std::string TestSendDataTcp();
  std::string TestSendDataUdp();
  std::string TestConnectAndCloseTcp();
  std::string TestConnectAndCloseUdp();

  // Used by the tests that access the C API directly.
  const PPB_Transport_Dev* transport_interface_;

  pp::Transport_Dev* transport1_;
  pp::Transport_Dev* transport2_;
};

#endif  // PPAPI_TESTS_TEST_TRANSPORT_H_
