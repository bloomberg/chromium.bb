/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_BROKER_TRUSTED_H_
#define PPAPI_C_PPB_BROKER_TRUSTED_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

#define PPB_BROKER_TRUSTED_INTERFACE "PPB_BrokerTrusted;0.1"

/**
 * @file
 * This file defines the PPB_BrokerTrusted interface, which provides
 * access to a trusted broker with greater privileges than the plugin.
 */

/**
 * @addtogroup Interfaces
 * @{
 */

/**
 * The PPB_BrokerTrusted interface provides access to a trusted broker
 * with greater privileges than the plugin. The interface only supports
 * out-of-process plugins and is to be used by proxy implementations.  All
 * functions should be called from the main thread only.
 */
struct PPB_BrokerTrusted {
  /**
   * Returns a trusted broker resource.
   */
  PP_Resource (*CreateTrusted)(PP_Instance instance);

  /**
   * Returns true if the resource is a trusted broker.
   */
  PP_Bool (*IsBrokerTrusted)(PP_Resource resource);

  /**
   * Connects to the trusted broker. It may have already
   * been launched by another plugin instance.
   * Returns PP_ERROR_WOULD_BLOCK on success, and invokes
   * the |connect_callback| asynchronously to complete.
   * As this function should always be invoked from the main thread,
   * do not use the blocking variant of PP_CompletionCallback.
   * Returns PP_ERROR_FAILED if called from an in-process plugin.
   */
  int32_t (*Connect)(PP_Resource broker,
                     struct PP_CompletionCallback connect_callback);

  /**
   * Returns the handle to the pipe. Use once Connect has completed.
   * Returns PP_OK on success.
   * Each plugin instance has its own pipe.
   */
  int32_t (*GetHandle)(PP_Resource broker, int32_t* handle);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_BROKER_TRUSTED_H_ */
