/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_policy_update_dev.idl modified Tue Oct  4 09:58:59 2011. */

#ifndef PPAPI_C_DEV_PPP_POLICY_UPDATE_DEV_H_
#define PPAPI_C_DEV_PPP_POLICY_UPDATE_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPP_POLICYUPDATE_DEV_INTERFACE_0_1 "PPP_PolicyUpdate(Dev);0.1"
#define PPP_POLICYUPDATE_DEV_INTERFACE PPP_POLICYUPDATE_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPP_PolicyUpdate_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
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
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_POLICY_UPDATE_DEV_H_ */

