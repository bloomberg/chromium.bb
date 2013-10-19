/* Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_var_resource_dev.idl modified Fri Oct 11 10:31:47 2013. */

#ifndef PPAPI_C_DEV_PPB_VAR_RESOURCE_DEV_H_
#define PPAPI_C_DEV_PPB_VAR_RESOURCE_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_VAR_RESOURCE_DEV_INTERFACE_0_1 "PPB_VarResource(Dev);0.1"
#define PPB_VAR_RESOURCE_DEV_INTERFACE PPB_VAR_RESOURCE_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_VarResource</code> struct providing
 * a way to interact with resource vars.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_VarResource_Dev_0_1 {
  /**
   * Converts a resource-type var to a <code>PP_Resource</code>.
   *
   * @param[in] var A <code>PP_Var</code> struct containing a resource-type var.
   *
   * @return A <code>PP_Resource</code> retrieved from the var, or 0 if the var
   * is not a resource. The reference count of the resource is incremented on
   * behalf of the caller.
   */
  PP_Resource (*VarToResource)(struct PP_Var var);
  /**
   * Creates a new <code>PP_Var</code> from a given resource.
   *
   * @param[in] resource A <code>PP_Resource</code> to be wrapped in a var.
   *
   * @return A <code>PP_Var</code> created for this resource, with type
   * <code>PP_VARTYPE_RESOURCE</code>. The reference count of the var is set to
   * 1 on behalf of the caller.
   */
  struct PP_Var (*VarFromResource)(PP_Resource resource);
};

typedef struct PPB_VarResource_Dev_0_1 PPB_VarResource_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_VAR_RESOURCE_DEV_H_ */

