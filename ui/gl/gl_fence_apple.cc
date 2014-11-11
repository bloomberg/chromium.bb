// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_apple.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gfx {

GLFenceAPPLE::GLFenceAPPLE(bool flush) {
  glGenFencesAPPLE(1, &fence_);
  glSetFenceAPPLE(fence_);
  DCHECK(glIsFenceAPPLE(fence_));
  if (flush) {
    glFlush();
  } else {
    flush_event_ = GLContext::GetCurrent()->SignalFlush();
  }
}

bool GLFenceAPPLE::HasCompleted() {
  DCHECK(glIsFenceAPPLE(fence_));
  return !!glTestFenceAPPLE(fence_);
}

void GLFenceAPPLE::ClientWait() {
  DCHECK(glIsFenceAPPLE(fence_));
  if (!flush_event_.get() || flush_event_->IsSignaled()) {
    glFinishFenceAPPLE(fence_);
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

void GLFenceAPPLE::ServerWait() {
  DCHECK(glIsFenceAPPLE(fence_));
  ClientWait();
}

GLFenceAPPLE::~GLFenceAPPLE() {
  DCHECK(glIsFenceAPPLE(fence_));
  glDeleteFencesAPPLE(1, &fence_);
}

}  // namespace gfx
