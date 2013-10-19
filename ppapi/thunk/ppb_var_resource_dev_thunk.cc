// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_var_resource_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource VarToResource(struct PP_Var var) {
  ProxyAutoLock lock;
  ResourceVar* resource = ResourceVar::FromPPVar(var);
  if (!resource)
    return 0;
  PP_Resource pp_resource = resource->GetPPResource();
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(pp_resource);
  return pp_resource;
}

struct PP_Var VarFromResource(PP_Resource resource) {
  ProxyAutoLock lock;
  return PpapiGlobals::Get()->GetVarTracker()->MakeResourcePPVar(resource);
}

const PPB_VarResource_Dev_0_1 g_ppb_varresource_dev_0_1_thunk = {
  &VarToResource,
  &VarFromResource
};

}  // namespace

const PPB_VarResource_Dev_0_1* GetPPB_VarResource_Dev_0_1_Thunk() {
  return &g_ppb_varresource_dev_0_1_thunk;
}

}  // namespace thunk
}  // namespace ppapi
