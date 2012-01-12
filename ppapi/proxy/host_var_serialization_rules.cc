// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_var_serialization_rules.h"

#include "base/logging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

using ppapi::PpapiGlobals;
using ppapi::StringVar;
using ppapi::VarTracker;

namespace ppapi {
namespace proxy {

HostVarSerializationRules::HostVarSerializationRules(PP_Module pp_module)
    : pp_module_(pp_module) {
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
  if (var.type == PP_VARTYPE_STRING)
    return StringVar::StringToPPVar(*str_val);
  return var;
}

void HostVarSerializationRules::EndReceiveCallerOwned(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    // Destroy the string BeginReceiveCallerOwned created above.
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
  }
}

PP_Var HostVarSerializationRules::ReceivePassRef(const PP_Var& var,
                                                 const std::string& str_val,
                                                 Dispatcher* /* dispatcher */) {
  if (var.type == PP_VARTYPE_STRING) {
    // Convert the string to the context of the current process.
    return StringVar::StringToPPVar(str_val);
  }

  // See PluginVarSerialization::BeginSendPassRef for an example.
  if (var.type == PP_VARTYPE_OBJECT)
    PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
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

void HostVarSerializationRules::EndSendPassRef(const PP_Var& /* var */,
                                               Dispatcher* /* dispatcher */) {
  // See PluginVarSerialization::ReceivePassRef for an example. We don't need
  // to do anything here.
}

void HostVarSerializationRules::VarToString(const PP_Var& var,
                                            std::string* str) {
  DCHECK(var.type == PP_VARTYPE_STRING);

  // This could be optimized to avoid an extra string copy by going to a lower
  // level of the browser's implementation of strings where we already have
  // a std::string.
  StringVar* string_var = StringVar::FromPPVar(var);
  if (string_var)
    *str = string_var->value();
}

void HostVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
}

}  // namespace proxy
}  // namespace ppapi
