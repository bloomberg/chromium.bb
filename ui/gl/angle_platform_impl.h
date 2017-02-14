// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANGLE_PLATFORM_IMPL_H_
#define UI_GL_ANGLE_PLATFORM_IMPL_H_

// Implements the ANGLE platform interface, for functionality like
// histograms and trace profiling.

#include "ui/gl/gl_context_egl.h"

namespace gl {

bool InitializeANGLEPlatform(EGLDisplay display);
void ResetANGLEPlatform(EGLDisplay display);

}  // namespace gl

#endif  // UI_GL_ANGLE_PLATFORM_IMPL_H_
