// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From ppb_view.idl modified Fri Feb  8 14:28:54 2013.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_view.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_view_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsView(PP_Resource resource) {
  EnterResource<PPB_View_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

PP_Bool GetRect(PP_Resource resource, struct PP_Rect* rect) {
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRect(rect);
}

PP_Bool IsFullscreen(PP_Resource resource) {
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsFullscreen();
}

PP_Bool IsVisible(PP_Resource resource) {
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsVisible();
}

PP_Bool IsPageVisible(PP_Resource resource) {
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->IsPageVisible();
}

PP_Bool GetClipRect(PP_Resource resource, struct PP_Rect* clip) {
  EnterResource<PPB_View_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetClipRect(clip);
}

const PPB_View_1_0 g_ppb_view_thunk_1_0 = {
  &IsView,
  &GetRect,
  &IsFullscreen,
  &IsVisible,
  &IsPageVisible,
  &GetClipRect
};

}  // namespace

const PPB_View_1_0* GetPPB_View_1_0_Thunk() {
  return &g_ppb_view_thunk_1_0;
}

}  // namespace thunk
}  // namespace ppapi
