/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_CURSOR_CONTROL_DEV_H_
#define PPAPI_C_DEV_PPP_CURSOR_CONTROL_DEV_H_

#include "ppapi/c/pp_instance.h"

#define PPP_CURSOR_CONTROL_DEV_INTERFACE "PPP_CursorControl(Dev);0.2"

struct PPP_CursorControl_Dev {
  // Called when the instance looses the cursor lock, e.g. because the user
  // pressed the ESC key.
  void (*CursorLockLost)(PP_Instance instance);
};

#endif  /* PPAPI_C_DEV_PPP_CURSOR_CONTROL_DEV_H_ */

