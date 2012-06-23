// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_2d_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_Graphics2D_API> EnterGraphics2D;

PP_Resource Create(PP_Instance instance,
                   const PP_Size* size,
                   PP_Bool is_always_opaque) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics2D(instance, *size, is_always_opaque);
}

PP_Bool IsGraphics2D(PP_Resource resource) {
  EnterGraphics2D enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_Bool Describe(PP_Resource graphics_2d,
                 PP_Size* size,
                 PP_Bool* is_always_opaque) {
  EnterGraphics2D enter(graphics_2d, true);
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
  EnterGraphics2D enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->PaintImageData(image_data, top_left, src_rect);
}

void Scroll(PP_Resource graphics_2d,
            const PP_Rect* clip_rect,
            const PP_Point* amount) {
  EnterGraphics2D enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->Scroll(clip_rect, amount);
}

void ReplaceContents(PP_Resource graphics_2d, PP_Resource image_data) {
  EnterGraphics2D enter(graphics_2d, true);
  if (enter.failed())
    return;
  enter.object()->ReplaceContents(image_data);
}

int32_t Flush(PP_Resource graphics_2d, PP_CompletionCallback callback) {
  EnterGraphics2D enter(graphics_2d, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Flush(enter.callback()));
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

const PPB_Graphics2D_1_0* GetPPB_Graphics2D_1_0_Thunk() {
  return &g_ppb_graphics_2d_thunk;
}

}  // namespace thunk
}  // namespace ppapi
