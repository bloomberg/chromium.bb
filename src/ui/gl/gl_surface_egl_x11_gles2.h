// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_X11_GLES2_H_
#define UI_GL_GL_SURFACE_EGL_X11_GLES2_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl_x11.h"

namespace gl {

// Encapsulates an EGL surface bound to a view using the X Window System.
class GL_EXPORT NativeViewGLSurfaceEGLX11GLES2
    : public NativeViewGLSurfaceEGLX11 {
 public:
  explicit NativeViewGLSurfaceEGLX11GLES2(EGLNativeWindowType window);

  // NativeViewGLSurfaceEGL overrides.
  EGLConfig GetConfig() override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool InitializeNativeWindow() override;

 protected:
  ~NativeViewGLSurfaceEGLX11GLES2() override;

 private:
  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

  EGLNativeWindowType parent_window_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGLX11GLES2);
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_X11_GLES2_H_
