// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/find_dev.h"

#include "ppapi/c/dev/ppb_find_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Find_Dev>() {
  return PPB_FIND_DEV_INTERFACE;
}

static const char kPPPFindInterface[] = PPP_FIND_DEV_INTERFACE;

PP_Bool StartFind(PP_Instance instance,
                  const char* text,
                  PP_Bool case_sensitive) {
  void* object = Instance::GetPerInstanceObject(instance, kPPPFindInterface);
  if (!object)
    return PP_FALSE;
  bool return_value = static_cast<Find_Dev*>(object)->StartFind(
      text, PP_ToBool(case_sensitive));
  return PP_FromBool(return_value);
}

void SelectFindResult(PP_Instance instance, PP_Bool forward) {
  void* object = Instance::GetPerInstanceObject(instance, kPPPFindInterface);
  if (object)
    static_cast<Find_Dev*>(object)->SelectFindResult(PP_ToBool(forward));
}

void StopFind(PP_Instance instance) {
  void* object = Instance::GetPerInstanceObject(instance, kPPPFindInterface);
  if (object)
    static_cast<Find_Dev*>(object)->StopFind();
}

const PPP_Find_Dev ppp_find = {
  &StartFind,
  &SelectFindResult,
  &StopFind
};

}  // namespace

Find_Dev::Find_Dev(Instance* instance) : associated_instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPFindInterface, &ppp_find);
  instance->AddPerInstanceObject(kPPPFindInterface, this);
}

Find_Dev::~Find_Dev() {
  Instance::RemovePerInstanceObject(associated_instance_,
                                    kPPPFindInterface, this);
}

void Find_Dev::NumberOfFindResultsChanged(int32_t total, bool final_result) {
  if (has_interface<PPB_Find_Dev>()) {
    get_interface<PPB_Find_Dev>()->NumberOfFindResultsChanged(
        associated_instance_.pp_instance(), total, PP_FromBool(final_result));
  }
}

void Find_Dev::SelectedFindResultChanged(int32_t index) {
  if (has_interface<PPB_Find_Dev>()) {
    get_interface<PPB_Find_Dev>()->SelectedFindResultChanged(
        associated_instance_.pp_instance(), index);
  }
}

}  // namespace pp
