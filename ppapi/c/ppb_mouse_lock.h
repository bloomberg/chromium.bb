/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_mouse_lock.idl modified Fri Oct 14 18:59:25 2011. */

#ifndef PPAPI_C_PPB_MOUSE_LOCK_H_
#define PPAPI_C_PPB_MOUSE_LOCK_H_

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_MOUSELOCK_INTERFACE_1_0 "PPB_MouseLock;1.0"
#define PPB_MOUSELOCK_INTERFACE PPB_MOUSELOCK_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_MouseLock</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_MouseLock</code> interface is implemented by the browser.
 * It provides a way of locking the target of mouse events to a single plugin
 * instance and removing the cursor from view. This is a useful input mode for
 * certain classes of applications, especially first person perspective 3D
 * applications and 3D modelling software.
 */
struct PPB_MouseLock {
  /**
   * Requests the mouse to be locked. The browser will permit mouse lock only
   * while the tab is in fullscreen mode.
   *
   * While the mouse is locked, the cursor is implicitly hidden from the user.
   * Any movement of the mouse will generate a
   * <code>PP_INPUTEVENT_TYPE_MOUSEMOVE</code>. The <code>GetPosition</code> of
   * <code>PPB_MouseInputEvent</code> reports the last known mouse position just
   * as mouse lock was entered; while the <code>GetMovement</code> provides
   * relative movement information, which indicates what the change in position
   * of the mouse would be had it not been locked.
   *
   * The browser may revoke mouse lock for reasons including but not limited to
   * the user pressing the ESC key, the user activating another program via a
   * reserved keystroke (e.g., ALT+TAB), or some other system event.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*LockMouse)(PP_Instance instance,
                       struct PP_CompletionCallback callback);
  /**
   * Causes the mouse to be unlocked, allowing it to track user movement again.
   * This is an asynchronous operation. The plugin instance will be notified via
   * the <code>PPP_MouseLock</code> interface when it has lost the mouse lock.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   */
  void (*UnlockMouse)(PP_Instance instance);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_MOUSE_LOCK_H_ */

