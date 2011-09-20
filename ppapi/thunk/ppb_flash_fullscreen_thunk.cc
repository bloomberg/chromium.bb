// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsFullscreen(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->FlashIsFullscreen(instance);
}

PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->FlashSetFullscreen(instance, fullscreen);
}

PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->FlashGetScreenSize(instance, size);
}

const PPB_FlashFullscreen g_ppb_flash_fullscreen_thunk = {
  &IsFullscreen,
  &SetFullscreen,
  &GetScreenSize
};

}  // namespace

const PPB_FlashFullscreen* GetPPB_FlashFullscreen_Thunk() {
  return &g_ppb_flash_fullscreen_thunk;
}

}  // namespace thunk
}  // namespace ppapi
