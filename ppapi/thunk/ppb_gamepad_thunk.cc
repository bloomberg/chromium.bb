// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_gamepad.idl modified Thu Mar 28 15:36:32 2013.

#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_gamepad_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

void Sample(PP_Instance instance, struct PP_GamepadsSampleData* data) {
  EnterInstanceAPI<PPB_Gamepad_API> enter(instance);
  if (enter.succeeded()) {
    enter.functions()->Sample(instance, data);
    return;
  }
  memset(data, 0, sizeof(struct PP_GamepadsSampleData));
}

const PPB_Gamepad_1_0 g_ppb_gamepad_thunk_1_0 = {
  &Sample
};

}  // namespace

const PPB_Gamepad_1_0* GetPPB_Gamepad_1_0_Thunk() {
  return &g_ppb_gamepad_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
