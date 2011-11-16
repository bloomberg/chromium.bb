// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CURSOR_CONTROL_H_
#define PPAPI_TESTS_TEST_CURSOR_CONTROL_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

struct PPB_CursorControl_Dev;

class TestCursorControl : public TestCase {
 public:
  TestCursorControl(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestSetCursor();

  const struct PPB_CursorControl_Dev* cursor_control_interface_;
};

#endif  // PPAPI_TESTS_TEST_CURSOR_CONTROL_H_
