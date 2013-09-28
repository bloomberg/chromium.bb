// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_OUTPUT_PROTECTION_PRIVATE_H_
#define PPAPI_TESTS_TEST_OUTPUT_PROTECTION_PRIVATE_H_

#include <string>

#include "ppapi/cpp/private/output_protection_private.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

class TestOutputProtectionPrivate: public TestCase {
 public:
  explicit TestOutputProtectionPrivate(TestingInstance* instance);

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string TestQueryStatus();
  std::string TestEnableProtection();

  const PPB_OutputProtection_Private* output_protection_interface_;
};

#endif  // PPAPI_TESTS_TEST_OUTPUT_PROTECTION_PRIVATE_H_
