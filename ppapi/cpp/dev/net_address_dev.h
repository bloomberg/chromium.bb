// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_NET_ADDRESS_DEV_H_
#define PPAPI_CPP_DEV_NET_ADDRESS_DEV_H_

#include "ppapi/c/dev/ppb_net_address_dev.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class InstanceHandle;

class NetAddress_Dev : public Resource {
 public:
  NetAddress_Dev();

  NetAddress_Dev(PassRef, PP_Resource resource);

  NetAddress_Dev(const InstanceHandle& instance,
                 const PP_NetAddress_IPv4_Dev& ipv4_addr);

  NetAddress_Dev(const InstanceHandle& instance,
                 const PP_NetAddress_IPv6_Dev& ipv6_addr);

  NetAddress_Dev(const NetAddress_Dev& other);

  virtual ~NetAddress_Dev();

  NetAddress_Dev& operator=(const NetAddress_Dev& other);

  /// Static function for determining whether the browser supports the required
  /// NetAddress interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  PP_NetAddress_Family_Dev GetFamily() const;

  Var DescribeAsString(bool include_port) const;

  bool DescribeAsIPv4Address(PP_NetAddress_IPv4_Dev* ipv4_addr) const;

  bool DescribeAsIPv6Address(PP_NetAddress_IPv6_Dev* ipv6_addr) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_NET_ADDRESS_DEV_H_
