/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_
#define PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FULLSCREEN_DEV_INTERFACE "PPB_Fullscreen(Dev);0.4"

// Use this interface to change a plugin instance to fullscreen mode.
struct PPB_Fullscreen_Dev {
  // Checks whether the plugin instance is currently in fullscreen mode.
  PP_Bool (*IsFullscreen)(PP_Instance instance);

  // Switches the plugin instance to/from fullscreen mode. Returns PP_TRUE on
  // success, PP_FALSE on failure.
  // This unbinds the current Graphics2D or Surface3D. Pending flushes and
  // swapbuffers will execute as if the resource was off-screen.  The transition
  // is asynchronous. During the transition, IsFullscreen will return PP_False,
  // and no Graphics2D or Surface3D can be bound. The transition ends at the
  // next DidChangeView.
  // Note: when switching to and from fullscreen, Context3D and Surface3D
  // resources need to be re-created. This is a current limitation that will be
  // lifted in a later revision.
  PP_Bool (*SetFullscreen)(PP_Instance instance, PP_Bool fullscreen);

  // Gets the size of the screen. When going fullscreen, the instance will be
  // resized to that size.
  PP_Bool (*GetScreenSize)(PP_Instance instance, struct PP_Size* size);
};

#endif  /* PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_ */

