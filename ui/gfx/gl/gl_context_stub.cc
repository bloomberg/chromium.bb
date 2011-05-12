// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_stub.h"

namespace gfx {

GLContextStub::GLContextStub() {
}

GLContextStub::~GLContextStub() {
}

bool GLContextStub::MakeCurrent() {
  return true;
}

bool GLContextStub::IsCurrent() {
  return true;
}

bool GLContextStub::IsOffscreen() {
  return false;
}

bool GLContextStub::SwapBuffers() {
  return true;
}

gfx::Size GLContextStub::GetSize() {
  return size_;
}

void* GLContextStub::GetHandle() {
  return NULL;
}

std::string GLContextStub::GetExtensions() {
  return std::string();
}

}  // namespace gfx
