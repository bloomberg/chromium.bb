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
#include "ppapi/c/dev/ppb_memory_dev.h"

namespace {

// Tests PPB_Memory_Dev::MemAlloc() and PPB_Core::MemFree().
void TestMemAllocAndMemFree() {
  void* mem = NULL;
  mem = PPBMemoryDev()->MemAlloc(100);  // No signficance to using 100
  EXPECT(mem != NULL);
  memset(mem, '-', 5);
  EXPECT(memcmp(mem, "-----", 5) == 0);
  PPBMemoryDev()->MemFree(mem);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestMemAllocAndMemFree", TestMemAllocAndMemFree);
}

void SetupPluginInterfaces() {
  // none
}
