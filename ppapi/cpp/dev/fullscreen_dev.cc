// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/fullscreen_dev.h"

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/size.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Fullscreen_Dev>() {
  return PPB_FULLSCREEN_DEV_INTERFACE;
}

}  // namespace

Fullscreen_Dev::Fullscreen_Dev(Instance* instance)
    : instance_(instance) {
}

Fullscreen_Dev::~Fullscreen_Dev() {
}

bool Fullscreen_Dev::IsFullscreen() {
  return has_interface<PPB_Fullscreen_Dev>() &&
      get_interface<PPB_Fullscreen_Dev>()->IsFullscreen(
          instance_->pp_instance());
}

bool Fullscreen_Dev::SetFullscreen(bool fullscreen) {
  if (!has_interface<PPB_Fullscreen_Dev>())
    return false;
  return PPBoolToBool(get_interface<PPB_Fullscreen_Dev>()->SetFullscreen(
      instance_->pp_instance(), BoolToPPBool(fullscreen)));
}

bool Fullscreen_Dev::GetScreenSize(Size* size) {
  if (!has_interface<PPB_Fullscreen_Dev>())
    return false;
  return PPBoolToBool(get_interface<PPB_Fullscreen_Dev>()->GetScreenSize(
      instance_->pp_instance(), &size->pp_size()));
}

}  // namespace pp
