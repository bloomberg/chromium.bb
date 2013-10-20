// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_PLATFORM_VERIFICATION_PRIVATE_H_
#define PAPPI_TESTS_TEST_PLATFORM_VERIFICATION_PRIVATE_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestPlatformVerificationPrivate : public TestCase {
 public:
  explicit TestPlatformVerificationPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestChallengePlatform();
};

#endif  // PAPPI_TESTS_TEST_PLATFORM_VERIFICATION_PRIVATE_H_
