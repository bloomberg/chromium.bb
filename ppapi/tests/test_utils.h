// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UTILS_H_
#define PPAPI_TESTS_TEST_UTILS_H_

#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/completion_callback.h"

const PPB_Testing_Dev* GetTestingInterface();
std::string ReportError(const char* method, int32_t error);

class TestCompletionCallback {
 public:
  TestCompletionCallback();

  int32_t WaitForResult();

  operator pp::CompletionCallback() const;

 private:
  static void Handler(void* user_data, int32_t result);

  int32_t result_;
  bool post_quit_task_;
};

#endif  // PPAPI_TESTS_TEST_UTILS_H_
