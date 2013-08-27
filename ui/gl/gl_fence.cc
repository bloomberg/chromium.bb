// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_fence.h"

#include "base/compiler_specific.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"

namespace {

class GLFenceNVFence: public gfx::GLFence {
 public:
  GLFenceNVFence() {
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
    glFlush();
  }

  virtual bool HasCompleted() OVERRIDE {
    return !!glTestFenceNV(fence_);
  }

  virtual void ClientWait() OVERRIDE {
    glFinishFenceNV(fence_);
  }

 private:
  virtual ~GLFenceNVFence() {
    glDeleteFencesNV(1, &fence_);
  }

  GLuint fence_;
};

class GLFenceARBSync: public gfx::GLFence {
 public:
  GLFenceARBSync() {
    sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
  }

  virtual bool HasCompleted() OVERRIDE {
    // Handle the case where FenceSync failed.
    if (!sync_)
      return true;

    // We could potentially use glGetSynciv here, but it doesn't work
    // on OSX 10.7 (always says the fence is not signaled yet).
    // glClientWaitSync works better, so let's use that instead.
    return  glClientWaitSync(sync_, 0, 0) != GL_TIMEOUT_EXPIRED;
  }

  virtual void ClientWait() OVERRIDE {
    glClientWaitSync(sync_, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
  }

 private:
  virtual ~GLFenceARBSync() {
    glDeleteSync(sync_);
  }

  GLsync sync_;
};

#if !defined(OS_MACOSX)
class EGLFenceSync : public gfx::GLFence {
 public:
  EGLFenceSync() {
    display_ = eglGetCurrentDisplay();
    sync_ = eglCreateSyncKHR(display_, EGL_SYNC_FENCE_KHR, NULL);
    glFlush();
  }

  virtual bool HasCompleted() OVERRIDE {
    EGLint value = 0;
    eglGetSyncAttribKHR(display_, sync_, EGL_SYNC_STATUS_KHR, &value);
    DCHECK(value == EGL_SIGNALED_KHR || value == EGL_UNSIGNALED_KHR);
    return !value || value == EGL_SIGNALED_KHR;
  }

  virtual void ClientWait() OVERRIDE {
    EGLint flags = 0;
    EGLTimeKHR time = EGL_FOREVER_KHR;
    eglClientWaitSyncKHR(display_, sync_, flags, time);
  }

 private:
  virtual ~EGLFenceSync() {
    eglDestroySyncKHR(display_, sync_);
  }

  EGLSyncKHR sync_;
  EGLDisplay display_;
};
#endif // !OS_MACOSX

}  // namespace

namespace gfx {

GLFence::GLFence() {
}

GLFence::~GLFence() {
}

// static
GLFence* GLFence::Create() {
#if !defined(OS_MACOSX)
  if (gfx::g_driver_egl.ext.b_EGL_KHR_fence_sync)
    return new EGLFenceSync();
#endif
  if (gfx::g_driver_gl.ext.b_GL_NV_fence)
    return new GLFenceNVFence();
  if (gfx::g_driver_gl.ext.b_GL_ARB_sync)
    return new GLFenceARBSync();
  return NULL;
}

}  // namespace gfx
