// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_PRINTING_H_
#define PPAPI_TESTS_TEST_PRINTING_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestPrinting : public TestCase {
 public:
  explicit TestPrinting(TestingInstance* instance);

  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

 private:
  // Tests.
  std::string TestGetDefaultPrintSettings();
};

#endif  // PPAPI_TESTS_TEST_PRINTING_H_
