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

PP_Var HostVarSerializationRules::SendCallerOwned(const PP_Var& var) {
  return var;
}

PP_Var HostVarSerializationRules::BeginReceiveCallerOwned(
    const PP_Var& var,
    Dispatcher* /* dispatcher */) {
  return var;
}

void HostVarSerializationRules::EndReceiveCallerOwned(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    // Destroy the string.
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
  }
}

PP_Var HostVarSerializationRules::ReceivePassRef(
    const PP_Var& var,
    Dispatcher* /* dispatcher */) {
  // See PluginVarSerialization::BeginSendPassRef for an example.
  if (var.type == PP_VARTYPE_OBJECT)
    PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
  return var;
}

PP_Var HostVarSerializationRules::BeginSendPassRef(const PP_Var& var) {
  return var;
}

void HostVarSerializationRules::EndSendPassRef(const PP_Var& /* var */,
                                               Dispatcher* /* dispatcher */) {
  // See PluginVarSerialization::ReceivePassRef for an example. We don't need
  // to do anything here.
}

void HostVarSerializationRules::ReleaseObjectRef(const PP_Var& var) {
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
}

}  // namespace proxy
}  // namespace ppapi
