// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence_egl.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace gfx {

GLFenceEGL::GLFenceEGL(bool flush) {
  display_ = eglGetCurrentDisplay();
  sync_ = eglCreateSyncKHR(display_, EGL_SYNC_FENCE_KHR, NULL);
  DCHECK(sync_ != EGL_NO_SYNC_KHR);
  if (flush) {
    glFlush();
  } else {
    flush_event_ = GLContext::GetCurrent()->SignalFlush();
  }
}

bool GLFenceEGL::HasCompleted() {
  EGLint value = 0;
  eglGetSyncAttribKHR(display_, sync_, EGL_SYNC_STATUS_KHR, &value);
  DCHECK(value == EGL_SIGNALED_KHR || value == EGL_UNSIGNALED_KHR);
  return !value || value == EGL_SIGNALED_KHR;
}

void GLFenceEGL::ClientWait() {
  if (!flush_event_ || flush_event_->IsSignaled()) {
    EGLint flags = 0;
    EGLTimeKHR time = EGL_FOREVER_KHR;
    eglClientWaitSyncKHR(display_, sync_, flags, time);
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

void GLFenceEGL::ServerWait() {
  if (!flush_event_ || flush_event_->IsSignaled()) {
    EGLint flags = 0;
    eglWaitSyncKHR(display_, sync_, flags);
  } else {
    LOG(ERROR) << "Trying to wait for uncommitted fence. Skipping...";
  }
}

GLFenceEGL::~GLFenceEGL() {
  eglDestroySyncKHR(display_, sync_);
}

}  // namespace gfx
