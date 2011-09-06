/* Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PPAPI_C_PPP_GRAPHICS_3D_H_
#define PPAPI_C_PPP_GRAPHICS_3D_H_

#include "ppapi/c/pp_instance.h"

#define PPP_GRAPHICS_3D_INTERFACE "PPP_Graphics_3D;0.2"

struct PPP_Graphics3D {
  // Called when the OpenGL ES window is invalidated and needs to be repainted.
  void (*Graphics3DContextLost)(PP_Instance instance);
};

#endif  /* PPAPI_C_PPP_GRAPHICS_3D_H_ */

