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

/// The <code>NetAddress_Dev</code> class represents a network address.
class NetAddress_Dev : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>NetAddress_Dev</code>
  /// object.
  NetAddress_Dev();

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_NetAddress_Dev</code> resource.
  NetAddress_Dev(PassRef, PP_Resource resource);

  /// A constructor used to create a <code>NetAddress_Dev</code> object with the
  /// specified IPv4 address.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  /// @param[in] ipv4_addr An IPv4 address.
  NetAddress_Dev(const InstanceHandle& instance,
                 const PP_NetAddress_IPv4_Dev& ipv4_addr);

  /// A constructor used to create a <code>NetAddress_Dev</code> object with the
  /// specified IPv6 address.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  /// @param[in] ipv6_addr An IPv6 address.
  NetAddress_Dev(const InstanceHandle& instance,
                 const PP_NetAddress_IPv6_Dev& ipv6_addr);

  /// The copy constructor for <code>NetAddress_Dev</code>.
  ///
  /// @param[in] other A reference to another <code>NetAddress_Dev</code>.
  NetAddress_Dev(const NetAddress_Dev& other);

  /// The destructor.
  virtual ~NetAddress_Dev();

  /// The assignment operator for <code>NetAddress_Dev</code>.
  ///
  /// @param[in] other A reference to another <code>NetAddress_Dev</code>.
  ///
  /// @return A reference to this <code>NetAddress_Dev</code> object.
  NetAddress_Dev& operator=(const NetAddress_Dev& other);

  /// Static function for determining whether the browser supports the
  /// <code>PPB_NetAddress_Dev</code> interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  /// Gets the address family.
  ///
  /// @return The address family on success;
  /// <code>PP_NETADDRESS_FAMILY_UNSPECIFIED</code> on failure.
  PP_NetAddress_Family_Dev GetFamily() const;

  /// Returns a human-readable description of the network address. The
  /// description is in the form of host [ ":" port ] and conforms to
  /// http://tools.ietf.org/html/rfc3986#section-3.2 for IPv4 and IPv6 addresses
  /// (e.g., "192.168.0.1", "192.168.0.1:99", or "[::1]:80").
  ///
  /// @param[in] include_port Whether to include the port number in the
  /// description.
  ///
  /// @return A string <code>Var</code> on success; an undefined
  /// <code>Var</code> on failure.
  Var DescribeAsString(bool include_port) const;

  /// Fills a <code>PP_NetAddress_IPv4_Dev</code> structure if the network
  /// address is of <code>PP_NETADDRESS_FAMILY_IPV4</code> address family.
  /// Note that passing a network address of
  /// <code>PP_NETADDRESS_FAMILY_IPV6</code> address family will fail even if
  /// the address is an IPv4-mapped IPv6 address.
  ///
  /// @param[out] ipv4_addr A <code>PP_NetAddress_IPv4_Dev</code> structure to
  /// store the result.
  ///
  /// @return A boolean value indicating whether the operation succeeded.
  bool DescribeAsIPv4Address(PP_NetAddress_IPv4_Dev* ipv4_addr) const;

  /// Fills a <code>PP_NetAddress_IPv6_Dev</code> structure if the network
  /// address is of <code>PP_NETADDRESS_FAMILY_IPV6</code> address family.
  /// Note that passing a network address of
  /// <code>PP_NETADDRESS_FAMILY_IPV4</code> address family will fail - this
  /// method doesn't map it to an IPv6 address.
  ///
  /// @param[out] ipv6_addr A <code>PP_NetAddress_IPv6_Dev</code> structure to
  /// store the result.
  ///
  /// @return A boolean value indicating whether the operation succeeded.
  bool DescribeAsIPv6Address(PP_NetAddress_IPv6_Dev* ipv6_addr) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_NET_ADDRESS_DEV_H_
