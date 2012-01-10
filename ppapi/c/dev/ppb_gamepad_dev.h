/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_gamepad_dev.idl modified Mon Jan  9 13:16:43 2012. */

#ifndef PPAPI_C_DEV_PPB_GAMEPAD_DEV_H_
#define PPAPI_C_DEV_PPB_GAMEPAD_DEV_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_GAMEPAD_DEV_INTERFACE_0_1 "PPB_Gamepad(Dev);0.1"
#define PPB_GAMEPAD_DEV_INTERFACE PPB_GAMEPAD_DEV_INTERFACE_0_1

/**
 * @file
 * This file defines the <code>PPB_Gamepad_Dev</code> interface, which
 * provides access to gamepad devices.
 */


/**
 * @addtogroup Structs
 * @{
 */
/**
 * The data for one gamepad device.
 */
struct PP_GamepadData_Dev {
  /**
   * Is there a gamepad connected at this index? If this is false, no other
   * data in this structure is valid.
   */
  char connected;
  /**
   * String identifier for the type of device/manufacturer.
   */
  uint16_t id[128];
  /**
   * Monotonically increasing value that is incremented when the data have
   * been updated.
   */
  uint64_t timestamp;
  /**
   * Number of valid elements in the |axes| array.
   */
  uint32_t axes_length;
  /**
   * Normalized values for the axes, indices valid up to |axesLength|-1. Axis
   * values range from -1..1, and are in order of "importance".
   */
  float axes[16];
  /**
   * Number of valid elements in the |buttons| array.
   */
  uint32_t buttons_length;
  /**
   * Normalized values for the buttons, indices valid up to |buttonsLength|
   * - 1. Button values range from 0..1, and are in order of importance.
   */
  float buttons[32];
};

/**
 * The data for all gamepads connected to the system.
 */
struct PP_GamepadsData_Dev {
  /**
   * Number of valid elements in the |items| array.
   */
  uint32_t length;
  /**
   * Data for an individual gamepad device connected to the system.
   */
  struct PP_GamepadData_Dev items[4];
};
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_Gamepad_Dev</code> interface allows retrieving data from
 * gamepad/joystick devices that are connected to the system.
 */
struct PPB_Gamepad_Dev_0_1 {
  /**
   * Samples the current state of the connected gamepads.
   */
  void (*SampleGamepads)(PP_Instance instance,
                         struct PP_GamepadsData_Dev* data);
};

typedef struct PPB_Gamepad_Dev_0_1 PPB_Gamepad_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_GAMEPAD_DEV_H_ */

