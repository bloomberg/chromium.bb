// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_QUERY_POLICY_H_
#define PAPPI_TESTS_TEST_QUERY_POLICY_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_QueryPolicy_Dev;

class TestQueryPolicy : public TestCase {
 public:
  explicit TestQueryPolicy(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestGetFullPolicy();
  std::string TestSubscribeToPolicyUpdates();

  // Used by the tests that access the C API directly.
  const PPB_QueryPolicy_Dev* query_policy_interface_;
};

#endif  // PAPPI_TESTS_TEST_BUFFER_H_
