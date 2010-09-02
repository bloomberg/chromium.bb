/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_core.h"
#include <stdio.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi_proxy {

namespace {

static void AddRefResource(PP_Resource resource) {
  DebugPrintf("PluginCore::AddRefResource: resource=%"NACL_PRIu64"\n",
              resource);
  NACL_UNIMPLEMENTED();
  // A naive implementation of the reference counting would just do an
  // rpc to the plugin every time.  Something smarter might create a local
  // object for each PP_Resource and only do the rpc when the count drops
  // to zero, etc.
}

static void ReleaseResource(PP_Resource resource) {
  DebugPrintf("PluginCore::ReleaseResource: resource=%"NACL_PRIu64"\n",
              resource);
  NACL_UNIMPLEMENTED();
}

static void* MemAlloc(size_t num_bytes) {
  DebugPrintf("PluginCore::MemAlloc: num_bytes=%"NACL_PRIuS"\n", num_bytes);
  return malloc(num_bytes);
}

static void MemFree(void* ptr) {
  DebugPrintf("PluginCore::MemFree: ptr=%p\n", ptr);
  free(ptr);
}

static PP_Time GetTime() {
  DebugPrintf("PluginCore::GetTime\n");
  NACL_UNIMPLEMENTED();
  return static_cast<PP_Time>(0.0);
}

static void CallOnMainThread(int32_t delay_in_milliseconds,
                             PP_CompletionCallback callback,
                             int32_t result) {
  UNREFERENCED_PARAMETER(callback);
  DebugPrintf("PluginCore::CallOnMainThread: delay=%" NACL_PRIu32
              ", result=%" NACL_PRIu32 "\n",
              delay_in_milliseconds,
              result);
  NACL_UNIMPLEMENTED();
  // See how NPN_PluginThreadAsyncCall is implemented in npruntime.
}

static bool IsMainThread() {
  NACL_UNIMPLEMENTED();
}

}  // namespace

const void* PluginCore::GetInterface() {
  static const PPB_Core intf = {
    AddRefResource,
    ReleaseResource,
    MemAlloc,
    MemFree,
    GetTime,
    CallOnMainThread,
    IsMainThread
  };
  return reinterpret_cast<const void*>(&intf);
}

}  // namespace ppapi_proxy
