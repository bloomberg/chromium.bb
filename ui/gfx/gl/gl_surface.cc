// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface.h"

#include "base/threading/thread_local.h"
#include "ui/gfx/gl/gl_context.h"

namespace gfx {

static base::ThreadLocalPointer<GLSurface> current_surface_;

GLSurface::GLSurface() {
}

GLSurface::~GLSurface() {
  if (GetCurrent() == this)
    SetCurrent(NULL);
}

bool GLSurface::Initialize()
{
  return true;
}

unsigned int GLSurface::GetBackingFrameBufferObject() {
  return 0;
}

void GLSurface::OnMakeCurrent(GLContext* context) {
}

GLSurface* GLSurface::GetCurrent() {
  return current_surface_.Get();
}

void GLSurface::SetCurrent(GLSurface* surface) {
  current_surface_.Set(surface);
}

}  // namespace gfx
