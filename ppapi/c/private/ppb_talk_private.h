/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_talk_private.idl modified Fri Nov  9 14:42:36 2012. */

#ifndef PPAPI_C_PRIVATE_PPB_TALK_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_TALK_PRIVATE_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_TALK_PRIVATE_INTERFACE_1_0 "PPB_Talk_Private;1.0"
#define PPB_TALK_PRIVATE_INTERFACE PPB_TALK_PRIVATE_INTERFACE_1_0

/**
 * @file
 * This file contains the <code>PPB_Talk</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Extra interface for Talk.
 */
struct PPB_Talk_Private_1_0 {
  /**
   * Creates a Talk_Private resource.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * Displays security UI.
   *
   * The callback will be issued with 1 as the result if the user gave
   * permission, or 0 if the user denied.
   *
   * You can only have one call pending. It will return PP_OK_COMPLETIONPENDING
   * if the request is queued, or PP_ERROR_INPROGRESS if there is already a
   * request in progress.
   */
  int32_t (*GetPermission)(PP_Resource talk_resource,
                           struct PP_CompletionCallback callback);
};

typedef struct PPB_Talk_Private_1_0 PPB_Talk_Private;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPB_TALK_PRIVATE_H_ */

