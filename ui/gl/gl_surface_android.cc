// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "third_party/khronos/EGL/egl.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return EGL_DEFAULT_DISPLAY;
}

}  // namespace gl
