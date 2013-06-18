// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/net_address_dev.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetAddress_Dev_0_1>() {
  return PPB_NETADDRESS_DEV_INTERFACE_0_1;
}

}  // namespace

NetAddress_Dev::NetAddress_Dev() {
}

NetAddress_Dev::NetAddress_Dev(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

NetAddress_Dev::NetAddress_Dev(const InstanceHandle& instance,
                               const PP_NetAddress_IPv4_Dev& ipv4_addr) {
  if (has_interface<PPB_NetAddress_Dev_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_NetAddress_Dev_0_1>()->CreateFromIPv4Address(
            instance.pp_instance(), &ipv4_addr));
  }
}

NetAddress_Dev::NetAddress_Dev(const InstanceHandle& instance,
                               const PP_NetAddress_IPv6_Dev& ipv6_addr) {
  if (has_interface<PPB_NetAddress_Dev_0_1>()) {
    PassRefFromConstructor(
        get_interface<PPB_NetAddress_Dev_0_1>()->CreateFromIPv6Address(
            instance.pp_instance(), &ipv6_addr));
  }
}

NetAddress_Dev::NetAddress_Dev(const NetAddress_Dev& other) : Resource(other) {
}

NetAddress_Dev::~NetAddress_Dev() {
}

NetAddress_Dev& NetAddress_Dev::operator=(const NetAddress_Dev& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool NetAddress_Dev::IsAvailable() {
  return has_interface<PPB_NetAddress_Dev_0_1>();
}

PP_NetAddress_Family_Dev NetAddress_Dev::GetFamily() const {
  if (has_interface<PPB_NetAddress_Dev_0_1>())
    return get_interface<PPB_NetAddress_Dev_0_1>()->GetFamily(pp_resource());

  return PP_NETADDRESS_FAMILY_UNSPECIFIED;
}

Var NetAddress_Dev::DescribeAsString(bool include_port) const {
  if (has_interface<PPB_NetAddress_Dev_0_1>()) {
    return Var(PASS_REF,
               get_interface<PPB_NetAddress_Dev_0_1>()->DescribeAsString(
                   pp_resource(), PP_FromBool(include_port)));
  }

  return Var();
}

bool NetAddress_Dev::DescribeAsIPv4Address(
    PP_NetAddress_IPv4_Dev* ipv4_addr) const {
  if (has_interface<PPB_NetAddress_Dev_0_1>()) {
    return PP_ToBool(
        get_interface<PPB_NetAddress_Dev_0_1>()->DescribeAsIPv4Address(
            pp_resource(), ipv4_addr));
  }

  return false;
}

bool NetAddress_Dev::DescribeAsIPv6Address(
    PP_NetAddress_IPv6_Dev* ipv6_addr) const {
  if (has_interface<PPB_NetAddress_Dev_0_1>()) {
    return PP_ToBool(
        get_interface<PPB_NetAddress_Dev_0_1>()->DescribeAsIPv6Address(
            pp_resource(), ipv6_addr));
  }

  return false;
}

}  // namespace pp
