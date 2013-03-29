// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/network_list_private.h"

#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkList_Private>() {
  return PPB_NETWORKLIST_PRIVATE_INTERFACE;
}

}  // namespace

NetworkListPrivate::NetworkListPrivate() {
}

NetworkListPrivate::NetworkListPrivate(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

// static
bool NetworkListPrivate::IsAvailable() {
  return has_interface<PPB_NetworkList_Private>();
}

uint32_t NetworkListPrivate::GetCount() const {
  if (!has_interface<PPB_NetworkList_Private>())
    return 0;
  return get_interface<PPB_NetworkList_Private>()->GetCount(pp_resource());
}

std::string NetworkListPrivate::GetName(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private>())
    return std::string();
  Var result(PASS_REF,
             get_interface<PPB_NetworkList_Private>()->GetName(
                 pp_resource(), index));
  return result.is_string() ? result.AsString() : std::string();
}

PP_NetworkListType_Private NetworkListPrivate::GetType(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private>())
    return PP_NETWORKLIST_ETHERNET;
  return get_interface<PPB_NetworkList_Private>()->GetType(
      pp_resource(), index);
}

PP_NetworkListState_Private NetworkListPrivate::GetState(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private>())
    return PP_NETWORKLIST_DOWN;
  return get_interface<PPB_NetworkList_Private>()->GetState(
      pp_resource(), index);
}

void NetworkListPrivate::GetIpAddresses(
    uint32_t index,
    std::vector<PP_NetAddress_Private>* addresses) const {
  if (!has_interface<PPB_NetworkList_Private>())
    return;

  // Most network interfaces don't have more than 3 network
  // interfaces.
  addresses->resize(3);

  int32_t result = get_interface<PPB_NetworkList_Private>()->GetIpAddresses(
      pp_resource(), index, &addresses->front(), addresses->size());

  if (result < 0) {
    addresses->resize(0);
    return;
  }

  if (result <= static_cast<int32_t>(addresses->size())) {
    addresses->resize(result);
    return;
  }

  addresses->resize(result);
  result = get_interface<PPB_NetworkList_Private>()->GetIpAddresses(
      pp_resource(), index, &addresses->front(), addresses->size());
  if (result < 0) {
    addresses->resize(0);
  } else if (result < static_cast<int32_t>(addresses->size())) {
    addresses->resize(result);
  }
}

std::string NetworkListPrivate::GetDisplayName(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private>())
    return std::string();
  Var result(PASS_REF,
             get_interface<PPB_NetworkList_Private>()->GetDisplayName(
                 pp_resource(), index));
  return result.is_string() ? result.AsString() : std::string();
}

uint32_t NetworkListPrivate::GetMTU(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private>())
    return 0;
  return get_interface<PPB_NetworkList_Private>()->GetMTU(
      pp_resource(), index);
}

}  // namespace pp
