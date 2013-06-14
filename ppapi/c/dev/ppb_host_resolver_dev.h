/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_host_resolver_dev.idl modified Mon Jun 10 13:42:25 2013. */

#ifndef PPAPI_C_DEV_PPB_HOST_RESOLVER_DEV_H_
#define PPAPI_C_DEV_PPB_HOST_RESOLVER_DEV_H_

#include "ppapi/c/dev/ppb_net_address_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_HOSTRESOLVER_DEV_INTERFACE_0_1 "PPB_HostResolver(Dev);0.1"
#define PPB_HOSTRESOLVER_DEV_INTERFACE PPB_HOSTRESOLVER_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_HostResolver_Dev</code> interface.
 * TODO(yzshen): Tidy up the document.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_HostResolver_Flags_Dev</code> is an enumeration of the
 * different types of flags, that can be OR-ed and passed to host
 * resolver.
 */
typedef enum {
  /**
   * AI_CANONNAME
   */
  PP_HOSTRESOLVER_FLAGS_CANONNAME = 1 << 0,
  /**
   * Hint to the resolver that only loopback addresses are configured.
   */
  PP_HOSTRESOLVER_FLAGS_LOOPBACK_ONLY = 1 << 1
} PP_HostResolver_Flags_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_HostResolver_Flags_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_HostResolver_Hint_Dev {
  PP_NetAddress_Family_Dev family;
  int32_t flags;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_HostResolver_Hint_Dev, 8);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_HostResolver_Dev_0_1 {
  /**
   * Allocates a Host Resolver resource.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a Host Resolver.
   */
  PP_Bool (*IsHostResolver)(PP_Resource resource);
  /**
   * Creates a new request to Host Resolver. |callback| is invoked when request
   * is processed and a list of network addresses is obtained. These addresses
   * can be used in Connect, Bind or Listen calls to connect to a given |host|
   * and |port|.
   */
  int32_t (*Resolve)(PP_Resource host_resolver,
                     const char* host,
                     uint16_t port,
                     const struct PP_HostResolver_Hint_Dev* hint,
                     struct PP_CompletionCallback callback);
  /**
   * Gets canonical name of host. Returns an undefined var if there is a pending
   * Resolve call or the previous Resolve call failed.
   */
  struct PP_Var (*GetCanonicalName)(PP_Resource host_resolver);
  /**
   * Gets number of network addresses obtained after Resolve call. Returns 0 if
   * there is a pending Resolve call or the previous Resolve call failed.
   */
  uint32_t (*GetNetAddressCount)(PP_Resource host_resolver);
  /**
   * Gets the |index|-th network address.
   * Returns 0 if there is a pending Resolve call or the previous Resolve call
   * failed, or |index| is not less than the number of available addresses.
   */
  PP_Resource (*GetNetAddress)(PP_Resource host_resolver, uint32_t index);
};

typedef struct PPB_HostResolver_Dev_0_1 PPB_HostResolver_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_HOST_RESOLVER_DEV_H_ */

