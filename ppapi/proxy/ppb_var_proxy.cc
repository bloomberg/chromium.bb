// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_var_proxy.h"

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {
namespace proxy {

namespace {

// PPP_Var plugin --------------------------------------------------------------

void AddRefVar(PP_Var var) {
  ppapi::ProxyAutoLock lock;
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
}

void ReleaseVar(PP_Var var) {
  ppapi::ProxyAutoLock lock;
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var);
}

PP_Var VarFromUtf8(PP_Module module, const char* data, uint32_t len) {
  ppapi::ProxyAutoLock lock;
  return StringVar::StringToPPVar(module, data, len);
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  ppapi::ProxyAutoLock lock;
  StringVar* str = StringVar::FromPPVar(var);
  if (str) {
    *len = static_cast<uint32_t>(str->value().size());
    return str->value().c_str();
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

}  // namespace

const PPB_Var* GetPPB_Var_Interface() {
  return &var_interface;
}

}  // namespace proxy
}  // namespace ppapi
