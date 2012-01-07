// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_RESOURCE_ARRAY_H_
#define PPAPI_TESTS_TEST_RESOURCE_ARRAY_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestResourceArray : public TestCase {
 public:
  explicit TestResourceArray(TestingInstance* instance);
  virtual ~TestResourceArray();

  // TestCase implementation.
  virtual void RunTests(const std::string& test_filter);

 private:
  std::string TestBasics();
  std::string TestOutOfRangeAccess();
  std::string TestEmptyArray();
  std::string TestInvalidElement();
};

#endif  // PPAPI_TESTS_TEST_RESOURCE_ARRAY_H_

