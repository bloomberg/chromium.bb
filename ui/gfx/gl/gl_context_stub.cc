// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_stub.h"

namespace gfx {

GLContextStub::GLContextStub() : GLContext(NULL) {
}

GLContextStub::~GLContextStub() {
}

bool GLContextStub::Initialize(
    GLSurface* compatible_surface, GpuPreference gpu_preference) {
  return true;
}

void GLContextStub::Destroy() {
}

bool GLContextStub::MakeCurrent(GLSurface* surface) {
  return true;
}

void GLContextStub::ReleaseCurrent(GLSurface* surface) {
}

bool GLContextStub::IsCurrent(GLSurface* surface) {
  return true;
}

void* GLContextStub::GetHandle() {
  return NULL;
}

void GLContextStub::SetSwapInterval(int interval) {
}

std::string GLContextStub::GetExtensions() {
  return std::string();
}

}  // namespace gfx
