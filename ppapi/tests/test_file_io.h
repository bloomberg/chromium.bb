// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FILE_IO_H_
#define PAPPI_TESTS_TEST_FILE_IO_H_

#include <string>

#include "ppapi/tests/test_case.h"

class TestFileIO : public TestCase {
 public:
  explicit TestFileIO(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestOpen();
  std::string TestReadWriteSetLength();
  std::string TestTouchQuery();
};

#endif  // PAPPI_TESTS_TEST_FILE_IO_H_
