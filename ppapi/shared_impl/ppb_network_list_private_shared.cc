// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppb_network_list_private_shared.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

PPB_NetworkList_Private_Shared::NetworkInfo::NetworkInfo()
    : type(PP_NETWORKLIST_UNKNOWN),
      state(PP_NETWORKLIST_DOWN),
      mtu(0) {
}

PPB_NetworkList_Private_Shared::NetworkInfo::~NetworkInfo() {
}

PPB_NetworkList_Private_Shared::PPB_NetworkList_Private_Shared(
    ResourceObjectType type,
    PP_Instance instance,
    scoped_ptr<NetworkList> list)
    : Resource(type, instance),
      list_(list.Pass()) {
}

PPB_NetworkList_Private_Shared::~PPB_NetworkList_Private_Shared() {
}

// static
PP_Resource PPB_NetworkList_Private_Shared::Create(
    ResourceObjectType type,
    PP_Instance instance,
    scoped_ptr<NetworkList> list) {
  scoped_refptr<PPB_NetworkList_Private_Shared> object(
      new PPB_NetworkList_Private_Shared(type, instance, list.Pass()));
  return object->GetReference();
}

::ppapi::thunk::PPB_NetworkList_Private_API*
PPB_NetworkList_Private_Shared::AsPPB_NetworkList_Private_API() {
  return this;
}

uint32_t PPB_NetworkList_Private_Shared::GetCount() {
  return list_->size();
}

PP_Var PPB_NetworkList_Private_Shared::GetName(uint32_t index) {
  if (index >= list_->size())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(list_->at(index).name);
}

PP_NetworkListType_Private PPB_NetworkList_Private_Shared::GetType(
    uint32_t index) {
  if (index >= list_->size())
    return PP_NETWORKLIST_UNKNOWN;
  return list_->at(index).type;
}

PP_NetworkListState_Private PPB_NetworkList_Private_Shared::GetState(
    uint32_t index) {
  if (index >= list_->size())
    return PP_NETWORKLIST_DOWN;
  return list_->at(index).state;
}

int32_t PPB_NetworkList_Private_Shared::GetIpAddresses(
    uint32_t index,
    struct PP_NetAddress_Private addresses[],
    uint32_t count) {
  if (index >= list_->size())
    return PP_ERROR_FAILED;
  count = std::min(
      count, static_cast<uint32_t>(list_->at(index).addresses.size()));
  memcpy(addresses, &(list_->at(index).addresses[0]),
         sizeof(PP_NetAddress_Private) * count);
  return list_->at(index).addresses.size();
}

PP_Var PPB_NetworkList_Private_Shared::GetDisplayName(uint32_t index) {
  if (index >= list_->size())
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(list_->at(index).display_name);
}

uint32_t PPB_NetworkList_Private_Shared::GetMTU(uint32_t index) {
  if (index >= list_->size())
    return 0;
  return list_->at(index).mtu;
}

}  // namespace thunk
