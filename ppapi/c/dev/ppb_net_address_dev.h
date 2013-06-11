/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_net_address_dev.idl modified Mon Jun 10 17:42:43 2013. */

#ifndef PPAPI_C_DEV_PPB_NET_ADDRESS_DEV_H_
#define PPAPI_C_DEV_PPB_NET_ADDRESS_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_NETADDRESS_DEV_INTERFACE_0_1 "PPB_NetAddress(Dev);0.1"
#define PPB_NETADDRESS_DEV_INTERFACE PPB_NETADDRESS_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_NetAddress_Dev</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /**
   * The address family is unspecified.
   */
  PP_NETADDRESS_FAMILY_UNSPECIFIED = 0,
  /**
   * The Internet Protocol version 4 (IPv4) address family.
   */
  PP_NETADDRESS_FAMILY_IPV4 = 1,
  /**
   * The Internet Protocol version 6 (IPv6) address family.
   */
  PP_NETADDRESS_FAMILY_IPV6 = 2
} PP_NetAddress_Family_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_NetAddress_Family_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * All members are expressed in network byte order.
 */
struct PP_NetAddress_IPv4_Dev {
  /**
   * Port number.
   */
  uint16_t port;
  /**
   * IPv4 address.
   */
  uint8_t addr[4];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_NetAddress_IPv4_Dev, 6);

/**
 * All members are expressed in network byte order.
 */
struct PP_NetAddress_IPv6_Dev {
  /**
   * Port number.
   */
  uint16_t port;
  /**
   * IPv6 address.
   */
  uint8_t addr[16];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_NetAddress_IPv6_Dev, 18);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_NetAddress_Dev</code> interface provides operations on
 * network addresses.
 */
struct PPB_NetAddress_Dev_0_1 {
  /**
   * Creates a <code>PPB_NetAddress_Dev</code> resource with the specified IPv4
   * address.
   */
  PP_Resource (*CreateFromIPv4Address)(
      PP_Instance instance,
      const struct PP_NetAddress_IPv4_Dev* ipv4_addr);
  /**
   * Creates a <code>PPB_NetAddress_Dev</code> resource with the specified IPv6
   * address.
   */
  PP_Resource (*CreateFromIPv6Address)(
      PP_Instance instance,
      const struct PP_NetAddress_IPv6_Dev* ipv6_addr);
  /**
   * Determines if a given resource is a network address.
   */
  PP_Bool (*IsNetAddress)(PP_Resource addr);
  /**
   * Gets the address family.
   */
  PP_NetAddress_Family_Dev (*GetFamily)(PP_Resource addr);
  /**
   * Returns a human-readable description of the network address. The
   * description is in the form of host [ ":" port ] and conforms to
   * http://tools.ietf.org/html/rfc3986#section-3.2 for IPv4 and IPv6 addresses
   * (e.g., "192.168.0.1", "192.168.0.1:99", or "[::1]:80").
   * Returns an undefined var on failure.
   */
  struct PP_Var (*DescribeAsString)(PP_Resource addr, PP_Bool include_port);
  /**
   * Fills a <code>PP_NetAddress_IPv4_Dev</code> structure if the network
   * address is of <code>PP_NETADDRESS_FAMILY_IPV4</code> address family.
   * Returns PP_FALSE on failure. Note that passing a network address of
   * <code>PP_NETADDRESS_FAMILY_IPV6</code> address family will fail even if the
   * address is an IPv4-mapped IPv6 address.
   */
  PP_Bool (*DescribeAsIPv4Address)(PP_Resource addr,
                                   struct PP_NetAddress_IPv4_Dev* ipv4_addr);
  /**
   * Fills a <code>PP_NetAddress_IPv6_Dev</code> structure if the network
   * address is of <code>PP_NETADDRESS_FAMILY_IPV6</code> address family.
   * Returns PP_FALSE on failure. Note that passing a network address of
   * <code>PP_NETADDRESS_FAMILY_IPV4</code> address family will fail - this
   * method doesn't map it to an IPv6 address.
   */
  PP_Bool (*DescribeAsIPv6Address)(PP_Resource addr,
                                   struct PP_NetAddress_IPv6_Dev* ipv6_addr);
};

typedef struct PPB_NetAddress_Dev_0_1 PPB_NetAddress_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_NET_ADDRESS_DEV_H_ */

