/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppb_flash_fullscreen.idl modified Fri Aug 26 10:51:16 2011. */

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_FULLSCREEN_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_FULLSCREEN_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FLASHFULLSCREEN_INTERFACE_0_1 "PPB_FlashFullscreen;0.1"
#define PPB_FLASHFULLSCREEN_INTERFACE PPB_FLASHFULLSCREEN_INTERFACE_0_1

/**
 * PPB_Fullscreen is a copy of PPB_Fullscreen_Dev at rev 0.4.
 * For backward compatibility keep the string for it until Flash is updated to
 * use PPB_FlashFullscreen.
 */
#define PPB_FULLSCREEN_DEV_INTERFACE_0_4 "PPB_Fullscreen(Dev);0.4"


/**
 * @file
 * This file defines the <code>PPB_FlashFullscreen</code> interface.
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_FlashFullscreen {
  /**
   * Checks whether the plugin instance is currently in fullscreen mode.
   */
  PP_Bool (*IsFullscreen)(PP_Instance instance);
  /**
   * Switches the plugin instance to/from fullscreen mode. Returns PP_TRUE on
   * success, PP_FALSE on failure.
   *
   * This unbinds the current Graphics2D or Surface3D. Pending flushes and
   * swapbuffers will execute as if the resource was off-screen. The transition
   * is asynchronous. During the transition, IsFullscreen will return PP_FALSE,
   * and no Graphics2D or Surface3D can be bound. The transition ends at the
   * next DidChangeView when going into fullscreen mode. The transition out of
   * fullscreen mode is synchronous.
   *
   * Note: when switching to and from fullscreen, Context3D and Surface3D
   * resources need to be re-created. This is a current limitation that will be
   * lifted in a later revision.
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

#endif  /* PPAPI_C_PRIVATE_PPB_FLASH_FULLSCREEN_H_ */
