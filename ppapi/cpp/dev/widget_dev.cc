// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/widget_dev.h"

#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_Widget_Dev> widget_f(PPB_WIDGET_DEV_INTERFACE);

}  // namespace

namespace pp {

Widget_Dev::Widget_Dev(PP_Resource resource) : Resource(resource) {
}

Widget_Dev::Widget_Dev(const Widget_Dev& other) : Resource(other) {
}

Widget_Dev& Widget_Dev::operator=(const Widget_Dev& other) {
  Widget_Dev copy(other);
  swap(copy);
  return *this;
}

void Widget_Dev::swap(Widget_Dev& other) {
  Resource::swap(other);
}

bool Widget_Dev::Paint(const Rect& rect, ImageData* image) {
  if (!widget_f)
    return false;
  return PPBoolToBool(widget_f->Paint(pp_resource(),
                                      &rect.pp_rect(),
                                      image->pp_resource()));
}

bool Widget_Dev::HandleEvent(const PP_InputEvent& event) {
  if (!widget_f)
    return false;
  return PPBoolToBool(widget_f->HandleEvent(pp_resource(), &event));
}

bool Widget_Dev::GetLocation(Rect* location) {
  if (!widget_f)
    return false;
  return PPBoolToBool(widget_f->GetLocation(pp_resource(),
                                            &location->pp_rect()));
}

void Widget_Dev::SetLocation(const Rect& location) {
  if (widget_f)
    widget_f->SetLocation(pp_resource(), &location.pp_rect());
}

}  // namespace pp
