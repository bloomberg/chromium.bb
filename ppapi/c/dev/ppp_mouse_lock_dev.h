/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppp_mouse_lock_dev.idl modified Tue Sep  6 16:07:16 2011. */

#ifndef PPAPI_C_DEV_PPP_MOUSE_LOCK_DEV_H_
#define PPAPI_C_DEV_PPP_MOUSE_LOCK_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_MOUSELOCK_DEV_INTERFACE_0_1 "PPP_MouseLock(Dev);0.1"
#define PPP_MOUSELOCK_DEV_INTERFACE PPP_MOUSELOCK_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPP_MouseLock_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPP_MouseLock_Dev</code> interface contains pointers to functions
 * that you must implement to receive mouse lock events from the browser.
 */
struct PPP_MouseLock_Dev {
  /**
   * Called when the instance loses the mouse lock, e.g. because the user
   * pressed the ESC key.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   */
  void (*MouseLockLost)(PP_Instance instance);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPP_MOUSE_LOCK_DEV_H_ */

