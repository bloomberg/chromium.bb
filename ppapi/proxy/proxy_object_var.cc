// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/proxy_object_var.h"

#include "base/logging.h"
#include "ppapi/c/pp_var.h"

using pp::proxy::PluginDispatcher;

namespace ppapi {

ProxyObjectVar::ProxyObjectVar(PluginDispatcher* dispatcher,
                               int32 host_var_id)
    : Var(0),
      dispatcher_(dispatcher),
      host_var_id_(host_var_id) {
  // Should be given valid objects or we'll crash later.
  DCHECK(dispatcher_);
  DCHECK(host_var_id_);
}

ProxyObjectVar::~ProxyObjectVar() {
}

ProxyObjectVar* ProxyObjectVar::AsProxyObjectVar() {
  return this;
}

PP_Var ProxyObjectVar::GetPPVar() {
  int32 id = GetOrCreateVarID();
  if (!id)
    return PP_MakeNull();

  PP_Var result;
  result.type = PP_VARTYPE_OBJECT;
  result.value.as_id = id;
  return result;
}

PP_VarType ProxyObjectVar::GetType() const {
  return PP_VARTYPE_OBJECT;
}

void ProxyObjectVar::AssignVarID(int32 id) {
  return Var::AssignVarID(id);
}

}  // namespace ppapi
