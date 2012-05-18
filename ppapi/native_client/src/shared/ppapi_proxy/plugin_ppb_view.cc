// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_view.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_upcall.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/upcall.h"

namespace ppapi_proxy {

namespace {

class ViewGetter {
 public:
  ViewGetter(PP_Resource resource, const char* function_name) {
    DebugPrintf("PPB_View::%s: resource=%"NACL_PRId32"\n",
                function_name,
                resource);
    view_ = PluginResource::GetAs<PluginView>(resource);
  }

  PluginView* get() { return view_.get(); }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ViewGetter);
  scoped_refptr<PluginView> view_;
};

// This macro is for starting a resource function. It makes sure resource_arg
// is of type PluginView, and returns error_return if it's not.
#define BEGIN_RESOURCE_THUNK(function_name, resource_arg, error_return) \
  ViewGetter view(resource_arg, function_name); \
  if (!view.get()) { \
    return error_return; \
  }

PP_Bool IsView(PP_Resource resource) {
  DebugPrintf("PPB_View::IsView: resource=%"NACL_PRId32"\n",
              resource);
  return PP_FromBool(PluginResource::GetAs<PluginView>(resource).get());
}

PP_Bool GetRect(PP_Resource resource, PP_Rect* viewport) {
  BEGIN_RESOURCE_THUNK("GetRect", resource, PP_FALSE);
  *viewport = view.get()->view_data().viewport_rect;
  return PP_TRUE;
}

PP_Bool IsFullscreen(PP_Resource resource) {
  BEGIN_RESOURCE_THUNK("IsFullscreen", resource, PP_FALSE);
  return view.get()->view_data().is_fullscreen;
}

PP_Bool IsUserVisible(PP_Resource resource) {
  BEGIN_RESOURCE_THUNK("IsUserVisible", resource, PP_FALSE);
  const ViewData& data = view.get()->view_data();
  return PP_FromBool(data.is_page_visible &&
                     data.clip_rect.size.width > 0 &&
                     data.clip_rect.size.height > 0);
}

PP_Bool IsPageVisible(PP_Resource resource) {
  BEGIN_RESOURCE_THUNK("IsPageVisible", resource, PP_FALSE);
  return view.get()->view_data().is_page_visible;
}

PP_Bool GetClipRect(PP_Resource resource, PP_Rect* clip) {
  BEGIN_RESOURCE_THUNK("GetClipRect", resource, PP_FALSE);
  *clip = view.get()->view_data().clip_rect;
  return PP_TRUE;
}

}  // namespace

PluginView::PluginView() {}

void PluginView::Init(const ViewData& view_data) {
  view_data_ = view_data;
}

const PPB_View* PluginView::GetInterface() {
  static const PPB_View view_interface = {
    &IsView,
    &GetRect,
    &IsFullscreen,
    &IsUserVisible,
    &IsPageVisible,
    &GetClipRect
  };
  return &view_interface;
}

PluginView::~PluginView() {}

}  // namespace ppapi_proxy
