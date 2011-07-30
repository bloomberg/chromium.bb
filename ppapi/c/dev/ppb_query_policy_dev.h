/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPB_QUERY_POLICY_DEV_H_
#define PPAPI_C_PPB_QUERY_POLICY_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"

#define PPB_QUERY_POLICY_DEV_INTERFACE_0_1 "PPB_QueryPolicy;0.1"
#define PPB_QUERY_POLICY_DEV_INTERFACE PPB_QUERY_POLICY_DEV_INTERFACE_0_1

struct PPB_QueryPolicy_Dev {
  /**
   * Subscribes the instance to receive updates via the
   * <code>PPP_PolicyUpdate_Dev</code> interface.
   *
   * The plugin is guaranteed to get one update immediately via the
   * <code>PP_PolicyUpdate_Dev</code> interface. This allows the plugin to
   * retrieve the current policy when subscribing for the first time.
   *
   * @param[in] instance A <code>PP_Instance</code> indentifying one instance
   * of a module.
   */
  void (*SubscribeToPolicyUpdates)(PP_Instance instance);

};

#endif  /* PPAPI_C_PPB_QUERY_POLICY_DEV_H_ */
