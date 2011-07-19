/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_SELECTION_DEV_H_
#define PPAPI_C_DEV_PPP_SELECTION_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"

#define PPP_SELECTION_DEV_INTERFACE "PPP_Selection(Dev);0.3"

struct PPP_Selection_Dev {
  /**
   * Returns the selection, either as plain text or as html depending on "html".
   * If nothing is selected, or if the given format is unavailable, return a
   * void string.
   */
  struct PP_Var (*GetSelectedText)(PP_Instance instance,
                                   PP_Bool html);
};

#endif  /* PPAPI_C_DEV_PPP_SELECTION_DEV_H_ */

