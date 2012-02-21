/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_keyboard_input_event_dev.idl,
 *   modified Tue Feb 21 08:56:59 2012.
 */

#ifndef PPAPI_C_DEV_PPB_KEYBOARD_INPUT_EVENT_DEV_H_
#define PPAPI_C_DEV_PPB_KEYBOARD_INPUT_EVENT_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_KEYBOARD_INPUT_EVENT_DEV_INTERFACE_0_1 \
    "PPB_KeyboardInputEvent(Dev);0.1"
#define PPB_KEYBOARD_INPUT_EVENT_DEV_INTERFACE \
    PPB_KEYBOARD_INPUT_EVENT_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_KeyboardInputEvent_Dev</code> interface,
 * which provides access to USB key codes that identify the physical key being
 * pressed.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_KeyboardInputEvent_Dev</code> interface is an extension to the
 * PPB_KeyboardInputEvent</code> interface that provides
 */
struct PPB_KeyboardInputEvent_Dev_0_1 {
  /**
   * This sets a USB key code in the given <code>PP_Resource</code>. It is
   * intended that this method be called immediately after any call to
   * <code>Create</code>.
   *
   * @param[in] key_event A <code>PP_Resource</code> created by
   * <code>PPB_KeyboardInputEvent</code>'s <code>Create</code> method.
   *
   * @param[in] usb_key_code The USB key code to associate with this
   * <code>key_event</code>.
   *
   * @return <code>PP_TRUE</code> if the USB key code was set successfully.
   */
  PP_Bool (*SetUsbKeyCode)(PP_Resource key_event, uint32_t usb_key_code);
  /**
   * GetUsbKeyCode() returns the USB key code associated with this keyboard
   * event.
   *
   * @param[in] key_event The key event for which to return the key code.
   *
   * @return The USB key code field for the keyboard event. If there is no
   * USB scancode associated with this event, or if the PP_Resource does not
   * support the PPB_InputEvent_API (i.e., it is not an input event), then
   * a 0 is returned.
   */
  uint32_t (*GetUsbKeyCode)(PP_Resource key_event);
};

typedef struct PPB_KeyboardInputEvent_Dev_0_1 PPB_KeyboardInputEvent_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_KEYBOARD_INPUT_EVENT_DEV_H_ */

