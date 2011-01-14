/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_DEV_PPP_GRAPHICS_3D_DEV_H_
#define PPAPI_C_DEV_PPP_GRAPHICS_3D_DEV_H_

#include "ppapi/c/pp_instance.h"

#define PPP_GRAPHICS_3D_DEV_INTERFACE "PPP_Graphics_3D(Dev);0.2"

struct PPP_Graphics3D_Dev {
  // Called when the OpenGL ES window is invalidated and needs to be repainted.
  void (*Graphics3DContextLost)(PP_Instance instance);
};

#endif  /* PPAPI_C_DEV_PPP_GRAPHICS_3D_DEV_H_ */

