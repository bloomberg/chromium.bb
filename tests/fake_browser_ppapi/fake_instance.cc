/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include "native_client/tests/fake_browser_ppapi/fake_instance.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"
#include "ppapi/c/ppb_instance.h"

using fake_browser_ppapi::DebugPrintf;

namespace fake_browser_ppapi {

namespace {

static Instance* GetInstancePointer(PP_Instance instance) {
  return reinterpret_cast<Instance*>(static_cast<uintptr_t>(instance));
}

static PP_Var GetWindowObject(PP_Instance instance) {
  return GetInstancePointer(instance)->GetWindowObject();
}

static PP_Var GetOwnerElementObject(PP_Instance instance) {
  return GetInstancePointer(instance)->GetOwnerElementObject();
}

static PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) {
  return
      static_cast<PP_Bool>(GetInstancePointer(instance)->BindGraphics(device));
}

static PP_Bool IsFullFrame(PP_Instance instance) {
  return static_cast<PP_Bool>(GetInstancePointer(instance)->IsFullFrame());
}

static PP_Var ExecuteScript(PP_Instance instance,
                            PP_Var script,
                            PP_Var* exception) {
  return GetInstancePointer(instance)->ExecuteScript(script, exception);
}

}  // namespace

PP_Var Instance::GetWindowObject() {
  DebugPrintf("Instance::GetWindowObject: instance=%p\n",
              reinterpret_cast<void*>(this));
  return window_->FakeWindowObject();
}

PP_Var Instance::GetOwnerElementObject() {
  DebugPrintf("Instance::GetOwnerElementObject: instance=%p\n",
              reinterpret_cast<void*>(this));
  NACL_UNIMPLEMENTED();
  return PP_MakeUndefined();
}

bool Instance::BindGraphics(PP_Resource device) {
  DebugPrintf("Instance::BindGraphicsDeviceContext: instance=%p"
              ", device=%"NACL_PRIu64"\n",
              reinterpret_cast<void*>(this), device);
  NACL_UNIMPLEMENTED();
  return false;
}

bool Instance::IsFullFrame() {
  DebugPrintf("Instance::IsFullFrame: instance=%p\n",
              reinterpret_cast<void*>(this));
  NACL_UNIMPLEMENTED();
  return false;
}

PP_Var Instance::ExecuteScript(PP_Var script,
                               PP_Var* exception) {
  DebugPrintf("Instance::ExecuteScript: instance=%p\n",
              reinterpret_cast<void*>(this));
  NACL_UNIMPLEMENTED();
  UNREFERENCED_PARAMETER(script);
  UNREFERENCED_PARAMETER(exception);
  return PP_MakeUndefined();
}

const PPB_Instance* Instance::GetInterface() {
  static const PPB_Instance instance_interface = {
    fake_browser_ppapi::GetWindowObject,
    fake_browser_ppapi::GetOwnerElementObject,
    fake_browser_ppapi::BindGraphics,
    fake_browser_ppapi::IsFullFrame,
    fake_browser_ppapi::ExecuteScript
  };
  return &instance_interface;
}

}  // namespace fake_browser_ppapi
