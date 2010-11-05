// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/selection_dev.h"

#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

static const char kPPPSelectionInterface[] = PPP_SELECTION_DEV_INTERFACE;

PP_Var GetSelectedText(PP_Instance instance, PP_Bool html) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPSelectionInterface);
  if (!object)
    return Var().Detach();
  return static_cast<Selection_Dev*>(object)->
      GetSelectedText(PPBoolToBool(html)).Detach();
}

const PPP_Selection_Dev ppp_selection = {
  &GetSelectedText
};

}  // namespace

Selection_Dev::Selection_Dev(Instance* instance)
    : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPSelectionInterface, &ppp_selection);
  associated_instance_->AddPerInstanceObject(kPPPSelectionInterface, this);
}

Selection_Dev::~Selection_Dev() {
  associated_instance_->RemovePerInstanceObject(kPPPSelectionInterface, this);
}

}  // namespace pp
