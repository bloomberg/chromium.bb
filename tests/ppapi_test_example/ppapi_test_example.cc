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
#include "ppapi/c/ppp_messaging.h"

namespace {

void MessageHandled(void* /*data*/, int32_t /*result*/) {
  printf("--- MessageHandled\n");
}

PP_Var TestFoo() {
  printf("--- TestFoo\n");
  EXPECT(pp_instance() != kInvalidInstance);
  EXPECT(pp_module() != kInvalidModule);
  return TEST_PASSED;
}

void HandleMessage(PP_Instance instance, PP_Var message) {
  printf("--- HandleMessage\n");
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "MessageHandled", MessageHandled, NULL /*user_data*/);
  PPBCore()->CallOnMainThread(10, callback, PP_OK);
}

const PPP_Messaging ppp_messaging_interface = { HandleMessage };

// TODO(polina): remove this when _Messaging proxy are checked in and
// the callback can be triggered by plugin.postMessage() instead of
// plugin.testCallback() from JavaScript.
PP_Var TestHandleMessage() {
  printf("--- TestHandleMessage\n");
  ppp_messaging_interface.HandleMessage(pp_instance(), PP_MakeUndefined());

  return TEST_PASSED;
}


}  // namespace

void SetupScriptableTests() {
  RegisterScriptableTest("testFoo", TestFoo);
  RegisterScriptableTest("testHandleMessage", TestHandleMessage);
}

void SetupPluginInterfaces() {
  RegisterPluginInterface(
      PPP_MESSAGING_INTERFACE,
      reinterpret_cast<const void*>(&ppp_messaging_interface));
}
