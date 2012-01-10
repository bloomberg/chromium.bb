// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_gamepad_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

void SampleGamepads(PP_Instance instance, PP_GamepadsData_Dev* data) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return;
  enter.functions()->SampleGamepads(instance, data);
}

const PPB_Gamepad_Dev g_ppb_gamepad_thunk = {
  &SampleGamepads,
};

}  // namespace

const PPB_Gamepad_Dev* GetPPB_Gamepad_Dev_0_1_Thunk() {
  return &g_ppb_gamepad_thunk;
}

}  // namespace thunk
}  // namespace ppapi
