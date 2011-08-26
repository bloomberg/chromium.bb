// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance,
                   const PP_Size* size,
                   PP_Bool is_always_opaque) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics2D(instance, *size, is_always_opaque);
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  EnterResource<PPB_Graphics2D_API> enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource graphics_2d,
                 PP_Size* size,
                 PP_Bool* is_always_opaque) {
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed()) {
    size->width = 0;
    size->height = 0;
    *is_always_opaque = PP_FALSE;
    return PP_FALSE;
  }
  return enter.object()->Describe(size, is_always_opaque);
}

void PaintImageData(PP_Resource graphics_2d,
                    PP_Resource image_data,
                    const PP_Point* top_left,
                    const PP_Rect* src_rect) {
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->PaintImageData(image_data, top_left, src_rect);
}

void Scroll(PP_Resource graphics_2d,
            const PP_Rect* clip_rect,
            const PP_Point* amount) {
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->Scroll(clip_rect, amount);
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->ReplaceContents(image_data);
}

int32_t Flush(PP_Resource graphics_2d,
              PP_CompletionCallback callback) {
  EnterResource<PPB_Graphics2D_API> enter(graphics_2d, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Flush(callback);
  return MayForceCallback(callback, result);
}

const PPB_Graphics2D g_ppb_graphics_2d_thunk = {
  &Create,
  &IsGraphics2D,
  &Describe,
  &PaintImageData,
  &Scroll,
  &ReplaceContents,
  &Flush
};

}  // namespace

const PPB_Graphics2D* GetPPB_Graphics2D_Thunk() {
  return &g_ppb_graphics_2d_thunk;
}

}  // namespace thunk
}  // namespace ppapi
