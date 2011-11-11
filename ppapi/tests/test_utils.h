// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UTILS_H_
#define PPAPI_TESTS_TEST_UTILS_H_

#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/completion_callback.h"

// Timeout to wait for some action to complete.
extern const int kActionTimeoutMs;

const PPB_Testing_Dev* GetTestingInterface();
std::string ReportError(const char* method, int32_t error);
void PlatformSleep(int duration_ms);

class TestCompletionCallback {
 public:
  TestCompletionCallback(PP_Instance instance);
  TestCompletionCallback(PP_Instance instance, bool force_async);

  // Waits for the callback to be called and returns the
  // result. Returns immediately if the callback was previously called
  // and the result wasn't returned (i.e. each result value received
  // by the callback is returned by WaitForResult() once and only
  // once).
  int32_t WaitForResult();

  operator pp::CompletionCallback() const;

  unsigned run_count() const { return run_count_; }
  void reset_run_count() { run_count_ = 0; }

  int32_t result() const { return result_; }

 private:
  static void Handler(void* user_data, int32_t result);

  bool have_result_;
  int32_t result_;
  bool force_async_;
  bool post_quit_task_;
  unsigned run_count_;
  PP_Instance instance_;
};

#endif  // PPAPI_TESTS_TEST_UTILS_H_
