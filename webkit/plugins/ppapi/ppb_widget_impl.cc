// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_widget_impl.h"

#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/plugin_module.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::thunk::PPB_Widget_API;

namespace webkit {
namespace ppapi {

PPB_Widget_Impl::PPB_Widget_Impl(PluginInstance* instance)
    : Resource(instance) {
  memset(&location_, 0, sizeof(location_));
}

PPB_Widget_Impl::~PPB_Widget_Impl() {
}

PPB_Widget_API* PPB_Widget_Impl::AsPPB_Widget_API() {
  return this;
}

PP_Bool PPB_Widget_Impl::Paint(const PP_Rect* rect, PP_Resource image_id) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_id, true);
  if (enter.failed())
    return PP_FALSE;
  return PaintInternal(gfx::Rect(rect->point.x, rect->point.y,
                                 rect->size.width, rect->size.height),
                       static_cast<PPB_ImageData_Impl*>(enter.object()));
}

PP_Bool PPB_Widget_Impl::GetLocation(PP_Rect* location) {
  *location = location_;
  return PP_TRUE;
}

void PPB_Widget_Impl::SetLocation(const PP_Rect* location) {
  location_ = *location;
  SetLocationInternal(location);
}

void PPB_Widget_Impl::Invalidate(const PP_Rect* dirty) {
  if (!instance())
    return;
  const PPP_Widget_Dev* widget = static_cast<const PPP_Widget_Dev*>(
      instance()->module()->GetPluginInterface(PPP_WIDGET_DEV_INTERFACE));
  if (!widget)
    return;
  ScopedResourceId resource(this);
  widget->Invalidate(instance()->pp_instance(), resource.id, dirty);
}

}  // namespace ppapi
}  // namespace webkit

