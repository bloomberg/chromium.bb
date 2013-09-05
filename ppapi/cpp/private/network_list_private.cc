// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/network_list_private.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/array_output.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkList_Private_0_3>() {
  return PPB_NETWORKLIST_PRIVATE_INTERFACE_0_3;
}

}  // namespace

NetworkListPrivate::NetworkListPrivate() {
}

NetworkListPrivate::NetworkListPrivate(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

// static
bool NetworkListPrivate::IsAvailable() {
  return has_interface<PPB_NetworkList_Private_0_3>();
}

uint32_t NetworkListPrivate::GetCount() const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return 0;
  return get_interface<PPB_NetworkList_Private_0_3>()->GetCount(pp_resource());
}

std::string NetworkListPrivate::GetName(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return std::string();
  Var result(PASS_REF,
             get_interface<PPB_NetworkList_Private_0_3>()->GetName(
                 pp_resource(), index));
  return result.is_string() ? result.AsString() : std::string();
}

PP_NetworkListType_Private NetworkListPrivate::GetType(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return PP_NETWORKLIST_ETHERNET;
  return get_interface<PPB_NetworkList_Private_0_3>()->GetType(
      pp_resource(), index);
}

PP_NetworkListState_Private NetworkListPrivate::GetState(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return PP_NETWORKLIST_DOWN;
  return get_interface<PPB_NetworkList_Private_0_3>()->GetState(
      pp_resource(), index);
}

int32_t NetworkListPrivate::GetIpAddresses(
    uint32_t index,
    std::vector<NetAddress>* addresses) const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return PP_ERROR_NOINTERFACE;
  if (!addresses)
    return PP_ERROR_BADARGUMENT;

  ResourceArrayOutputAdapter<NetAddress> adapter(addresses);
  return get_interface<PPB_NetworkList_Private_0_3>()->GetIpAddresses(
      pp_resource(), index, adapter.pp_array_output());
}

std::string NetworkListPrivate::GetDisplayName(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return std::string();
  Var result(PASS_REF,
             get_interface<PPB_NetworkList_Private_0_3>()->GetDisplayName(
                 pp_resource(), index));
  return result.is_string() ? result.AsString() : std::string();
}

uint32_t NetworkListPrivate::GetMTU(uint32_t index) const {
  if (!has_interface<PPB_NetworkList_Private_0_3>())
    return 0;
  return get_interface<PPB_NetworkList_Private_0_3>()->GetMTU(
      pp_resource(), index);
}

}  // namespace pp
