// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/view.h"

#include "ppapi/c/ppb_view.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_View>() {
  return PPB_VIEW_INTERFACE;
}

}  // namespace

View::View() : Resource() {
}

View::View(PP_Resource view_resource) : Resource(view_resource) {
}

Rect View::GetRect() const {
  if (!has_interface<PPB_View>())
    return Rect();
  PP_Rect out;
  if (PP_ToBool(get_interface<PPB_View>()->GetRect(pp_resource(), &out)))
    return Rect(out);
  return Rect();
}

bool View::IsFullscreen() const {
  if (!has_interface<PPB_View>())
    return false;
  return PP_ToBool(get_interface<PPB_View>()->IsFullscreen(pp_resource()));
}

bool View::IsVisible() const {
  if (!has_interface<PPB_View>())
    return false;
  return PP_ToBool(get_interface<PPB_View>()->IsVisible(pp_resource()));
}

bool View::IsPageVisible() const {
  if (!has_interface<PPB_View>())
    return true;
  return PP_ToBool(get_interface<PPB_View>()->IsPageVisible(pp_resource()));
}

Rect View::GetClipRect() const {
  if (!has_interface<PPB_View>())
    return Rect();
  PP_Rect out;
  if (PP_ToBool(get_interface<PPB_View>()->GetClipRect(pp_resource(), &out)))
    return Rect(out);
  return Rect();
}

}  // namespace pp
