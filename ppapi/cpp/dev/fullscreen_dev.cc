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

DeviceFuncs<PPB_Fullscreen_Dev> ppb_fullscreen_f(PPB_FULLSCREEN_DEV_INTERFACE);

}  // anonymous namespace

Fullscreen_Dev::Fullscreen_Dev(Instance* instance)
    : associated_instance_(instance) {
}

Fullscreen_Dev::~Fullscreen_Dev() {
}

bool Fullscreen_Dev::IsFullscreen() {
  return ppb_fullscreen_f && ppb_fullscreen_f->IsFullscreen(
      associated_instance_->pp_instance());
}

bool Fullscreen_Dev::SetFullscreen(bool fullscreen) {
  if (!ppb_fullscreen_f)
    return false;
  return PPBoolToBool(
      ppb_fullscreen_f->SetFullscreen(associated_instance_->pp_instance(),
                                      BoolToPPBool(fullscreen)));
}

}  // namespace pp
