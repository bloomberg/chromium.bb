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

static PP_Var GetWindowObject(PP_Instance instance) {
  return GetInstance(instance)->GetWindowObject();
}

static PP_Var GetOwnerElementObject(PP_Instance instance) {
  return GetInstance(instance)->GetOwnerElementObject();
}

static PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) {
  return static_cast<PP_Bool>(GetInstance(instance)->BindGraphics(device));
}

static PP_Bool IsFullFrame(PP_Instance instance) {
  return static_cast<PP_Bool>(GetInstance(instance)->IsFullFrame());
}

static PP_Var ExecuteScript(PP_Instance instance,
                            PP_Var script,
                            PP_Var* exception) {
  return GetInstance(instance)->ExecuteScript(script, exception);
}

}  // namespace

Instance Instance::kInvalidInstance;

PP_Var Instance::GetWindowObject() {
  DebugPrintf("Instance::GetWindowObject: instance=%"NACL_PRId32"\n",
              instance_id_);
  if (window_)
    return window_->FakeWindowObject();
  else
    return PP_MakeUndefined();
}

PP_Var Instance::GetOwnerElementObject() {
  DebugPrintf("Instance::GetOwnerElementObject: instance=%"NACL_PRId32"\n",
              instance_id_);
  NACL_UNIMPLEMENTED();
  return PP_MakeUndefined();
}

bool Instance::BindGraphics(PP_Resource device) {
  DebugPrintf("Instance::BindGraphicsDeviceContext: instance=%"NACL_PRId32"\n",
              ", device=%"NACL_PRIu32"\n",
              instance_id_,
              device);
  NACL_UNIMPLEMENTED();
  return false;
}

bool Instance::IsFullFrame() {
  DebugPrintf("Instance::IsFullFrame: instance=%"NACL_PRId32"\n",
              instance_id_);
  NACL_UNIMPLEMENTED();
  return false;
}

PP_Var Instance::ExecuteScript(PP_Var script,
                               PP_Var* exception) {
  DebugPrintf("Instance::ExecuteScript: instance=%"NACL_PRId32"\n",
              instance_id_);
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
