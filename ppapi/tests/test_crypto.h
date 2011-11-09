// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CRYPTO_H_
#define PPAPI_TESTS_TEST_CRYPTO_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

struct PPB_Crypto_Dev;

class TestCrypto : public TestCase {
 public:
  TestCrypto(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();

 private:
  std::string TestGetRandomBytes();

  const struct PPB_Crypto_Dev* crypto_interface_;
};

#endif  // PPAPI_TESTS_TEST_CRYPTO_H_
