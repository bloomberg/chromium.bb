// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/var_resource_dev.h"

#include "ppapi/c/dev/ppb_var_resource_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarResource_Dev_0_1>() {
  return PPB_VAR_RESOURCE_DEV_INTERFACE_0_1;
}

}  // namespace

VarResource_Dev::VarResource_Dev(const pp::Resource& resource) : Var(Null()) {
  if (!has_interface<PPB_VarResource_Dev_0_1>()) {
    PP_NOTREACHED();
    return;
  }

  // Note: Var(Null()) sets is_managed_ to true, so |var_| will be properly
  // released upon destruction.
  var_ = get_interface<PPB_VarResource_Dev_0_1>()->VarFromResource(
      resource.pp_resource());
}

VarResource_Dev::VarResource_Dev(const Var& var) : Var(var) {
  if (!var.is_resource()) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarResource_Dev::VarResource_Dev(const VarResource_Dev& other) : Var(other) {}

VarResource_Dev::~VarResource_Dev() {}

VarResource_Dev& VarResource_Dev::operator=(const VarResource_Dev& other) {
  Var::operator=(other);
  return *this;
}

Var& VarResource_Dev::operator=(const Var& other) {
  if (other.is_resource()) {
    Var::operator=(other);
  } else {
    PP_NOTREACHED();
    Var::operator=(Var(Null()));
  }
  return *this;
}

pp::Resource VarResource_Dev::AsResource() {
  if (!has_interface<PPB_VarResource_Dev_0_1>())
    return pp::Resource();

  return pp::Resource(
      pp::PASS_REF,
      get_interface<PPB_VarResource_Dev_0_1>()->VarToResource(var_));
}

}  // namespace pp
