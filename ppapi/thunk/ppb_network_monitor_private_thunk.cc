// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_network_monitor_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_NetworkMonitor_Private_API> EnterNetworkMonitor;

PP_Resource Create(PP_Instance instance,
                   PPB_NetworkMonitor_Callback callback,
                   void* user_data) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetworkMonitor(instance, callback, user_data);
}

PP_Bool IsNetworkMonitor(PP_Resource resource) {
  EnterNetworkMonitor enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

const PPB_NetworkMonitor_Private g_ppb_network_monitor_private_thunk = {
  &Create,
  &IsNetworkMonitor,
};

}  // namespace

const PPB_NetworkMonitor_Private_0_2*
GetPPB_NetworkMonitor_Private_0_2_Thunk() {
  return &g_ppb_network_monitor_private_thunk;
}

}  // namespace thunk
}  // namespace ppapi
