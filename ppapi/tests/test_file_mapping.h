// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FILE_MAPPING_H_
#define PAPPI_TESTS_TEST_FILE_MAPPING_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_FileMapping_0_1;

class TestFileMapping : public TestCase {
 public:
  explicit TestFileMapping(TestingInstance* instance)
      : TestCase(instance),
        file_mapping_if_(NULL) {
  }
  virtual ~TestFileMapping() {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string MapAndCheckResults(uint32_t prot, uint32_t flags);

  std::string TestBadParameters();
  std::string TestMap();
  std::string TestPartialRegions();

  // TODO(dmichael): Use unversioned struct when it goes stable.
  const PPB_FileMapping_0_1* file_mapping_if_;
};

#endif  // PAPPI_TESTS_TEST_FILE_MAPPING_H_
