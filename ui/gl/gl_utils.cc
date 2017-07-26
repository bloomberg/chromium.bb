// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some useful utilities for the ui/gl classes.

#include "ui/gl/gl_utils.h"

#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches.h"

namespace gl {

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  return command_line->HasSwitch(switches::kUsePassthroughCmdDecoder);
}

int GetGLColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space == gfx::ColorSpace::CreateSCRGBLinear())
    return GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM;
  return GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM;
}
}
