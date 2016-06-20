// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/egl_thread_context.h"

#include "base/logging.h"

namespace remoting {

EglThreadContext::EglThreadContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(eglGetCurrentDisplay() == EGL_NO_DISPLAY);
  CHECK(eglGetCurrentContext() == EGL_NO_CONTEXT);
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!display_ || !eglInitialize(display_, NULL, NULL)) {
    LOG(FATAL) << "Failed to initialize EGL display.";
  }

  const EGLint config_attribs[] = {
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };

  EGLint numConfigs;
  if (!eglChooseConfig(display_, config_attribs, &config_, 1, &numConfigs)) {
    LOG(FATAL) << "Failed to choose config.";
  }
  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT,
                              context_attribs);
  if (!context_) {
    LOG(FATAL) << "Failed to create context.";
  }
}

EglThreadContext::~EglThreadContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display_, context_);
  if (surface_) {
    eglDestroySurface(display_, surface_);
  }
  eglTerminate(display_);
}

void EglThreadContext::BindToWindow(EGLNativeWindowType window) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (surface_) {
    eglDestroySurface(display_, surface_);
    surface_ = EGL_NO_SURFACE;
  }
  if (window) {
    surface_ = eglCreateWindowSurface(display_, config_, window, NULL);
    if (!surface_) {
      LOG(FATAL) << "Failed to create window surface.";
    }
  } else {
    surface_ = EGL_NO_SURFACE;
  }
  if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
    LOG(FATAL) << "Failed to make current.";
  }
}

bool EglThreadContext::IsWindowBound() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return surface_ != EGL_NO_SURFACE;
}

void EglThreadContext::SwapBuffers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsWindowBound());
  if (!eglSwapBuffers(display_, surface_)) {
    LOG(FATAL) << "Failed to swap buffer.";
  }
}

}  // namespace remoting
