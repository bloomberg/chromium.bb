// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsFullscreen(PP_Instance instance) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFullscreen(instance);
}

PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetFullscreen(instance, fullscreen);
}

PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) {
  EnterFunction<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->GetScreenSize(instance, size);
}

const PPB_Fullscreen_Dev g_ppb_fullscreen_thunk = {
  &IsFullscreen,
  &SetFullscreen,
  &GetScreenSize
};

}  // namespace

const PPB_Fullscreen_Dev* GetPPB_Fullscreen_Dev_Thunk() {
  return &g_ppb_fullscreen_thunk;
}

}  // namespace thunk
}  // namespace ppapi
