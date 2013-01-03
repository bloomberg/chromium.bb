// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_instance.idl modified Thu Dec 27 10:36:33 2012.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->BindGraphics(instance, device);
}

PP_Bool IsFullFrame(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFullFrame(instance);
}

const PPB_Instance_1_0 g_ppb_instance_thunk_1_0 = {
  &BindGraphics,
  &IsFullFrame
};

}  // namespace

const PPB_Instance_1_0* GetPPB_Instance_1_0_Thunk() {
  return &g_ppb_instance_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
