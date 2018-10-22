// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#include "ui/gl/gl_utils.h"

#include "base/logging.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

int GetGLColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space.IsHDR())
    return GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM;
  return GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM;
}

// Used by chrome://gpucrash and gpu_benchmarking_extension's
// CrashForTesting.
void Crash() {
  DVLOG(1) << "GPU: Simulating GPU crash";
  // Good bye, cruel world.
  volatile int* it_s_the_end_of_the_world_as_we_know_it = nullptr;
  *it_s_the_end_of_the_world_as_we_know_it = 0xdead;
}
}
