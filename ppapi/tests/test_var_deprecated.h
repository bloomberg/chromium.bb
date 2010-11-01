// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TEST_TEST_VAR_DEPRECATED_H_
#define PPAPI_TEST_TEST_VAR_DEPRECATED_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_Testing_Dev;
struct PPB_Var_Deprecated;

class TestVarDeprecated : public TestCase {
 public:
  explicit TestVarDeprecated(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestBasicString();
  std::string TestInvalidAndEmpty();
  std::string TestInvalidUtf8();
  std::string TestNullInputInUtf8Conversion();
  std::string TestValidUtf8();
  std::string TestUtf8WithEmbeddedNulls();
  std::string TestVarToUtf8ForWrongType();
  std::string TestHasPropertyAndMethod();

  // Used by the tests that access the C API directly.
  const PPB_Var_Deprecated* var_interface_;
  const PPB_Testing_Dev* testing_interface_;
};

#endif  // PPAPI_TEST_TEST_VAR_DEPRECATED_H_

