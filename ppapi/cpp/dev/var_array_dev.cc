// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/var_array_dev.h"

#include "ppapi/c/dev/ppb_var_array_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarArray_Dev_0_1>() {
  return PPB_VAR_ARRAY_DEV_INTERFACE_0_1;
}

}  // namespace

VarArray_Dev::VarArray_Dev() : Var(Null()) {
  if (has_interface<PPB_VarArray_Dev_0_1>())
    var_ = get_interface<PPB_VarArray_Dev_0_1>()->Create();
  else
    PP_NOTREACHED();
}

VarArray_Dev::VarArray_Dev(const Var& var) : Var(var) {
  if (!var.is_array()) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarArray_Dev::VarArray_Dev(const PP_Var& var) : Var(var) {
  if (var.type != PP_VARTYPE_ARRAY) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarArray_Dev::VarArray_Dev(const VarArray_Dev& other) : Var(other) {
}

VarArray_Dev::~VarArray_Dev() {
}

VarArray_Dev& VarArray_Dev::operator=(const VarArray_Dev& other) {
  Var::operator=(other);
  return *this;
}

Var& VarArray_Dev::operator=(const Var& other) {
  if (other.is_array()) {
    Var::operator=(other);
  } else {
    PP_NOTREACHED();
    Var::operator=(Var(Null()));
  }
  return *this;
}

Var VarArray_Dev::Get(uint32_t index) const {
  if (!has_interface<PPB_VarArray_Dev_0_1>())
    return Var();

  return Var(PASS_REF, get_interface<PPB_VarArray_Dev_0_1>()->Get(var_, index));
}

PP_Bool VarArray_Dev::Set(uint32_t index, const Var& value) {
  if (!has_interface<PPB_VarArray_Dev_0_1>())
    return PP_FALSE;

  return get_interface<PPB_VarArray_Dev_0_1>()->Set(var_, index,
                                                    value.pp_var());
}

uint32_t VarArray_Dev::GetLength() const {
  if (!has_interface<PPB_VarArray_Dev_0_1>())
    return 0;

  return get_interface<PPB_VarArray_Dev_0_1>()->GetLength(var_);
}

PP_Bool VarArray_Dev::SetLength(uint32_t length) {
  if (!has_interface<PPB_VarArray_Dev_0_1>())
    return PP_FALSE;

  return get_interface<PPB_VarArray_Dev_0_1>()->SetLength(var_, length);
}

}  // namespace pp
