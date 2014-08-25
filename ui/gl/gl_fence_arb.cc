// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_arb.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gfx {

GLFenceARB::GLFenceARB(bool flush) {
  sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  if (flush) {
    glFlush();
  } else {
    flush_event_ = GLContext::GetCurrent()->SignalFlush();
  }
}

bool GLFenceARB::HasCompleted() {
  // Handle the case where FenceSync failed.
  if (!sync_)
    return true;

  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  // We could potentially use glGetSynciv here, but it doesn't work
  // on OSX 10.7 (always says the fence is not signaled yet).
  // glClientWaitSync works better, so let's use that instead.
  return glClientWaitSync(sync_, 0, 0) != GL_TIMEOUT_EXPIRED;
}

void GLFenceARB::ClientWait() {
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  if (!flush_event_.get() || flush_event_->IsSignaled()) {
    glClientWaitSync(sync_, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

void GLFenceARB::ServerWait() {
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  if (!flush_event_.get() || flush_event_->IsSignaled()) {
    glWaitSync(sync_, 0, GL_TIMEOUT_IGNORED);
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

GLFenceARB::~GLFenceARB() {
  DCHECK_EQ(GL_TRUE, glIsSync(sync_));
  glDeleteSync(sync_);
}

}  // namespace gfx
