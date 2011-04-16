// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_url_request_info.h"

namespace {

void EmptyCompletionCallback(void* /*data*/, int32_t /*result*/) {
}

// Calls PPB_Core::CallOnMainThread(). To be invoked off the main thread.
void* InvokeCallOnMainThread(void* thread_argument) {
  PPB_Core* ppb_core = reinterpret_cast<PPB_Core*>(thread_argument);
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "CallOnMainThreadCallback_FromNonMainThread",
      EmptyCompletionCallback,
      NULL /*user_data*/);
  ppb_core->CallOnMainThread(0 /*delay*/, callback, PP_OK);
  return NULL;
}

// Calls PPB_Core::IsMainThread(). To be invoked off the main thread.
void* InvokeIsMainThread(void* thread_argument) {
  PPB_Core* ppb_core = reinterpret_cast<PPB_Core*>(thread_argument);
  return reinterpret_cast<void*>(ppb_core->IsMainThread());
}

// Tests PPB_Core::GetTime().
PP_Var TestGetTime() {
  PP_Time time1 = PPBCore()->GetTime();
  EXPECT(time1 > 0);

  usleep(100000);  // 0.1 second

  PP_Time time2 = PPBCore()->GetTime();
  EXPECT(time2 > time1);

  return TEST_PASSED;
}

// Tests PPB_Core::GetTimeTicks().
PP_Var TestGetTimeTicks() {
  PP_TimeTicks time_ticks1 = PPBCore()->GetTimeTicks();
  EXPECT(time_ticks1 > 0);

  usleep(100000);  // 0.1 second

  PP_TimeTicks time_ticks2 = PPBCore()->GetTimeTicks();
  EXPECT(time_ticks2 > time_ticks1);

  return TEST_PASSED;
}

// Tests PPB_Core::CallOnMainThread() from the main thread.
PP_Var TestCallOnMainThreadFromMainThread() {
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "CallOnMainThreadCallback_FromMainThread",
      EmptyCompletionCallback,
      NULL /*user_data*/);
  PPBCore()->CallOnMainThread(0 /*delay*/, callback, PP_OK);

  return TEST_PASSED;
}

// Tests PPB_Core::CallOnMainThread from non-main thread.
PP_Var TestCallOnMainThreadFromNonMainThread() {
  pthread_t tid;
  void* ppb_core = reinterpret_cast<void*>(const_cast<PPB_Core*>(PPBCore()));
  CHECK(pthread_create(&tid, NULL, InvokeCallOnMainThread, ppb_core) == 0);
  // Use a non-joined thread.  This is a more useful test than
  // joining the thread: we want to test CallOnMainThread() when it
  // is called concurrently with the main thread.
  CHECK(pthread_detach(tid) == 0);

  return TEST_PASSED;
}

// Tests PPB_Core::IsMainThread() from the main thread.
PP_Var TestIsMainThreadFromMainThread() {
  EXPECT(PPBCore()->IsMainThread() == PP_TRUE);
  return TEST_PASSED;
}

// Tests PPB_Core::IsMainThread() from non-main thread.
PP_Var TestIsMainThreadFromNonMainThread() {
  pthread_t tid;
  void* thread_result;
  void* ppb_core = reinterpret_cast<void*>(const_cast<PPB_Core*>(PPBCore()));
  CHECK(pthread_create(&tid, NULL, InvokeIsMainThread, ppb_core) == 0);
  CHECK(pthread_join(tid, &thread_result) == 0);
  EXPECT(reinterpret_cast<int>(thread_result) == PP_FALSE);

  return TEST_PASSED;
}

// Tests PPB_Core::MemAlloc() and PPB_Core::MemFree().
PP_Var TestMemAllocAndMemFree() {
  void* mem = NULL;
  mem = PPBCore()->MemAlloc(100);  // No signficance to using 100
  EXPECT(mem != NULL);
  memset(mem, '-', 5);
  EXPECT(memcmp(mem, "-----", 5) == 0);
  PPBCore()->MemFree(mem);

  return TEST_PASSED;
}

// Tests PPB_Core::AddRefResource() and PPB_Core::ReleaseResource() with
// a valid resource.
PP_Var TestAddRefAndReleaseResource() {
  PP_Resource valid_resource = PPBURLRequestInfo()->Create(pp_instance());
  EXPECT(valid_resource != kInvalidResource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) == PP_TRUE);

  // Adjusting ref count should not delete the resource.
  for (size_t j = 0; j < 100; ++j) PPBCore()->AddRefResource(valid_resource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) == PP_TRUE);
  for (size_t j = 0; j < 100; ++j) PPBCore()->ReleaseResource(valid_resource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) == PP_TRUE);

  // Releasing the ref count from Create() must delete the resource.
  PPBCore()->ReleaseResource(valid_resource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) != PP_TRUE);

  return TEST_PASSED;
}

// Tests PPB_Core::AddRefResource() and PPB_Core::ReleaseResource() with
// an invalid resource.
PP_Var TestAddRefAndReleaseInvalidResource() {
  for (size_t j = 0; j < 100; ++j) {
    PPBCore()->AddRefResource(kInvalidResource);
    PPBCore()->ReleaseResource(kInvalidResource);
  }

  return TEST_PASSED;
}

}  // namespace

void SetupScriptableTests() {
  RegisterScriptableTest("testGetTime", TestGetTime);
  RegisterScriptableTest("testGetTimeTicks", TestGetTimeTicks);
  RegisterScriptableTest("testIsMainThread_FromMainThread",
                         TestIsMainThreadFromMainThread);
  RegisterScriptableTest("testIsMainThread_FromNonMainThread",
                         TestIsMainThreadFromNonMainThread);
  RegisterScriptableTest("testMemAllocAndMemFree",
                         TestMemAllocAndMemFree);
  RegisterScriptableTest("testAddRefAndReleaseResource",
                         TestAddRefAndReleaseResource);
  RegisterScriptableTest("testAddRefAndReleaseInvalidResource",
                         TestAddRefAndReleaseInvalidResource);
  RegisterScriptableTest("testCallOnMainThread_FromMainThread",
                         TestCallOnMainThreadFromMainThread);
  RegisterScriptableTest("testCallOnMainThread_FromNonMainThread",
                         TestCallOnMainThreadFromNonMainThread);
}

void SetupPluginInterfaces() {
  // none
}
