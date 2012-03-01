/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_gamepad_dev.idl modified Mon Feb 27 13:23:13 2012. */

#ifndef PPAPI_C_DEV_PPB_GAMEPAD_DEV_H_
#define PPAPI_C_DEV_PPB_GAMEPAD_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_GAMEPAD_DEV_INTERFACE_0_2 "PPB_Gamepad(Dev);0.2"
#define PPB_GAMEPAD_DEV_INTERFACE PPB_GAMEPAD_DEV_INTERFACE_0_2

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
struct PP_GamepadSampleData_Dev {
  /**
   * Number of valid elements in the |axes| array.
   */
  uint32_t axes_length;
  /**
   * Normalized values for the axes, indices valid up to |axes_length|-1. Axis
   * values range from -1..1, and are in order of "importance".
   */
  float axes[16];
  /**
   * Number of valid elements in the |buttons| array.
   */
  uint32_t buttons_length;
  /**
   * Normalized values for the buttons, indices valid up to |buttons_length|
   * - 1. Button values range from 0..1, and are in order of importance.
   */
  float buttons[32];
  /**
   * Monotonically increasing value that is incremented when the data have
   * been updated.
   */
  double timestamp;
  /**
   * Identifier for the type of device/manufacturer.
   */
  uint16_t id[128];
  /**
   * Is there a gamepad connected at this index? If this is false, no other
   * data in this structure is valid.
   */
  PP_Bool connected;
  /* Padding to make the struct the same size between 64 and 32. */
  char unused_pad_[4];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_GamepadSampleData_Dev, 472);

/**
 * The data for all gamepads connected to the system.
 */
struct PP_GamepadsSampleData_Dev {
  /**
   * Number of valid elements in the |items| array.
   */
  uint32_t length;
  /* Padding to make the struct the same size between 64 and 32. */
  char unused_pad_[4];
  /**
   * Data for an individual gamepad device connected to the system.
   */
  struct PP_GamepadSampleData_Dev items[4];
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_GamepadsSampleData_Dev, 1896);
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
struct PPB_Gamepad_Dev_0_2 {
  /**
   * Samples the current state of the connected gamepads.
   */
  void (*Sample)(PP_Instance instance, struct PP_GamepadsSampleData_Dev* data);
};

typedef struct PPB_Gamepad_Dev_0_2 PPB_Gamepad_Dev;
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_GAMEPAD_DEV_H_ */

