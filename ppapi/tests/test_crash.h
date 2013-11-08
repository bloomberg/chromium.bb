// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CRASH_H_
#define PPAPI_TESTS_TEST_CRASH_H_

#include <string>

#include "ppapi/tests/test_case.h"

// This test crashes at different points, allowing us to make sure the browser
// detects the crashes.
class TestCrash : public TestCase {
 public:
  explicit TestCrash(TestingInstance* instance);

 private:
  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

  std::string TestCrashInCallOnMainThread();
};

#endif  // PPAPI_TESTS_TEST_CRASH_H_
