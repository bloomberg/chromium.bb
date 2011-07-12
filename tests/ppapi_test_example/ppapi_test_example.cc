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

void TestSimple() {
  printf("--- TestSimple\n");
  EXPECT(pp_instance() != kInvalidInstance);
  EXPECT(pp_module() != kInvalidModule);
  TEST_PASSED;
}

void TestCallback() {
  printf("--- TestCallback\n");
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "Callback", Callback, NULL /*user_data*/);
  PPBCore()->CallOnMainThread(10, callback, PP_OK);
  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestSimple", TestSimple);
  RegisterTest("TestCallback", TestCallback);
}

void SetupPluginInterfaces() {
  // TODO(polina): add an example of PPP interface testing
}
