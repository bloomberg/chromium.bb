// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/find_dev.h"

#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPFindInterface[] = PPP_FIND_DEV_INTERFACE;

PP_Bool StartFind(PP_Instance instance,
                  const char* text,
                  PP_Bool case_sensitive) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPFindInterface);
  if (!object)
    return PP_FALSE;
  bool return_value = static_cast<Find_Dev*>(object)->StartFind(
      text, PPBoolToBool(case_sensitive));
  return BoolToPPBool(return_value);
}

void SelectFindResult(PP_Instance instance, PP_Bool forward) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPFindInterface);
  if (object)
    static_cast<Find_Dev*>(object)->SelectFindResult(PPBoolToBool(forward));
}

void StopFind(PP_Instance instance) {
  void* object =
      pp::Instance::GetPerInstanceObject(instance, kPPPFindInterface);
  if (object)
    static_cast<Find_Dev*>(object)->StopFind();
}

const PPP_Find_Dev ppp_find = {
  &StartFind,
  &SelectFindResult,
  &StopFind
};

DeviceFuncs<PPB_Find_Dev> ppb_find_f(PPB_FIND_DEV_INTERFACE);

}  // namespace

Find_Dev::Find_Dev(Instance* instance) : associated_instance_(instance) {
  pp::Module::Get()->AddPluginInterface(kPPPFindInterface, &ppp_find);
  associated_instance_->AddPerInstanceObject(kPPPFindInterface, this);
}

Find_Dev::~Find_Dev() {
  associated_instance_->RemovePerInstanceObject(kPPPFindInterface, this);
}

void Find_Dev::NumberOfFindResultsChanged(int32_t total, bool final_result) {
  if (ppb_find_f) {
    ppb_find_f->NumberOfFindResultsChanged(associated_instance_->pp_instance(),
                                           total,
                                           BoolToPPBool(final_result));
  }
}

void Find_Dev::SelectedFindResultChanged(int32_t index) {
  if (ppb_find_f) {
    ppb_find_f->SelectedFindResultChanged(associated_instance_->pp_instance(),
                                          index);
  }
}

}  // namespace pp
