// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface.h"

namespace gfx {

GLSurface::GLSurface() {
}

GLSurface::~GLSurface() {
}

bool GLSurface::Initialize()
{
  return true;
}

unsigned int GLSurface::GetBackingFrameBufferObject() {
  return 0;
}

}  // namespace gfx
