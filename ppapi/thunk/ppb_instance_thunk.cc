// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Var GetWindowObject(PP_Instance instance) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetWindowObject(instance);
}

PP_Var GetOwnerElementObject(PP_Instance instance) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetOwnerElementObject(instance);
}

PP_Bool BindGraphics(PP_Instance instance, PP_Resource graphics_id) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->BindGraphics(instance, graphics_id);
}

PP_Bool IsFullFrame(PP_Instance instance) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFullFrame(instance);
}

PP_Var ExecuteScript(PP_Instance instance,
                     PP_Var script,
                     PP_Var* exception) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->ExecuteScript(instance, script, exception);
}

const PPB_Instance_1_0 g_ppb_instance_1_0_thunk = {
  &BindGraphics,
  &IsFullFrame
};

const PPB_Instance_Private g_ppb_instance_private_thunk = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &ExecuteScript
};

}  // namespace

const PPB_Instance_1_0* GetPPB_Instance_1_0_Thunk() {
  return &g_ppb_instance_1_0_thunk;
}
const PPB_Instance_Private* GetPPB_Instance_Private_Thunk() {
  return &g_ppb_instance_private_thunk;
}

}  // namespace thunk
}  // namespace ppapi
