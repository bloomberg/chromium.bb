// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_view_dev.h"
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

float GetDeviceScale(PP_Resource resource) {
  EnterView enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetData().device_scale;
}

float GetCSSScale(PP_Resource resource) {
  EnterView enter(resource, true);
  if (enter.failed())
    return 0.0f;
  return enter.object()->GetData().css_scale;
}

const PPB_View g_ppb_view_thunk = {
  &IsView,
  &GetRect,
  &IsFullscreen,
  &IsUserVisible,
  &IsPageVisible,
  &GetClipRect
};

const PPB_View_Dev g_ppb_view_dev_thunk = {
  &GetDeviceScale,
  &GetCSSScale
};

}  // namespace

const PPB_View* GetPPB_View_1_0_Thunk() {
  return &g_ppb_view_thunk;
}

const PPB_View_Dev* GetPPB_View_Dev_0_1_Thunk() {
  return &g_ppb_view_dev_thunk;
}

}  // namespace thunk
}  // namespace ppapi
