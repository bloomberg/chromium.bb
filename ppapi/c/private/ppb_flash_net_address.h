/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_flash_net_address.idl modified Fri Nov  4 12:47:53 2011. */

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_NET_ADDRESS_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_NET_ADDRESS_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_FLASH_NETADDRESS_INTERFACE_0_1 "PPB_Flash_NetAddress;0.1"
#define PPB_FLASH_NETADDRESS_INTERFACE PPB_FLASH_NETADDRESS_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_Flash_NetAddress</code> interface.
 */


/**
 * @addtogroup Structs
 * @{
 */
/**
 * This is an opaque type holding a network address.
 */
struct PP_Flash_NetAddress {
  uint32_t size;
  char data[128];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_Flash_NetAddress, 132);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Flash_NetAddress</code> interface provides operations on
 * network addresses.
 */
struct PPB_Flash_NetAddress {
  /**
   * Returns PP_TRUE if the two addresses are equal (host and port).
   */
  PP_Bool (*AreEqual)(const struct PP_Flash_NetAddress* addr1,
                      const struct PP_Flash_NetAddress* addr2);
  /**
   * Returns PP_TRUE if the two addresses refer to the same host.
   */
  PP_Bool (*AreHostsEqual)(const struct PP_Flash_NetAddress* addr1,
                           const struct PP_Flash_NetAddress* addr2);
  /**
   * Returns a human-readable description of the network address, optionally
   * including the port (e.g., "192.168.0.1", "192.168.0.1:99", or "[::1]:80"),
   * or an undefined var on failure.
   */
  struct PP_Var (*Describe)(PP_Module module,
                            const struct PP_Flash_NetAddress* addr,
                            PP_Bool include_port);
  /**
   * Replaces the port in the given source address. Returns PP_TRUE on success.
   */
  PP_Bool (*ReplacePort)(const struct PP_Flash_NetAddress* src_addr,
                         uint16_t port,
                         struct PP_Flash_NetAddress* dest_addr);
  /**
   * Gets the "any" address (for IPv4 or IPv6); for use with UDP Bind.
   */
  void (*GetAnyAddress)(PP_Bool is_ipv6, struct PP_Flash_NetAddress* addr);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FLASH_NET_ADDRESS_H_ */

