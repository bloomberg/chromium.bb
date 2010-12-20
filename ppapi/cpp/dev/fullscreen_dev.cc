// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/fullscreen_dev.h"

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Fullscreen_Dev>() {
  return PPB_FULLSCREEN_DEV_INTERFACE;
}

}  // namespace

Fullscreen_Dev::Fullscreen_Dev(Instance* instance)
    : associated_instance_(instance) {
}

Fullscreen_Dev::~Fullscreen_Dev() {
}

bool Fullscreen_Dev::IsFullscreen() {
  return has_interface<PPB_Fullscreen_Dev>() &&
      get_interface<PPB_Fullscreen_Dev>()->IsFullscreen(
          associated_instance_->pp_instance());
}

bool Fullscreen_Dev::SetFullscreen(bool fullscreen) {
  if (!has_interface<PPB_Fullscreen_Dev>())
    return false;
  return PPBoolToBool(get_interface<PPB_Fullscreen_Dev>()->SetFullscreen(
      associated_instance_->pp_instance(), BoolToPPBool(fullscreen)));
}

}  // namespace pp
