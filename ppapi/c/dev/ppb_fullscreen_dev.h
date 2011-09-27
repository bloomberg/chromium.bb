/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_fullscreen_dev.idl modified Mon Sep 26 15:06:40 2011. */

#ifndef PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_
#define PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FULLSCREEN_DEV_INTERFACE_0_5 "PPB_Fullscreen(Dev);0.5"
#define PPB_FULLSCREEN_DEV_INTERFACE PPB_FULLSCREEN_DEV_INTERFACE_0_5

/**
 * @file
 * This file defines the <code>PPB_Fullscreen_Dev</code> interface.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Fullscreen_Dev {
  /**
   * Checks whether the plugin instance is currently in fullscreen mode.
   */
  PP_Bool (*IsFullscreen)(PP_Instance instance);
  /**
   * Switches the plugin instance to/from fullscreen mode. Returns PP_TRUE on
   * success, PP_FALSE on failure.
   *
   * This unbinds the current 2D or 3D devices. Pending flushes and swapbuffers
   * will execute as if the resource was off-screen. The transition to and from
   * fullscreen is asynchronous. During the transition, IsFullscreen will
   * return the original value, and no 2D or 3D device can be bound.
   * The transition ends at the next DidChangeView.
   *
   * The transition to fullscreen can only occur while the browser is
   * processing a user gesture, even if PP_TRUE is returned. Note that two
   * DidChangeView calls will happen when switching to fullscreen:
   * one for moving the plugin to the middle of the window and one for
   * stretching the window placing the plugin in the middle of the screen.
   * Plugin size will not be affected.
   */
  PP_Bool (*SetFullscreen)(PP_Instance instance, PP_Bool fullscreen);
  /**
   * Gets the size of the screen in pixels. When going fullscreen, the instance
   * will be resized to that size.
   */
  PP_Bool (*GetScreenSize)(PP_Instance instance, struct PP_Size* size);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_ */

