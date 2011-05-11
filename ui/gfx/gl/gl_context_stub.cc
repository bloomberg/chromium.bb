// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gl/gl_context_stub.h"

namespace gfx {

StubGLContext::~StubGLContext() {}

bool StubGLContext::MakeCurrent() {
  return true;
}

bool StubGLContext::IsCurrent() {
  return true;
}

bool StubGLContext::IsOffscreen() {
  return false;
}

bool StubGLContext::SwapBuffers() {
  return true;
}

gfx::Size StubGLContext::GetSize() {
  return size_;
}

void* StubGLContext::GetHandle() {
  return NULL;
}

std::string StubGLContext::GetExtensions() {
  return std::string();
}

}  // namespace gfx
