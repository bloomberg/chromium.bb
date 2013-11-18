// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From dev/ppb_graphics_2d_dev.idl modified Fri Nov  8 13:58:50 2013.

#include "ppapi/c/dev/ppb_graphics_2d_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool SetScale(PP_Resource resource, float scale) {
  VLOG(4) << "PPB_Graphics2D_Dev::SetScale()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->SetScale(scale);
}

float GetScale(PP_Resource resource) {
  VLOG(4) << "PPB_Graphics2D_Dev::GetScale()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetScale();
}

void SetOffset(PP_Resource resource, const struct PP_Point* offset) {
  VLOG(4) << "PPB_Graphics2D_Dev::SetOffset()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return;
  enter.object()->SetOffset(offset);
}

void SetResizeMode(PP_Resource resource,
                   PP_Graphics2D_Dev_ResizeMode resize_mode) {
  VLOG(4) << "PPB_Graphics2D_Dev::SetResizeMode()";
  EnterResource<PPB_Graphics2D_API> enter(resource, true);
  if (enter.failed())
    return;
  enter.object()->SetResizeMode(resize_mode);
}

const PPB_Graphics2D_Dev_0_1 g_ppb_graphics2d_dev_thunk_0_1 = {
  &SetScale,
  &GetScale,
  &SetResizeMode
};

const PPB_Graphics2D_Dev_0_2 g_ppb_graphics2d_dev_thunk_0_2 = {
  &SetScale,
  &GetScale,
  &SetOffset,
  &SetResizeMode
};

}  // namespace

const PPB_Graphics2D_Dev_0_1* GetPPB_Graphics2D_Dev_0_1_Thunk() {
  return &g_ppb_graphics2d_dev_thunk_0_1;
}

const PPB_Graphics2D_Dev_0_2* GetPPB_Graphics2D_Dev_0_2_Thunk() {
  return &g_ppb_graphics2d_dev_thunk_0_2;
}

}  // namespace thunk
}  // namespace ppapi
