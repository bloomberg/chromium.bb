// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_EGL_THREAD_CONTEXT_H_
#define REMOTING_CLIENT_JNI_EGL_THREAD_CONTEXT_H_

#include <EGL/egl.h>

#include "base/macros.h"
#include "base/threading/thread_checker.h"

namespace remoting {

// Establishes an EGL-OpenGL|ES2 context on current thread. Must be constructed,
// used, and deleted on single thread (i.e. the display thread). Each thread can
// have no more than one EglThreadContext.
//
// An example use case:
// class DisplayHandler {
//   void OnSurfaceCreated(ANativeWindow* surface) {
//     context_.BindToWindow(surface);
//   }
//   void OnSurfaceDestroyed() {
//     context_.BindToWindow(nullptr);
//   }
//   EglThreadContext context_;
// };
class EglThreadContext {
 public:
  EglThreadContext();
  ~EglThreadContext();

  // Creates a surface on the given window and binds the context to the surface.
  // Unbinds |window| last bound if |window| is NULL.
  // EGLNativeWindowType is platform specific. E.g. ANativeWindow* on Android.
  void BindToWindow(EGLNativeWindowType window);

  // Returns true IFF the context is bound to a window (i.e. current surface is
  // not NULL).
  bool IsWindowBound() const;

  // Posts EGL surface buffer to the window being bound. Window must be bound
  // before calling this function.
  void SwapBuffers();

 private:
  EGLDisplay display_ = EGL_NO_DISPLAY;
  EGLConfig config_ = nullptr;
  EGLSurface surface_ = EGL_NO_SURFACE;
  EGLContext context_ = EGL_NO_CONTEXT;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(EglThreadContext);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_EGL_THREAD_CONTEXT_H_
