// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/view.h"

#include "ppapi/c/ppb_view.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_View_1_0>() {
  return PPB_VIEW_INTERFACE_1_0;
}

}  // namespace

View::View() : Resource() {
}

View::View(PP_Resource view_resource) : Resource(view_resource) {
}

Rect View::GetRect() const {
  if (!has_interface<PPB_View_1_0>())
    return Rect();
  PP_Rect out;
  if (PP_ToBool(get_interface<PPB_View_1_0>()->GetRect(pp_resource(), &out)))
    return Rect(out);
  return Rect();
}

bool View::IsFullscreen() const {
  if (!has_interface<PPB_View_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_View_1_0>()->IsFullscreen(pp_resource()));
}

bool View::IsVisible() const {
  if (!has_interface<PPB_View_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_View_1_0>()->IsVisible(pp_resource()));
}

bool View::IsPageVisible() const {
  if (!has_interface<PPB_View_1_0>())
    return true;
  return PP_ToBool(get_interface<PPB_View_1_0>()->IsPageVisible(pp_resource()));
}

Rect View::GetClipRect() const {
  if (!has_interface<PPB_View_1_0>())
    return Rect();
  PP_Rect out;
  if (PP_ToBool(get_interface<PPB_View_1_0>()->GetClipRect(pp_resource(),
                                                           &out)))
    return Rect(out);
  return Rect();
}

}  // namespace pp
