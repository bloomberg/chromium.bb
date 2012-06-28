// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/graphics_2d_dev.h"

#include "ppapi/c/dev/ppb_graphics_2d_dev.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics2D_Dev>() {
  return PPB_GRAPHICS2D_DEV_INTERFACE;
}

}  // namespace

bool Graphics2DDev::SetScale(float scale) {
  if (!has_interface<PPB_Graphics2D_Dev>())
    return false;
  return PP_ToBool(get_interface<PPB_Graphics2D_Dev>()->SetScale(pp_resource(),
                                                                 scale));
}

float Graphics2DDev::GetScale() {
  if (!has_interface<PPB_Graphics2D_Dev>())
    return 1.0f;
  return get_interface<PPB_Graphics2D_Dev>()->GetScale(pp_resource());
}

}  // namespace pp
