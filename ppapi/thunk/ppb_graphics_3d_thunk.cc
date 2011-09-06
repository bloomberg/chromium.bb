// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_Graphics3D_API> EnterGraphics3D;

int32_t GetAttribMaxValue(PP_Resource instance,
                          int32_t attribute,
                          int32_t* value) {
  // TODO(alokp): Implement me.
  return PP_ERROR_FAILED;
}

PP_Resource Create(PP_Instance instance,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics3D(
      instance, share_context, attrib_list);
}

PP_Bool IsGraphics3D(PP_Resource resource) {
  EnterGraphics3D enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetAttribs(PP_Resource graphics_3d, int32_t* attrib_list) {
  EnterGraphics3D enter(graphics_3d, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetAttribs(attrib_list);
}

int32_t SetAttribs(PP_Resource graphics_3d, int32_t* attrib_list) {
  EnterGraphics3D enter(graphics_3d, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->SetAttribs(attrib_list);
}

int32_t GetError(PP_Resource graphics_3d) {
  EnterGraphics3D enter(graphics_3d, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  return enter.object()->GetError();
}

int32_t ResizeBuffers(PP_Resource graphics_3d, int32_t width, int32_t height) {
  EnterGraphics3D enter(graphics_3d, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->ResizeBuffers(width, height);
}

int32_t SwapBuffers(PP_Resource graphics_3d, PP_CompletionCallback callback) {
  EnterGraphics3D enter(graphics_3d, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->SwapBuffers(callback);
  return MayForceCallback(callback, result);
}

const PPB_Graphics3D_Dev g_ppb_graphics_3d_thunk = {
  &GetAttribMaxValue,
  &Create,
  &IsGraphics3D,
  &GetAttribs,
  &SetAttribs,
  &GetError,
  &ResizeBuffers,
  &SwapBuffers
};

}  // namespace

const PPB_Graphics3D_Dev* GetPPB_Graphics3D_Thunk() {
  return &g_ppb_graphics_3d_thunk;
}

}  // namespace thunk
}  // namespace ppapi
