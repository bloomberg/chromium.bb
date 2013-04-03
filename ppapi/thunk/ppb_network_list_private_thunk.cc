// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_network_list_private.idl,
//   modified Wed Apr  3 10:41:20 2013.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_network_list_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/ppb_network_list_private_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsNetworkList(PP_Resource resource) {
  VLOG(4) << "PPB_NetworkList_Private::IsNetworkList()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

uint32_t GetCount(PP_Resource resource) {
  VLOG(4) << "PPB_NetworkList_Private::GetCount()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetCount();
}

struct PP_Var GetName(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList_Private::GetName()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetName(index);
}

PP_NetworkListType_Private GetType(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList_Private::GetType()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return PP_NETWORKLIST_UNKNOWN;
  return enter.object()->GetType(index);
}

PP_NetworkListState_Private GetState(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList_Private::GetState()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return PP_NETWORKLIST_DOWN;
  return enter.object()->GetState(index);
}

int32_t GetIpAddresses(PP_Resource resource,
                       uint32_t index,
                       struct PP_NetAddress_Private addresses[],
                       uint32_t count) {
  VLOG(4) << "PPB_NetworkList_Private::GetIpAddresses()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetIpAddresses(index, addresses, count);
}

struct PP_Var GetDisplayName(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList_Private::GetDisplayName()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.object()->GetDisplayName(index);
}

uint32_t GetMTU(PP_Resource resource, uint32_t index) {
  VLOG(4) << "PPB_NetworkList_Private::GetMTU()";
  EnterResource<PPB_NetworkList_Private_API> enter(resource, true);
  if (enter.failed())
    return 0;
  return enter.object()->GetMTU(index);
}

const PPB_NetworkList_Private_0_2 g_ppb_networklist_private_thunk_0_2 = {
  &IsNetworkList,
  &GetCount,
  &GetName,
  &GetType,
  &GetState,
  &GetIpAddresses,
  &GetDisplayName,
  &GetMTU
};

}  // namespace

const PPB_NetworkList_Private_0_2* GetPPB_NetworkList_Private_0_2_Thunk() {
  return &g_ppb_networklist_private_thunk_0_2;
}

}  // namespace thunk
}  // namespace ppapi
