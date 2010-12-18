// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_
#define PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_FULLSCREEN_DEV_INTERFACE "PPB_Fullscreen(Dev);0.2"

// Use this interface to change a plugin instance to fullscreen mode.
struct PPB_Fullscreen_Dev {
  // Checks whether the plugin instance is currently in fullscreen mode.
  PP_Bool (*IsFullscreen)(PP_Instance instance);

  // Switches the plugin instance to/from fullscreen mode. Returns PP_TRUE on
  // success, PP_FALSE on failure.
  // When in fullscreen mode, the plugin will be transparently scaled to the
  // size of the screen. It will not receive a ViewChanged event, and doesn't
  // need to rebind the graphics context. The pending flushes will execute
  // normally, to the new fullscreen window.
  PP_Bool (*SetFullscreen)(PP_Instance instance, PP_Bool fullscreen);
};

#endif  // PPAPI_C_DEV_PPB_FULLSCREEN_DEV_H_
