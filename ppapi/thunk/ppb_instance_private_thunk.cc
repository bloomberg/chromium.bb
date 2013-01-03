// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_instance_private.idl modified Thu Dec 27 10:36:33 2012.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

struct PP_Var GetWindowObject(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetWindowObject(instance);
}

struct PP_Var GetOwnerElementObject(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetOwnerElementObject(instance);
}

struct PP_Var ExecuteScript(PP_Instance instance,
                            struct PP_Var script,
                            struct PP_Var* exception) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->ExecuteScript(instance, script, exception);
}

const PPB_Instance_Private_0_1 g_ppb_instance_private_thunk_0_1 = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &ExecuteScript
};

}  // namespace

const PPB_Instance_Private_0_1* GetPPB_Instance_Private_0_1_Thunk() {
  return &g_ppb_instance_private_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
