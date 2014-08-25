// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_nv.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gfx {

GLFenceNV::GLFenceNV(bool flush) {
  // What if either of these GL calls fails? TestFenceNV will return true.
  // See spec:
  // http://www.opengl.org/registry/specs/NV/fence.txt
  //
  // What should happen if TestFenceNV is called for a name before SetFenceNV
  // is called?
  //     We generate an INVALID_OPERATION error, and return TRUE.
  //     This follows the semantics for texture object names before
  //     they are bound, in that they acquire their state upon binding.
  //     We will arbitrarily return TRUE for consistency.
  glGenFencesNV(1, &fence_);
  glSetFenceNV(fence_, GL_ALL_COMPLETED_NV);
  DCHECK(glIsFenceNV(fence_));
  if (flush) {
    glFlush();
  } else {
    flush_event_ = GLContext::GetCurrent()->SignalFlush();
  }
}

bool GLFenceNV::HasCompleted() {
  DCHECK(glIsFenceNV(fence_));
  return !!glTestFenceNV(fence_);
}

void GLFenceNV::ClientWait() {
  DCHECK(glIsFenceNV(fence_));
  if (!flush_event_.get() || flush_event_->IsSignaled()) {
    glFinishFenceNV(fence_);
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

void GLFenceNV::ServerWait() {
  DCHECK(glIsFenceNV(fence_));
  ClientWait();
}

GLFenceNV::~GLFenceNV() {
  DCHECK(glIsFenceNV(fence_));
  glDeleteFencesNV(1, &fence_);
}

}  // namespace gfx
