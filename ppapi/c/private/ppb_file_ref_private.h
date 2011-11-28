/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_C_PRIVATE_PPB_FILE_REF_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_FILE_REF_PRIVATE_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

#define PPB_FILEREFPRIVATE_INTERFACE_0_1 "PPB_FileRefPrivate;0.1"
#define PPB_FILEREFPRIVATE_INTERFACE PPB_FILEREFPRIVATE_INTERFACE_0_1

/**
 * @file
 * This file contains the <code>PPB_FileRefPrivate</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/* PPB_FileRefPrivate interface */
struct PPB_FileRefPrivate {
  /**
   * GetAbsolutePath() returns the absolute path of the file.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Var</code> containing the absolute path of the file.
   */
  struct PP_Var (*GetAbsolutePath)(PP_Resource file_ref);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_FILE_REF_PRIVATE_H_ */
