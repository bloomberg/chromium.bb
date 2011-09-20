// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash_fullscreen.h"

#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/size.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FlashFullscreen>() {
  return PPB_FLASHFULLSCREEN_INTERFACE;
}

}  // namespace

FlashFullscreen::FlashFullscreen(Instance* instance)
    : instance_(instance) {
}

FlashFullscreen::~FlashFullscreen() {
}

bool FlashFullscreen::IsFullscreen() {
  return has_interface<PPB_FlashFullscreen>() &&
      get_interface<PPB_FlashFullscreen>()->IsFullscreen(
          instance_->pp_instance());
}

bool FlashFullscreen::SetFullscreen(bool fullscreen) {
  if (!has_interface<PPB_FlashFullscreen>())
    return false;
  return PP_ToBool(get_interface<PPB_FlashFullscreen>()->SetFullscreen(
      instance_->pp_instance(), PP_FromBool(fullscreen)));
}

bool FlashFullscreen::GetScreenSize(Size* size) {
  if (!has_interface<PPB_FlashFullscreen>())
    return false;
  return PP_ToBool(get_interface<PPB_FlashFullscreen>()->GetScreenSize(
      instance_->pp_instance(), &size->pp_size()));
}

}  // namespace pp
