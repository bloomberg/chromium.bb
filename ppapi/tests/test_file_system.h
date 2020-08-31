// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FILE_SYSTEM_H_
#define PAPPI_TESTS_TEST_FILE_SYSTEM_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestFileSystem : public TestCase {
 public:
  explicit TestFileSystem(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestOpen();
  std::string TestMultipleOpens();
  std::string TestResourceConversion();
};

#endif  // PAPPI_TESTS_TEST_FILE_SYSTEM_H_

