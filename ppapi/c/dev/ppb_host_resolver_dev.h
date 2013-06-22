/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_host_resolver_dev.idl modified Thu Jun 20 12:08:29 2013. */

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
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * <code>PP_HostResolver_Flags_Dev</code> is an enumeration of flags which can
 * be OR-ed and passed to the host resolver. Currently there is only one flag
 * defined.
 */
typedef enum {
  /**
   * Hint to request the canonical name of the host, which can be retrieved by
   * <code>GetCanonicalName()</code>.
   */
  PP_HOSTRESOLVER_FLAGS_CANONNAME = 1 << 0
} PP_HostResolver_Flags_Dev;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_HostResolver_Flags_Dev, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
/**
 * <code>PP_HostResolver_Hint_Dev</code> represents hints for host resolution.
 */
struct PP_HostResolver_Hint_Dev {
  /**
   * Network address family.
   */
  PP_NetAddress_Family_Dev family;
  /**
   * Combination of flags from <code>PP_HostResolver_Flags_Dev</code>.
   */
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
/**
 * The <code>PPB_HostResolver_Dev</code> interface supports host name
 * resolution.
 *
 * Permissions: In order to run <code>Resolve()</code>, apps permission
 * <code>socket</code> with subrule <code>resolve-host</code> is required.
 * For more details about network communication permissions, please see:
 * http://developer.chrome.com/apps/app_network.html
 */
struct PPB_HostResolver_Dev_0_1 {
  /**
   * Creates a host resolver resource.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance of
   * a module.
   *
   * @return A <code>PP_Resource</code> corresponding to a host reslover or 0
   * on failure.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if a given resource is a host resolver.
   *
   * @param[in] resource A <code>PP_Resource</code> to check.
   *
   * @return <code>PP_TRUE</code> if the input is a
   * <code>PPB_HostResolver_Dev</code> resource; <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsHostResolver)(PP_Resource resource);
  /**
   * Requests resolution of a host name. If the call completes successfully, the
   * results can be retrieved by <code>GetCanonicalName()</code>,
   * <code>GetNetAddressCount()</code> and <code>GetNetAddress()</code>.
   *
   * @param[in] host_resolver A <code>PP_Resource</code> corresponding to a host
   * resolver.
   * @param[in] host The host name (or IP address literal) to resolve.
   * @param[in] port The port number to be set in the resulting network
   * addresses.
   * @param[in] hint A <code>PP_HostResolver_Hint_Dev</code> structure providing
   * hints for host resolution.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   * <code>PP_ERROR_NOACCESS</code> will be returned if the caller doesn't have
   * required permissions. <code>PP_ERROR_NAME_NOT_RESOLVED</code> will be
   * returned if the host name couldn't be resolved.
   */
  int32_t (*Resolve)(PP_Resource host_resolver,
                     const char* host,
                     uint16_t port,
                     const struct PP_HostResolver_Hint_Dev* hint,
                     struct PP_CompletionCallback callback);
  /**
   * Gets the canonical name of the host.
   *
   * @param[in] host_resolver A <code>PP_Resource</code> corresponding to a host
   * resolver.
   *
   * @return A string <code>PP_Var</code> on success, which is an empty string
   * if <code>PP_HOSTRESOLVER_FLAGS_CANONNAME</code> is not set in the hint
   * flags when calling <code>Resolve()</code>; an undefined <code>PP_Var</code>
   * if there is a pending <code>Resolve()</code> call or the previous
   * <code>Resolve()</code> call failed.
   */
  struct PP_Var (*GetCanonicalName)(PP_Resource host_resolver);
  /**
   * Gets the number of network addresses.
   *
   * @param[in] host_resolver A <code>PP_Resource</code> corresponding to a host
   * resolver.
   *
   * @return The number of available network addresses on success; 0 if there is
   * a pending <code>Resolve()</code> call or the previous
   * <code>Resolve()</code> call failed.
   */
  uint32_t (*GetNetAddressCount)(PP_Resource host_resolver);
  /**
   * Gets a network address.
   *
   * @param[in] host_resolver A <code>PP_Resource</code> corresponding to a host
   * resolver.
   * @param[in] index An index indicating which address to return.
   *
   * @return A <code>PPB_NetAddress_Dev</code> resource on success; 0 if there
   * is a pending <code>Resolve()</code> call or the previous
   * <code>Resolve()</code> call failed, or the specified index is out of range.
   */
  PP_Resource (*GetNetAddress)(PP_Resource host_resolver, uint32_t index);
};

typedef struct PPB_HostResolver_Dev_0_1 PPB_HostResolver_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_HOST_RESOLVER_DEV_H_ */

