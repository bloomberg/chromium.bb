// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_BROKER_H_
#define PPAPI_TESTS_TEST_BROKER_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

struct PPB_BrokerTrusted;

class TestBroker : public TestCase {
 public:
  TestBroker(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestCreate();

  const struct PPB_BrokerTrusted* broker_interface_;
};

#endif  // PPAPI_TESTS_TEST_BROKER_H_
