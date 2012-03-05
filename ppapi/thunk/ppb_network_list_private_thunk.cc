// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_network_list_private_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_NetworkList_Private_API> EnterNetworkList;

PP_Bool IsNetworkList(PP_Resource resource) {
  EnterNetworkList enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

uint32_t GetCount(PP_Resource network_list) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCount();
}

PP_Var GetName(PP_Resource network_list, uint32_t index) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName(index);
}

PP_NetworkListType_Private GetType(PP_Resource network_list, uint32_t index) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return PP_NETWORKLIST_UNKNOWN;
  return enter.object()->GetType(index);
}

PP_NetworkListState_Private GetState(PP_Resource network_list, uint32_t index) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return PP_NETWORKLIST_DOWN;
  return enter.object()->GetState(index);
}

int32_t GetIpAddresses(PP_Resource network_list,
                       uint32_t index,
                       struct PP_NetAddress_Private addresses[],
                       uint32_t count) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetIpAddresses(index, addresses, count);
}

PP_Var GetDisplayName(PP_Resource network_list, uint32_t index) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetDisplayName(index);
}

uint32_t GetMTU(PP_Resource network_list, uint32_t index) {
  EnterNetworkList enter(network_list, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetMTU(index);
}

const PPB_NetworkList_Private g_ppb_network_list_private_thunk = {
  &IsNetworkList,
  &GetCount,
  &GetName,
  &GetType,
  &GetState,
  &GetIpAddresses,
  &GetDisplayName,
  &GetMTU,
};

}  // namespace

const PPB_NetworkList_Private_0_2* GetPPB_NetworkList_Private_0_2_Thunk() {
  return &g_ppb_network_list_private_thunk;
}

}  // namespace thunk
}  // namespace ppapi
