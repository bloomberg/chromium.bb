/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPP_POLICY_UPDATE_DEV_H_
#define PPAPI_C_PPP_POLICY_UPDATE_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"

#define PPP_POLICY_UPDATE_DEV_INTERFACE_0_1 "PPB_PolicyUpdate;0.1"
#define PPP_POLICY_UPDATE_DEV_INTERFACE PPP_POLICY_UPDATE_DEV_INTERFACE_0_1

struct PPP_PolicyUpdate_Dev {
  /**
   * Signals that the policy has been updated, and provides it via a JSON
   * string.
   *
   * @param[in] instance A <code>PP_Instance</code> indentifying one instance
   * of a module.
   * @param[in] A <code>PP_Var</code> with a JSON string representing the
   * encoded policy.
   */
  void (*PolicyUpdated)(PP_Instance instance, struct PP_Var policy_json);
};

#endif  /* PPAPI_C_PPP_POLICY_UPDATE_DEV_H_ */
