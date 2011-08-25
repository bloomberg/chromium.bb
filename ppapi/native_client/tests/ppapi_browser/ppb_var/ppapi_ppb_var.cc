// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <new>
#include <vector>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/ppb_var.h"

namespace {

const int kNumThreads = 100;
const int kNumTriesPerThread = 100;

const PPB_Var* var_interface = NULL;

// Creates a bunch of new PP_Var(string), then deletes them.
// This tests locking on the dictionary that looks up PP_Vars.
void* StringCreateDeleteThreadFunc(void* thread_argument) {
  PP_Var* vars = new(std::nothrow) PP_Var[kNumTriesPerThread];
  EXPECT(vars);
  static const char* kTestString = "A test string";
  const uint32_t kTestStringLen = strlen(kTestString);
  for (int i = 0; i < kNumTriesPerThread; ++i) {
    vars[i] =
        var_interface->VarFromUtf8(pp_module(), kTestString, kTestStringLen);
  }
  for (int i = 0; i < kNumTriesPerThread; ++i) {
    var_interface->Release(vars[i]);
  }
  delete vars;
  return NULL;
}

// Tests creation and deletion.
// Spawns kNumThreads threads, each of which creates kNumTriesPerThread PP_Vars
// and then calls Release on each of them.
void TestStringVarCreateDelete() {
  static pthread_t tids[kNumThreads];
  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT(pthread_create(&tids[i],
                          NULL,
                          StringCreateDeleteThreadFunc,
                          NULL) == 0);
  }
  // Join the threads back before reporting success.
  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT(pthread_join(tids[i], NULL) == 0);
  }
  TEST_PASSED;
}

// Creates a bunch of copies of a single PP_Var(string), then deletes them.
// This tests locked reference counting.
void* StringVarRefCountThreadFunc(void* thread_argument) {
  PP_Var* test_var = reinterpret_cast<PP_Var*>(thread_argument);
  // Run the ref count up.
  for (int i = 0; i < kNumTriesPerThread; ++i) {
    var_interface->AddRef(*test_var);
  }
  // Run the ref count back down.
  for (int i = 0; i < kNumTriesPerThread; ++i) {
    var_interface->Release(*test_var);
  }
  // Run the ref count up and down in rapid succession.
  for (int i = 0; i < kNumTriesPerThread; ++i) {
    var_interface->AddRef(*test_var);
    var_interface->Release(*test_var);
  }
  return NULL;
}

// Tests reference count atomicity.
// Creates one variable and spawns kNumThreads, each of which does:
// 1) kNumTriesPerThread AddRefs
// 2) kNumTriesPerThread Releases
// 3) kNumTriesPerThread (AddRef, Release) pairs
void TestStringVarRefCount() {
  pthread_t tid[kNumThreads];
  static const char* kTestString = "A test string";
  const uint32_t kTestStringLen = strlen(kTestString);
  PP_Var test_var =
      var_interface->VarFromUtf8(pp_module(), kTestString, kTestStringLen);
  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT(pthread_create(&tid[i],
                          NULL,
                          StringVarRefCountThreadFunc,
                          &test_var) == 0);
  }
  // Join the threads back before reporting success.
  for (int i = 0; i < kNumThreads; ++i) {
    EXPECT(pthread_join(tid[i], NULL) == 0);
  }
  var_interface->Release(test_var);
  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  var_interface = PPBVar();
  EXPECT(var_interface != NULL);
  RegisterTest("TestStringVarCreateDelete", TestStringVarCreateDelete);
  RegisterTest("TestStringVarRefCount", TestStringVarRefCount);
}

void SetupPluginInterfaces() {
  // none
}
