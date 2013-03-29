/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_network_monitor_private.idl,
 *   modified Thu Mar 28 10:30:11 2013.
 */

#ifndef PPAPI_C_PRIVATE_PPB_NETWORK_MONITOR_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NETWORK_MONITOR_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_NETWORKMONITOR_PRIVATE_INTERFACE_0_2 \
    "PPB_NetworkMonitor_Private;0.2"
#define PPB_NETWORKMONITOR_PRIVATE_INTERFACE \
    PPB_NETWORKMONITOR_PRIVATE_INTERFACE_0_2

/**
 * @file
 * This file defines the <code>PPB_NetworkMonitor_Private</code> interface.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * <code>PPB_NetworkMonitor_Callback</code> is a callback function
 * type that is used to receive notifications about network
 * configuration changes. The <code>network_list</code> passed to this
 * callback is a <code>PPB_NetworkList_Private</code> resource that
 * contains current configuration of network interfaces.
 */
typedef void (*PPB_NetworkMonitor_Callback)(void* user_data,
                                            PP_Resource network_list);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_NetworkMonitor_Private</code> provides access to
 * notifications of network configuration changes.
 */
struct PPB_NetworkMonitor_Private_0_2 {
  /**
   * Starts network change monitoring. The specified
   * <code>callback</code> will be called on the main thread once
   * after this method is called (to supply the initial network
   * configuration) and then later every time network configuration
   * changes. Notifications are stopped when the returned resource is
   * destroyed. If the plugin doesn't have access to the network list
   * then the callback will be called once with the
   * <code>network_list</code> parameter is set to 0.
   *
   * @param[in] callback The callback that will be called every time
   * network configuration changes or NULL to stop network monitoring.
   *
   * @param[inout] user_data The data to be passed to the callback on
   * each call.
   *
   * @return A <code>PP_Resource</code> containing the created
   * NetworkMonitor resource.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        PPB_NetworkMonitor_Callback callback,
                        void* user_data);
  /**
   * Determines if the specified <code>resource</code> is a
   * <code>NetworkMonitor</code> object.
   *
   * @param[in] resource A <code>PP_Resource</code> resource.
   *
   * @return Returns <code>PP_TRUE</code> if <code>resource</code> is
   * a <code>PPB_NetworkMonitor_Private</code>, <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsNetworkMonitor)(PP_Resource resource);
};

typedef struct PPB_NetworkMonitor_Private_0_2 PPB_NetworkMonitor_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_NETWORK_MONITOR_PRIVATE_H_ */

