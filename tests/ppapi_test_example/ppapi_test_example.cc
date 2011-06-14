// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"

namespace {

void Callback(void* /*data*/, int32_t /*result*/) {
  printf("--- Callback\n");
}

// Below are the before & after versions of sample test cases demonstrating
// how to transition from synchronous scripting to postMessage as the test
// driving mechanism.

// Before.
PP_Var TestSimpleSync() {
  printf("--- TestSimpleSync\n");
  EXPECT(pp_instance() != kInvalidInstance);
  EXPECT(pp_module() != kInvalidModule);
  return TEST_PASSED;
}

// After.
void TestSimpleAsync() {
  printf("--- TestSimpleAsync\n");
  EXPECT_ASYNC(pp_instance() != kInvalidInstance);
  EXPECT_ASYNC(pp_module() != kInvalidModule);
  TEST_PASSED_ASYNC;
}

// Before.
PP_Var TestCallbackSync() {
  printf("--- TestCallbackSync\n");
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "CallbackSync", Callback, NULL /*user_data*/);
  PPBCore()->CallOnMainThread(10, callback, PP_OK);
  return TEST_PASSED;
}

// After.
void TestCallbackAsync() {
  printf("--- TestCallbackAsync\n");
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "CallbackAsync", Callback, NULL /*user_data*/);
  PPBCore()->CallOnMainThread(10, callback, PP_OK);
  TEST_PASSED_ASYNC;
}

}  // namespace

void SetupTests() {
  RegisterScriptableTest("TestSimpleSync", TestSimpleSync);
  RegisterScriptableTest("TestCallbackSync", TestCallbackSync);
  RegisterTest("TestSimpleAsync", TestSimpleAsync);
  RegisterTest("TestCallbackAsync", TestCallbackAsync);
}

void SetupPluginInterfaces() {
  // TODO(polina): add an example of PPP interface testing
}
