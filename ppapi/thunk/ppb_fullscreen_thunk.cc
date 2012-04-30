// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
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
  const ViewData* view = enter.functions()->GetViewData(instance);
  if (!view)
    return PP_FALSE;
  return PP_FromBool(view->is_fullscreen);
}

PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->SetFullscreen(instance, fullscreen);
}

PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->GetScreenSize(instance, size);
}

const PPB_Fullscreen g_ppb_fullscreen_thunk = {
  &IsFullscreen,
  &SetFullscreen,
  &GetScreenSize
};

}  // namespace

const PPB_Fullscreen_1_0* GetPPB_Fullscreen_1_0_Thunk() {
  return &g_ppb_fullscreen_thunk;
}

}  // namespace thunk
}  // namespace ppapi
