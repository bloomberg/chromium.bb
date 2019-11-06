// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_NET_ADDRESS_PRIVATE_H_
#define PAPPI_TESTS_TEST_NET_ADDRESS_PRIVATE_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestNetAddressPrivate : public TestCase {
 public:
  explicit TestNetAddressPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestAreEqual();
  std::string TestAreHostsEqual();
  std::string TestDescribe();
  std::string TestReplacePort();
  std::string TestGetAnyAddress();
  std::string TestDescribeIPv6();
  std::string TestGetFamily();
  std::string TestGetPort();
  std::string TestGetAddress();
  std::string TestGetScopeID();
};

#endif  // PAPPI_TESTS_TEST_NET_ADDRESS_PRIVATE_H_
