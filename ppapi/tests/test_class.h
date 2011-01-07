// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CLASS_H_
#define PPAPI_TESTS_TEST_CLASS_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Class;
struct PPB_Testing_Dev;

class TestClass : public TestCase {
 public:
  explicit TestClass(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestConstructEmptyObject();

  // Used by the tests that access the C API directly.
  const PPB_Class* class_interface_;
  const PPB_Testing_Dev* testing_interface_;
};

#endif  // PPAPI_TESTS_TEST_VAR_H_

