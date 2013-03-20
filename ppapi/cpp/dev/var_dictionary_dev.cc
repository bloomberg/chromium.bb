// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/var_dictionary_dev.h"

#include "ppapi/c/dev/ppb_var_dictionary_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VarDictionary_Dev_0_1>() {
  return PPB_VAR_DICTIONARY_DEV_INTERFACE_0_1;
}

}  // namespace

VarDictionary_Dev::VarDictionary_Dev() : Var(Null()) {
  if (has_interface<PPB_VarDictionary_Dev_0_1>())
    var_ = get_interface<PPB_VarDictionary_Dev_0_1>()->Create();
  else
    PP_NOTREACHED();
}

VarDictionary_Dev::VarDictionary_Dev(const Var& var) : Var(var) {
  if (!var.is_dictionary()) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarDictionary_Dev::VarDictionary_Dev(const PP_Var& var) : Var(var) {
  if (var.type != PP_VARTYPE_DICTIONARY) {
    PP_NOTREACHED();

    // This takes care of releasing the reference that this object holds.
    Var::operator=(Var(Null()));
  }
}

VarDictionary_Dev::VarDictionary_Dev(const VarDictionary_Dev& other)
    : Var(other) {
}

VarDictionary_Dev::~VarDictionary_Dev() {
}

VarDictionary_Dev& VarDictionary_Dev::operator=(
    const VarDictionary_Dev& other) {
  Var::operator=(other);
  return *this;
}

Var& VarDictionary_Dev::operator=(const Var& other) {
  if (other.is_dictionary()) {
    Var::operator=(other);
  } else {
    PP_NOTREACHED();
    Var::operator=(Var(Null()));
  }
  return *this;
}

Var VarDictionary_Dev::Get(const Var& key) const {
  if (!has_interface<PPB_VarDictionary_Dev_0_1>())
    return Var();

  return Var(
      PASS_REF,
      get_interface<PPB_VarDictionary_Dev_0_1>()->Get(var_, key.pp_var()));
}

PP_Bool VarDictionary_Dev::Set(const Var& key, const Var& value) {
  if (!has_interface<PPB_VarDictionary_Dev_0_1>())
    return PP_FALSE;

  return get_interface<PPB_VarDictionary_Dev_0_1>()->Set(var_, key.pp_var(),
                                                         value.pp_var());
}

void VarDictionary_Dev::Delete(const Var& key) {
  if (has_interface<PPB_VarDictionary_Dev_0_1>())
    get_interface<PPB_VarDictionary_Dev_0_1>()->Delete(var_, key.pp_var());
}

PP_Bool VarDictionary_Dev::HasKey(const Var& key) const {
  if (!has_interface<PPB_VarDictionary_Dev_0_1>())
    return PP_FALSE;

  return get_interface<PPB_VarDictionary_Dev_0_1>()->HasKey(var_, key.pp_var());
}

VarArray_Dev VarDictionary_Dev::GetKeys() const {
  if (!has_interface<PPB_VarDictionary_Dev_0_1>())
    return VarArray_Dev();

  Var result(PASS_REF,
             get_interface<PPB_VarDictionary_Dev_0_1>()->GetKeys(var_));
  if (result.is_array())
    return VarArray_Dev(result);
  else
    return VarArray_Dev();
}

}  // namespace pp
