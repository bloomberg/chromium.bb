// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/fullscreen.h"

#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/size.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Fullscreen>() {
  return PPB_FULLSCREEN_INTERFACE;
}

}  // namespace

Fullscreen::Fullscreen(Instance* instance)
    : instance_(instance) {
}

Fullscreen::~Fullscreen() {
}

bool Fullscreen::IsFullscreen() {
  return has_interface<PPB_Fullscreen>() &&
      get_interface<PPB_Fullscreen>()->IsFullscreen(
          instance_->pp_instance());
}

bool Fullscreen::SetFullscreen(bool fullscreen) {
  if (!has_interface<PPB_Fullscreen>())
    return false;
  return PP_ToBool(get_interface<PPB_Fullscreen>()->SetFullscreen(
      instance_->pp_instance(), PP_FromBool(fullscreen)));
}

bool Fullscreen::GetScreenSize(Size* size) {
  if (!has_interface<PPB_Fullscreen>())
    return false;
  return PP_ToBool(get_interface<PPB_Fullscreen>()->GetScreenSize(
      instance_->pp_instance(), &size->pp_size()));
}

}  // namespace pp
