// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_UMA_H_
#define PAPPI_TESTS_TEST_UMA_H_

#include <string>

#include "ppapi/tests/test_case.h"

struct PPB_UMA_Private;

class TestUMA : public TestCase {
 public:
  explicit TestUMA(TestingInstance* instance) : TestCase(instance) {}

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestCount();
  std::string TestTime();
  std::string TestEnum();

  // Used by the tests that access the C API directly.
  const PPB_UMA_Private* uma_interface_;
};

#endif  // PAPPI_TESTS_TEST_UMA_H_
