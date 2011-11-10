/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_net_address_private.idl modified Wed Nov  9 12:53:35 2011. */

#ifndef PPAPI_C_PRIVATE_PPB_NET_ADDRESS_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NET_ADDRESS_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_NETADDRESS_PRIVATE_INTERFACE_0_1 "PPB_NetAddress_Private;0.1"
#define PPB_NETADDRESS_PRIVATE_INTERFACE PPB_NETADDRESS_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_NetAddress_Private</code> interface.
 */


/**
 * @addtogroup Structs
 * @{
 */
/**
 * This is an opaque type holding a network address.
 */
struct PP_NetAddress_Private {
  uint32_t size;
  char data[128];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_NetAddress_Private, 132);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_NetAddress_Private</code> interface provides operations on
 * network addresses.
 */
struct PPB_NetAddress_Private {
  /**
   * Returns PP_TRUE if the two addresses are equal (host and port).
   */
  PP_Bool (*AreEqual)(const struct PP_NetAddress_Private* addr1,
                      const struct PP_NetAddress_Private* addr2);
  /**
   * Returns PP_TRUE if the two addresses refer to the same host.
   */
  PP_Bool (*AreHostsEqual)(const struct PP_NetAddress_Private* addr1,
                           const struct PP_NetAddress_Private* addr2);
  /**
   * Returns a human-readable description of the network address, optionally
   * including the port (e.g., "192.168.0.1", "192.168.0.1:99", or "[::1]:80"),
   * or an undefined var on failure.
   */
  struct PP_Var (*Describe)(PP_Module module,
                            const struct PP_NetAddress_Private* addr,
                            PP_Bool include_port);
  /**
   * Replaces the port in the given source address. Returns PP_TRUE on success.
   */
  PP_Bool (*ReplacePort)(const struct PP_NetAddress_Private* src_addr,
                         uint16_t port,
                         struct PP_NetAddress_Private* dest_addr);
  /**
   * Gets the "any" address (for IPv4 or IPv6); for use with UDP Bind.
   */
  void (*GetAnyAddress)(PP_Bool is_ipv6, struct PP_NetAddress_Private* addr);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_NET_ADDRESS_PRIVATE_H_ */

