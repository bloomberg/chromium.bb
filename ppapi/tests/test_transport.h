// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_TRANSPORT_H_
#define PPAPI_TESTS_TEST_TRANSPORT_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Transport_Dev;

namespace pp {
class Transport;
}

class TestTransport : public TestCase {
 public:
  explicit TestTransport(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestBasics();

  // Used by the tests that access the C API directly.
  const PPB_Transport_Dev* transport_interface_;
};

#endif  // PPAPI_TESTS_TEST_TRANSPORT_H_
