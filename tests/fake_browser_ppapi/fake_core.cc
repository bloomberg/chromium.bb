/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_core.h"
#include <stdio.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

using fake_browser_ppapi::DebugPrintf;

namespace {

// The plugin does reference counting indirectly by using PPAPI C++ layer.
// The assumption is that PPAPI tests cover reference counting, so we don't
// need to test it here. Instead every resource will be registered with the
// host on creation and deallocated in its destructor.

static void AddRefResource(PP_Resource resource) {
  DebugPrintf("Core::AddRefResource: resource=%"NACL_PRIu64"\n", resource);
}

static void ReleaseResource(PP_Resource resource) {
  DebugPrintf("Core::ReleaseResource: resource=%"NACL_PRIu64"\n", resource);
}

static void* MemAlloc(uint32_t num_bytes) {
  DebugPrintf("Core::MemAlloc: num_bytes=%"NACL_PRIuS"\n", num_bytes);
  return malloc(num_bytes);
}

static void MemFree(void* ptr) {
  DebugPrintf("Core::MemFree: ptr=%p\n", ptr);
  free(ptr);
}

static PP_Time GetTime() {
  DebugPrintf("Core::GetTime\n");
  static double time = 0.0;
  // TODO(sehr): Do we need a real time here?
  time += 1.0;
  return static_cast<PP_Time>(time);
}

static PP_TimeTicks GetTimeTicks() {
  DebugPrintf("Core::GetTime\n");
  static double time = 0.0;
  // TODO(sehr): Do we need a real time here?
  time += 1.0;
  return static_cast<PP_Time>(time);
}

static void CallOnMainThread(int32_t delay_in_milliseconds,
                             PP_CompletionCallback callback,
                             int32_t result) {
  UNREFERENCED_PARAMETER(callback);
  DebugPrintf("Core::CallOnMainThread: delay=%" NACL_PRIu32
              ", result=%" NACL_PRIu32 "\n",
              delay_in_milliseconds,
              result);
  NACL_UNIMPLEMENTED();
}

static PP_Bool IsMainThread() {
  DebugPrintf("Core::IsMainThread: always true\n");
  return PP_TRUE;
}

}  // namespace

namespace fake_browser_ppapi {

const PPB_Core* Core::GetInterface() {
  static const PPB_Core core_interface = {
    AddRefResource,
    ReleaseResource,
    MemAlloc,
    MemFree,
    GetTime,
    GetTimeTicks,
    CallOnMainThread,
    IsMainThread
  };
  return &core_interface;
}

}  // namespace fake_browser_ppapi
