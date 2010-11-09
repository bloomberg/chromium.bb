/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/plugin_instance.h"

#include <stdio.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_instance.h"

namespace ppapi_proxy {

namespace {

static PluginInstance* GetInstancePointer(PP_Instance instance) {
  return reinterpret_cast<PluginInstance*>(static_cast<uintptr_t>(instance));
}

static PP_Var GetWindowObjectThunk(PP_Instance instance) {
  DebugPrintf("PluginInstance::GetWindowObject: instance=%" NACL_PRIx64"\n",
              instance);
  return GetInstancePointer(instance)->GetWindowObject();
}

static PP_Var GetOwnerElementObjectThunk(PP_Instance instance) {
  DebugPrintf("PluginInstance::GetOwnerElementObject: instance=%"
              NACL_PRIx64 "\n",
              instance);
  return GetInstancePointer(instance)->GetOwnerElementObject();
}

static PP_Bool BindGraphicsThunk(PP_Instance instance,
                              PP_Resource device) {
  DebugPrintf("PluginInstance::BindGraphicsDeviceContext: instance=%"
              NACL_PRIx64 ", device=%" NACL_PRIu64 "\n",
              instance, device);
  return static_cast<PP_Bool>
      (GetInstancePointer(instance)->BindGraphics(device));
}

static PP_Bool IsFullFrameThunk(PP_Instance instance) {
  DebugPrintf("PluginInstance::IsFullFrame: instance=%" NACL_PRIx64 "\n",
              instance);
  return static_cast<PP_Bool>(GetInstancePointer(instance)->IsFullFrame());
}

static PP_Var ExecuteScriptThunk(PP_Instance instance,
                                 PP_Var script,
                                 PP_Var* exception) {
  DebugPrintf("PluginInstance::ExecuteScript: instance=%" NACL_PRIx64"\n",
              instance);
  return GetInstancePointer(instance)->ExecuteScript(script, exception);
}

}  // namespace

const PPB_Instance* PluginInstance::GetInterface() {
  static const PPB_Instance intf = {
    GetWindowObjectThunk,
    GetOwnerElementObjectThunk,
    BindGraphicsThunk,
    IsFullFrameThunk,
    ExecuteScriptThunk
  };
  return &intf;
}

}  // namespace ppapi_proxy
