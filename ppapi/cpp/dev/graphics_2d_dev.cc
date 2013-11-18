// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/graphics_2d_dev.h"

#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics2D_Dev_0_1>() {
  return PPB_GRAPHICS2D_DEV_INTERFACE_0_1;
}

template <> const char* interface_name<PPB_Graphics2D_Dev_0_2>() {
  return PPB_GRAPHICS2D_DEV_INTERFACE_0_2;
}

}  // namespace

// static
bool Graphics2D_Dev::SupportsScale() {
  return has_interface<PPB_Graphics2D_Dev_0_1>() ||
      has_interface<PPB_Graphics2D_Dev_0_2>();
}

bool Graphics2D_Dev::SetScale(float scale) {
  if (has_interface<PPB_Graphics2D_Dev_0_2>()) {
    return PP_ToBool(get_interface<PPB_Graphics2D_Dev_0_2>()->SetScale(
        pp_resource(), scale));
  }
  if (has_interface<PPB_Graphics2D_Dev_0_1>()) {
    return PP_ToBool(get_interface<PPB_Graphics2D_Dev_0_1>()->SetScale(
        pp_resource(), scale));
  }
  return false;
}

float Graphics2D_Dev::GetScale() {
  if (has_interface<PPB_Graphics2D_Dev_0_2>())
    return get_interface<PPB_Graphics2D_Dev_0_2>()->GetScale(pp_resource());
  if (has_interface<PPB_Graphics2D_Dev_0_1>())
    return get_interface<PPB_Graphics2D_Dev_0_1>()->GetScale(pp_resource());
  return 1.0f;
}

void Graphics2D_Dev::SetOffset(const pp::Point& offset) {
  if (!has_interface<PPB_Graphics2D_Dev_0_2>())
    return;
  get_interface<PPB_Graphics2D_Dev_0_2>()->SetOffset(pp_resource(),
                                                     &offset.pp_point());
}

void Graphics2D_Dev::SetResizeMode(PP_Graphics2D_Dev_ResizeMode resize_mode) {
  if (!has_interface<PPB_Graphics2D_Dev_0_2>())
    return;
  get_interface<PPB_Graphics2D_Dev_0_2>()->SetResizeMode(pp_resource(),
                                                         resize_mode);
}

}  // namespace pp
