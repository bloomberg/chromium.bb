// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppb_network_list_private_shared.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

NetworkInfo::NetworkInfo()
    : type(PP_NETWORKLIST_UNKNOWN),
      state(PP_NETWORKLIST_DOWN),
      mtu(0) {
}

NetworkInfo::~NetworkInfo() {
}

NetworkListStorage::NetworkListStorage(const NetworkList& list)
    : list_(list) {
}

NetworkListStorage::~NetworkListStorage() {
}

PPB_NetworkList_Private_Shared::PPB_NetworkList_Private_Shared(
    ResourceObjectType type,
    PP_Instance instance,
    const scoped_refptr<NetworkListStorage>& list)
    : Resource(type, instance),
      list_(list) {
}

PPB_NetworkList_Private_Shared::~PPB_NetworkList_Private_Shared() {
}

// static
PP_Resource PPB_NetworkList_Private_Shared::Create(
    ResourceObjectType type,
    PP_Instance instance,
    const scoped_refptr<NetworkListStorage>& list) {
  scoped_refptr<PPB_NetworkList_Private_Shared> object(
      new PPB_NetworkList_Private_Shared(type, instance, list));
  return object->GetReference();
}

::ppapi::thunk::PPB_NetworkList_API*
PPB_NetworkList_Private_Shared::AsPPB_NetworkList_API() {
  return this;
}

const NetworkList& PPB_NetworkList_Private_Shared::GetNetworkListData() const {
  return list_->list();
}

uint32_t PPB_NetworkList_Private_Shared::GetCount() {
  return static_cast<uint32_t>(list_->list().size());
}

PP_Var PPB_NetworkList_Private_Shared::GetName(uint32_t index) {
  if (index >= list_->list().size())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(list_->list().at(index).name);
}

PP_NetworkListType_Private PPB_NetworkList_Private_Shared::GetType(
    uint32_t index) {
  if (index >= list_->list().size())
    return PP_NETWORKLIST_UNKNOWN;
  return list_->list().at(index).type;
}

PP_NetworkListState_Private PPB_NetworkList_Private_Shared::GetState(
    uint32_t index) {
  if (index >= list_->list().size())
    return PP_NETWORKLIST_DOWN;
  return list_->list().at(index).state;
}

int32_t PPB_NetworkList_Private_Shared::GetIpAddresses(
    uint32_t index,
    struct PP_NetAddress_Private addresses[],
    uint32_t count) {
  if (index >= list_->list().size())
    return PP_ERROR_FAILED;
  count = std::min(
      count, static_cast<uint32_t>(list_->list().at(index).addresses.size()));
  memcpy(addresses, &(list_->list().at(index).addresses[0]),
         sizeof(PP_NetAddress_Private) * count);
  return static_cast<int32_t>(list_->list().at(index).addresses.size());
}

PP_Var PPB_NetworkList_Private_Shared::GetDisplayName(uint32_t index) {
  if (index >= list_->list().size())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(list_->list().at(index).display_name);
}

uint32_t PPB_NetworkList_Private_Shared::GetMTU(uint32_t index) {
  if (index >= list_->list().size())
    return 0;
  return list_->list().at(index).mtu;
}

}  // namespace thunk
