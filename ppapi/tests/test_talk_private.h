// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_TALK_PRIVATE_H_
#define PAPPI_TESTS_TEST_TALK_PRIVATE_H_

#include <string>

#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/tests/test_case.h"

class TestTalkPrivate : public TestCase {
 public:
  explicit TestTalkPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestGetPermission();

  const PPB_Talk_Private_1_0* talk_private_interface_1;
};

#endif  // PPAPI_TESTS_TEST_TALK_PRIVATE_H_
