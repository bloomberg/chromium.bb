// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_MOJO_TEST_MOJO_H_
#define PPAPI_TESTS_MOJO_TEST_MOJO_H_

#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

class TestMojo : public TestCase {
 public:
  explicit TestMojo(TestingInstance* instance);
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& test_filter);
 private:
  std::string TestCreateMessagePipe();
};

#endif  // PPAPI_TESTS_MOJO_TEST_MOJO_H_
