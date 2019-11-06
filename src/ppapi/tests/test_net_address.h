// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_NET_ADDRESS_H_
#define PAPPI_TESTS_TEST_NET_ADDRESS_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestNetAddress : public TestCase {
 public:
  explicit TestNetAddress(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestIPv4Address();
  std::string TestIPv6Address();
  std::string TestDescribeAsString();
};

#endif  // PAPPI_TESTS_TEST_NET_ADDRESS_H_
