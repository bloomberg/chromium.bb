/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_core.h"
#include <stdio.h>
#include <map>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource_tracker.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/cpp/common.h"

using ppapi_proxy::DebugPrintf;

// All of the methods here are invoked from the plugin's main (UI) thread,
// so no locking is done.

namespace {
#ifdef __native_client__
__thread bool is_main_thread = false;
bool main_thread_marked = false;
#endif  // __native_client__

// Increment the reference count for a specified resource.  This only takes
// care of the plugin's reference count - the Resource should obtain the
// browser reference when it stores the browser's Resource id. When the
// Resource's reference count goes to zero, the destructor should make sure
// the browser reference is returned.
void AddRefResource(PP_Resource resource) {
  DebugPrintf("PluginCore::AddRefResource: resource=%"NACL_PRIu64"\n",
              resource);
  if (ppapi_proxy::PluginResourceTracker::Get()->AddRefResource(resource))
    DebugPrintf("Warning: AddRefResource()ing a nonexistent resource");
}

void ReleaseResource(PP_Resource resource) {
  DebugPrintf("PluginCore::ReleaseResource: resource=%"NACL_PRIu64"\n",
              resource);
  if (ppapi_proxy::PluginResourceTracker::Get()->UnrefResource(resource))
    DebugPrintf("Warning: ReleaseRefResource()ing a nonexistent resource");
}

void* MemAlloc(size_t num_bytes) {
  DebugPrintf("PluginCore::MemAlloc: num_bytes=%"NACL_PRIuS"\n", num_bytes);
  return malloc(num_bytes);
}

void MemFree(void* ptr) {
  DebugPrintf("PluginCore::MemFree: ptr=%p\n", ptr);
  free(ptr);
}

PP_TimeTicks GetTime() {
  DebugPrintf("PluginCore::GetTime\n");
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  double time;
  NaClSrpcError retval = PpbCoreRpcClient::PPB_Core_GetTime(channel, &time);
  if (retval != NACL_SRPC_RESULT_OK) {
    return static_cast<PP_Time>(-1.0);
  } else {
    return static_cast<PP_Time>(time);
  }
}

PP_TimeTicks GetTimeTicks() {
  DebugPrintf("PluginCore::GetTime\n");
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  double time;
  // TODO(sehr): implement time ticks.
  NaClSrpcError retval = PpbCoreRpcClient::PPB_Core_GetTime(channel, &time);
  if (retval != NACL_SRPC_RESULT_OK) {
    return static_cast<PP_TimeTicks>(-1.0);
  } else {
    return static_cast<PP_TimeTicks>(time);
  }
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

static PP_Bool IsMainThread() {
  DebugPrintf("PluginCore::IsMainThread\n");
#ifdef __native_client__
  return pp::BoolToPPBool(is_main_thread);
#else
  // TODO(polina): implement this for trusted clients if needed.
  NACL_UNIMPLEMENTED();
  return PP_TRUE;
#endif  // __native_client__
}

}  // namespace

namespace ppapi_proxy {

const void* PluginCore::GetInterface() {
  static const PPB_Core intf = {
    AddRefResource,
    ReleaseResource,
    MemAlloc,
    MemFree,
    GetTime,
    GetTimeTicks,
    CallOnMainThread,
    IsMainThread
  };
  return reinterpret_cast<const void*>(&intf);
}

void PluginCore::MarkMainThread() {
#ifdef __native_client__
  if (main_thread_marked) {
    // A main thread was already designated.  Fail.
    NACL_NOTREACHED();
  } else {
    is_main_thread = true;
    // Setting this once works because the main thread will call this function
    // before calling any pthread_creates.  Hence the result is already
    // published before other threads might attempt to call it.
    main_thread_marked = true;
  }
#endif  // __native_client__
}


}  // namespace ppapi_proxy
