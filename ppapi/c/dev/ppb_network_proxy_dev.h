/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_network_proxy_dev.idl modified Thu Jun  6 10:44:17 2013. */

#ifndef PPAPI_C_DEV_PPB_NETWORK_PROXY_DEV_H_
#define PPAPI_C_DEV_PPB_NETWORK_PROXY_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_NETWORKPROXY_DEV_INTERFACE_0_1 "PPB_NetworkProxy(Dev);0.1"
#define PPB_NETWORKPROXY_DEV_INTERFACE PPB_NETWORKPROXY_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_NetworkProxy_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_NetworkProxy_Dev_0_1 {
  /**
   * Retrieves the proxy that will be used for the given URL. The result will
   * be a string in PAC format. For more details about PAC format, please see
   * http://en.wikipedia.org/wiki/Proxy_auto-config
   */
  int32_t (*GetProxyForURL)(PP_Instance instance,
                            struct PP_Var url,
                            struct PP_Var* proxy_string,
                            struct PP_CompletionCallback callback);
};

typedef struct PPB_NetworkProxy_Dev_0_1 PPB_NetworkProxy_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_NETWORK_PROXY_DEV_H_ */

