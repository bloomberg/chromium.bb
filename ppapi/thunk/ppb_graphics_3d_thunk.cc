// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_graphics_3d.idl modified Fri Mar 29 10:54:38 2013.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_3d.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetAttribMaxValue(PP_Resource instance,
                          int32_t attribute,
                          int32_t* value) {
  EnterResource<PPB_Graphics3D_API> enter(instance, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetAttribMaxValue(attribute, value);
}

PP_Resource Create(PP_Instance instance,
                   PP_Resource share_context,
                   const int32_t attrib_list[]) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics3D(instance,
                                             share_context,
                                             attrib_list);
}

PP_Bool IsGraphics3D(PP_Resource resource) {
  EnterResource<PPB_Graphics3D_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetAttribs(PP_Resource context, int32_t attrib_list[]) {
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetAttribs(attrib_list);
}

int32_t SetAttribs(PP_Resource context, const int32_t attrib_list[]) {
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->SetAttribs(attrib_list);
}

int32_t GetError(PP_Resource context) {
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetError();
}

int32_t ResizeBuffers(PP_Resource context, int32_t width, int32_t height) {
  EnterResource<PPB_Graphics3D_API> enter(context, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->ResizeBuffers(width, height);
}

int32_t SwapBuffers(PP_Resource context,
                    struct PP_CompletionCallback callback) {
  EnterResource<PPB_Graphics3D_API> enter(context, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SwapBuffers(enter.callback()));
}

const PPB_Graphics3D_1_0 g_ppb_graphics3d_thunk_1_0 = {
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

const PPB_Graphics3D_1_0* GetPPB_Graphics3D_1_0_Thunk() {
  return &g_ppb_graphics3d_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
