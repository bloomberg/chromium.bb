// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_utils.h"

#include <stdio.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/module.h"

const PPB_Testing_Dev* GetTestingInterface() {
  static const PPB_Testing_Dev* g_testing_interface =
      reinterpret_cast<PPB_Testing_Dev const*>(
          pp::Module::Get()->GetBrowserInterface(PPB_TESTING_DEV_INTERFACE));
  return g_testing_interface;
}

std::string ReportError(const char* method, int32_t error) {
  char error_as_string[12];
  sprintf(error_as_string, "%d", error);
  std::string result = method + std::string(" failed with error: ") +
      error_as_string;
  if (error == PP_ERROR_NOSPACE)
    result += ". Did you run the test with --unlimited-quota-for-files?";
  return result;
}

TestCompletionCallback::TestCompletionCallback(PP_Instance instance)
    : result_(PP_ERROR_WOULDBLOCK),
      post_quit_task_(false),
      run_count_(0),
      instance_(instance) {
}

int32_t TestCompletionCallback::WaitForResult() {
  result_ = PP_ERROR_WOULDBLOCK;  // Reset
  post_quit_task_ = true;
  GetTestingInterface()->RunMessageLoop(instance_);
  return result_;
}

TestCompletionCallback::operator pp::CompletionCallback() const {
  return pp::CompletionCallback(&TestCompletionCallback::Handler,
                                const_cast<TestCompletionCallback*>(this));
}

// static
void TestCompletionCallback::Handler(void* user_data, int32_t result) {
  TestCompletionCallback* callback =
      static_cast<TestCompletionCallback*>(user_data);
  callback->result_ = result;
  callback->run_count_++;
  if (callback->post_quit_task_) {
    callback->post_quit_task_ = false;
    GetTestingInterface()->QuitMessageLoop(callback->instance_);
  }
}
