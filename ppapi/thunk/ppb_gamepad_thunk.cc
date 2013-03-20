// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "ppapi/c/ppb_gamepad.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_gamepad_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

void SampleGamepads(PP_Instance instance, PP_GamepadsSampleData* data) {
  EnterInstanceAPI<PPB_Gamepad_API> enter(instance);
  if (enter.succeeded()) {
    enter.functions()->Sample(instance, data);
    return;
  }
  // Failure, zero out.
  memset(data, 0, sizeof(PP_GamepadsSampleData));
}

const PPB_Gamepad g_ppb_gamepad_thunk = {
  &SampleGamepads,
};

}  // namespace

const PPB_Gamepad* GetPPB_Gamepad_1_0_Thunk() {
  return &g_ppb_gamepad_thunk;
}

}  // namespace thunk
}  // namespace ppapi
