// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_var_serialization_rules.h"

#include "base/logging.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"

namespace pp {
namespace proxy {

HostVarSerializationRules::HostVarSerializationRules(
    const PPB_Var_Deprecated* var_interface,
    PP_Module pp_module)
    : var_interface_(var_interface),
      pp_module_(pp_module) {
}

HostVarSerializationRules::~HostVarSerializationRules() {
}

PP_Var HostVarSerializationRules::SendCallerOwned(const PP_Var& var,
                                                  std::string* str_val) {
  if (var.type == PP_VARTYPE_STRING)
    VarToString(var, str_val);
  return var;
}

PP_Var HostVarSerializationRules::BeginReceiveCallerOwned(
    const PP_Var& var,
    const std::string* str_val,
    Dispatcher* /* dispatcher */) {
  if (var.type == PP_VARTYPE_STRING) {
    // Convert the string to the context of the current process.
    return var_interface_->VarFromUtf8(pp_module_, str_val->c_str(),
                                       static_cast<uint32_t>(str_val->size()));
  }
  return var;
}

void HostVarSerializationRules::EndReceiveCallerOwned(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    // Destroy the string BeginReceiveCallerOwned created above.
    var_interface_->Release(var);
  }
}

PP_Var HostVarSerializationRules::ReceivePassRef(const PP_Var& var,
                                                 const std::string& str_val,
                                                 Dispatcher* /* dispatcher */) {
  if (var.type == PP_VARTYPE_STRING) {
    // Convert the string to the context of the current process.
    return var_interface_->VarFromUtf8(pp_module_, str_val.c_str(),
                                       static_cast<uint32_t>(str_val.size()));
  }

  // See PluginVarSerialization::BeginSendPassRef for an example.
  if (var.type == PP_VARTYPE_OBJECT)
    var_interface_->AddRef(var);
  return var;
}

PP_Var HostVarSerializationRules::BeginSendPassRef(const PP_Var& var,
                                                 std::string* str_val) {
  // See PluginVarSerialization::ReceivePassRef for an example. We don't need
  // to do anything here other than convert the string.
  if (var.type == PP_VARTYPE_STRING)
    VarToString(var, str_val);
  return var;
}

void HostVarSerializationRules::EndSendPassRef(const PP_Var& var) {
  // See PluginVarSerialization::ReceivePassRef for an example. We don't need
  // to do anything here.
}

void HostVarSerializationRules::VarToString(const PP_Var& var,
                                            std::string* str) {
  DCHECK(var.type == PP_VARTYPE_STRING);

  // This could be optimized to avoid an extra string copy by going to a lower
  // level of the browser's implementation of strings where we already have
  // a std::string.
  uint32_t len = 0;
  const char* data = var_interface_->VarToUtf8(var, &len);
  str->assign(data, len);
}

void HostVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  var_interface_->Release(var);
}

}  // namespace proxy
}  // namespace pp
