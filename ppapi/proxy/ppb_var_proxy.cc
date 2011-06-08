// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_var_proxy.h"

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/plugin_var_tracker.h"

namespace pp {
namespace proxy {

namespace {

// PPP_Var plugin --------------------------------------------------------------

void AddRefVar(PP_Var var) {
  PluginVarTracker::GetInstance()->AddRef(var);
}

void ReleaseVar(PP_Var var) {
  PluginVarTracker::GetInstance()->Release(var);
}

PP_Var VarFromUtf8(PP_Module module, const char* data, uint32_t len) {
  PP_Var ret = {};
  ret.type = PP_VARTYPE_STRING;
  ret.value.as_id = PluginVarTracker::GetInstance()->MakeString(data, len);
  return ret;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  const std::string* str =
      PluginVarTracker::GetInstance()->GetExistingString(var);
  if (str) {
    *len = static_cast<uint32_t>(str->size());
    return str->c_str();
  }
  *len = 0;
  return NULL;
}

const PPB_Var var_interface = {
  &AddRefVar,
  &ReleaseVar,
  &VarFromUtf8,
  &VarToUtf8
};

InterfaceProxy* CreateVarProxy(Dispatcher* dispatcher,
                               const void* target_interface) {
  return new PPB_Var_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Var_Proxy::PPB_Var_Proxy(Dispatcher* dispatcher,
                             const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Var_Proxy::~PPB_Var_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Var_Proxy::GetInfo() {
  static const Info info = {
    &var_interface,
    PPB_VAR_INTERFACE,
    INTERFACE_ID_PPB_VAR,
    false,
    &CreateVarProxy,
  };
  return &info;
}

bool PPB_Var_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // All PPB_Var calls are handled locally; there is no need to send or receive
  // messages here.
  return false;
}

}  // namespace proxy
}  // namespace pp
