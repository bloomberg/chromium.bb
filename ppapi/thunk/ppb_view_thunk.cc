// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/ppb_view.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_view_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_View_API> EnterView;

bool IsRectVisible(const PP_Rect& rect) {
  return rect.size.width > 0 && rect.size.height > 0;
}

PP_Bool IsView(PP_Resource resource) {
  EnterView enter(resource, false);
  return enter.succeeded() ? PP_TRUE : PP_FALSE;
}

PP_Bool GetSize(PP_Resource resource, PP_Size* size) {
  EnterView enter(resource, true);
  if (enter.failed() || !size)
    return PP_FALSE;
  *size = enter.object()->GetData().rect.size;
  return PP_TRUE;
}

PP_Bool GetRect(PP_Resource resource, PP_Rect* viewport) {
  EnterView enter(resource, true);
  if (enter.failed() || !viewport)
    return PP_FALSE;
  *viewport = enter.object()->GetData().rect;
  return PP_TRUE;
}

PP_Bool IsFullscreen(PP_Resource resource) {
  EnterView enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(enter.object()->GetData().is_fullscreen);
}

PP_Bool IsUserVisible(PP_Resource resource) {
  EnterView enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(enter.object()->GetData().is_page_visible &&
                     IsRectVisible(enter.object()->GetData().clip_rect));
}

PP_Bool IsPageVisible(PP_Resource resource) {
  EnterView enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(enter.object()->GetData().is_page_visible);
}

PP_Bool IsClipVisible(PP_Resource resource) {
  EnterView enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return PP_FromBool(IsRectVisible(enter.object()->GetData().clip_rect));
}

PP_Bool GetClipRect(PP_Resource resource, PP_Rect* clip) {
  EnterView enter(resource, true);
  if (enter.failed() || !clip)
    return PP_FALSE;
  *clip = enter.object()->GetData().clip_rect;
  return PP_TRUE;
}

const PPB_View g_ppb_view_thunk = {
  &IsView,
  &GetRect,
  &IsFullscreen,
  &IsUserVisible,
  &IsPageVisible,
  &GetClipRect
};

}  // namespace

const PPB_View* GetPPB_View_Thunk() {
  return &g_ppb_view_thunk;
}

}  // namespace thunk
}  // namespace ppapi
