// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_CHAR_SET_H_
#define PPAPI_TESTS_TEST_CHAR_SET_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

struct PPB_CharSet_Dev;

class TestCharSet : public TestCase {
 public:
  TestCharSet(TestingInstance* instance);

  // TestCase implementation.

  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestUTF16ToCharSet();
  std::string TestCharSetToUTF16();
  std::string TestGetDefaultCharSet();

  // Converts the given UTF-8 string to a NON-NULL TERMINATED UTF-16 string
  // stored in the given vector.
  std::vector<uint16_t> UTF8ToUTF16(const std::string& utf8);

  const struct PPB_CharSet_Dev* char_set_interface_;
};

#endif  // PPAPI_TESTS_TEST_CHAR_SET_H_
