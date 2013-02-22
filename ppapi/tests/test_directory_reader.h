// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_DIRECTORY_READER_H_
#define PAPPI_TESTS_TEST_DIRECTORY_READER_H_

#include <string>

#include "ppapi/tests/test_case.h"

namespace pp {
class FileRef;
}

class TestDirectoryReader : public TestCase {
 public:
  explicit TestDirectoryReader(TestingInstance* instance)
      : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  int32_t DeleteDirectoryRecursively(pp::FileRef*);

  std::string TestReadEntries();
};

#endif  // PAPPI_TESTS_TEST_DIRECTORY_READER_H_
