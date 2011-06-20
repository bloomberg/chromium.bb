// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_widget_impl.h"

#include "base/logging.h"
#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppb_image_data_impl.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/plugin_module.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Bool IsWidget(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Widget_Impl>(resource));
}

PP_Bool Paint(PP_Resource resource,
              const PP_Rect* rect,
              PP_Resource image_id) {
  scoped_refptr<PPB_Widget_Impl> widget(
      Resource::GetAs<PPB_Widget_Impl>(resource));
  if (!widget)
    return PP_FALSE;

  scoped_refptr<PPB_ImageData_Impl> image(
      Resource::GetAs<PPB_ImageData_Impl>(image_id));
  if (!image)
    return PP_FALSE;

  return BoolToPPBool(widget->Paint(rect, image));
}

PP_Bool HandleEvent(PP_Resource resource, const PP_InputEvent* event) {
  scoped_refptr<PPB_Widget_Impl> widget(
      Resource::GetAs<PPB_Widget_Impl>(resource));
  return BoolToPPBool(widget && widget->HandleEvent(event));
}

PP_Bool GetLocation(PP_Resource resource, PP_Rect* location) {
  scoped_refptr<PPB_Widget_Impl> widget(
      Resource::GetAs<PPB_Widget_Impl>(resource));
  return BoolToPPBool(widget && widget->GetLocation(location));
}

void SetLocation(PP_Resource resource, const PP_Rect* location) {
  scoped_refptr<PPB_Widget_Impl> widget(
      Resource::GetAs<PPB_Widget_Impl>(resource));
  if (widget)
    widget->SetLocation(location);
}

const PPB_Widget_Dev ppb_widget = {
  &IsWidget,
  &Paint,
  &HandleEvent,
  &GetLocation,
  &SetLocation,
};

}  // namespace

PPB_Widget_Impl::PPB_Widget_Impl(PluginInstance* instance)
    : Resource(instance) {
}

PPB_Widget_Impl::~PPB_Widget_Impl() {
}

// static
const PPB_Widget_Dev* PPB_Widget_Impl::GetInterface() {
  return &ppb_widget;
}

PPB_Widget_Impl* PPB_Widget_Impl::AsPPB_Widget_Impl() {
  return this;
}

bool PPB_Widget_Impl::GetLocation(PP_Rect* location) {
  *location = location_;
  return true;
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

