/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_network_list_private.idl,
 *   modified Fri Feb 17 11:34:34 2012.
 */

#ifndef PPAPI_C_PRIVATE_PPB_NETWORK_LIST_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NETWORK_LIST_PRIVATE_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_net_address_private.h"

#define PPB_NETWORKLIST_PRIVATE_INTERFACE_0_1 "PPB_NetworkList_Private;0.1"
#define PPB_NETWORKLIST_PRIVATE_INTERFACE PPB_NETWORKLIST_PRIVATE_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_NetworkList_Private</code> interface.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * <code>PPB_NetworkListChanged_Callback</code> is a callback function
 * type that is used to receive notifications about changes in the
 * NetworkList.
 */
typedef void (*PPB_NetworkListChanged_Callback)(void* user_data);
/**
 * @}
 */

/**
 * @addtogroup Enums
 * @{
 */
/**
 * Type of a network interface.
 */
typedef enum {
  /**
   * Wired Ethernet network.
   */
  PP_NETWORKLIST_ETHERNET = 0,
  /**
   * Wireless Wi-Fi network.
   */
  PP_NETWORKLIST_WIFI = 1,
  /**
   * Cellular network (e.g. LTE).
   */
  PP_NETWORKLIST_CELLULAR = 2
} PP_NetworkListType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_NetworkListType, 4);

/**
 * State of a network interface.
 */
typedef enum {
  /**
   * Network interface is down.
   */
  PP_NETWORKLIST_DOWN = 0,
  /**
   * Network interface is up.
   */
  PP_NETWORKLIST_UP = 1
} PP_NetworkListState;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_NetworkListState, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_NetworkList_Private</code> provides list of network
 * interfaces and associated IP addressed.
 */
struct PPB_NetworkList_Private_0_1 {
  /**
   * Create() creates a new <code>NetworkList</code> object.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return A <code>PP_Resource</code> corresponding to a NetworkList if
   * successful, 0 if the instance is invalid.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Determines if the specified <code>resource</code> is a
   * <code>NetworkList</code> object.
   *
   * @param[in] resource A <code>PP_Resource</code> resource.
   *
   * @return Returns <code>PP_TRUE</code> if <code>resource</code> is
   * a <code>PPB_NetworkList_Private</code>, <code>PP_FALSE</code>
   * otherwise.
   */
  PP_Bool (*IsNetworkList)(PP_Resource resource);
  /**
   * Updates the list with the current state of the network interfaces
   * in the system.
   */
  int32_t (*Update)(struct PP_CompletionCallback callback);
  /**
   * Starts change notifications. The specified <code>callback</code>
   * will be called every time the network list changes. Every time
   * the callback is called the plugin must call Update() method to
   * actually receive updated list.
   */
  int32_t (*StartNotifications)(PP_Resource resource,
                                PPB_NetworkListChanged_Callback callback,
                                void* user_data);
  /**
   * Stops change notifications. After this method is called the
   * callback specified in <code>StartNotifications()</code> will not
   * be called anymore.
   */
  int32_t (*StopNotifications)(PP_Resource resource);
  /**
   * @return Returns number of available network interfaces or 0 if
   * the list has never been updated.
   */
  uint32_t (*GetCount)(PP_Resource resource);
  /**
   * @return Returns name for the network interface with the specified
   * <code>index</code>.
   */
  struct PP_Var (*GetName)(PP_Resource resource, uint32_t index);
  /**
   * @return Returns type of the network interface with the specified
   * <code>index</code>.
   */
  PP_NetworkListType (*GetType)(PP_Resource resource, uint32_t index);
  /**
   * @return Returns current state of the network interface with the
   * specified <code>index</code>.
   */
  PP_NetworkListState (*GetState)(PP_Resource resource, uint32_t index);
  /**
   * @return Returns <code>NetAddress</code> object that contains
   * address of the specified <code>family</code> for the network
   * interface with the specified <code>index</code>, or 0 if the
   * address is not assigned.
   */
  PP_Resource (*GetIpAddress)(PP_Resource resource,
                              uint32_t index,
                              PP_NetAddressFamily_Private family);
  /**
   * @return Returns display name for the network interface with the
   * specified <code>index</code>.
   */
  struct PP_Var (*GetDisplayName)(PP_Resource resource, uint32_t index);
  /**
   * @return Returns MTU for the network interface with the specified
   * <code>index</code>.
   */
  uint32_t (*GetMTU)(PP_Resource resource, uint32_t index);
};

typedef struct PPB_NetworkList_Private_0_1 PPB_NetworkList_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_NETWORK_LIST_PRIVATE_H_ */

