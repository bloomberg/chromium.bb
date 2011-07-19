// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_surface_stub.h"

namespace gfx {

GLSurfaceStub::~GLSurfaceStub() {
}

void GLSurfaceStub::Destroy() {
}

bool GLSurfaceStub::IsOffscreen() {
  return false;
}

bool GLSurfaceStub::SwapBuffers() {
  return true;
}

gfx::Size GLSurfaceStub::GetSize() {
  return size_;
}

void* GLSurfaceStub::GetHandle() {
  return NULL;
}

}  // namespace gfx
