// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_NETWORK_PROXY_H_
#define PAPPI_TESTS_TEST_NETWORK_PROXY_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestNetworkProxy : public TestCase {
 public:
  explicit TestNetworkProxy(TestingInstance* instance);

 private:
  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

  std::string TestGetProxyForURL();
};

#endif  // PAPPI_TESTS_TEST_NETWORK_PROXY_H_
