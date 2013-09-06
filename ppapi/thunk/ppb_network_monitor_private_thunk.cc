// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_network_monitor_private.idl,
//   modified Wed Sep  4 14:16:07 2013.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_network_monitor_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_NetworkMonitor_Private::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateNetworkMonitorPrivate(instance);
}

int32_t UpdateNetworkList(PP_Resource network_monitor,
                          PP_Resource* network_list,
                          struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_NetworkMonitor_Private::UpdateNetworkList()";
  EnterResource<PPB_NetworkMonitor_API> enter(network_monitor, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->UpdateNetworkList(network_list,
                                                           enter.callback()));
}

PP_Bool IsNetworkMonitor(PP_Resource resource) {
  VLOG(4) << "PPB_NetworkMonitor_Private::IsNetworkMonitor()";
  EnterResource<PPB_NetworkMonitor_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

const PPB_NetworkMonitor_Private_0_3 g_ppb_networkmonitor_private_thunk_0_3 = {
  &Create,
  &UpdateNetworkList,
  &IsNetworkMonitor
};

}  // namespace

const PPB_NetworkMonitor_Private_0_3*
    GetPPB_NetworkMonitor_Private_0_3_Thunk() {
  return &g_ppb_networkmonitor_private_thunk_0_3;
}

}  // namespace thunk
}  // namespace ppapi
