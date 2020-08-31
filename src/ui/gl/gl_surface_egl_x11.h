// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_X11_H_
#define UI_GL_GL_SURFACE_EGL_X11_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

// Encapsulates an EGL surface bound to a view using the X Window System.
class GL_EXPORT NativeViewGLSurfaceEGLX11 : public NativeViewGLSurfaceEGL,
                                            public ui::XEventDispatcher {
 public:
  explicit NativeViewGLSurfaceEGLX11(EGLNativeWindowType window);
  NativeViewGLSurfaceEGLX11(const NativeViewGLSurfaceEGLX11& other) = delete;
  NativeViewGLSurfaceEGLX11& operator=(const NativeViewGLSurfaceEGLX11& rhs) =
      delete;

  // NativeViewGLSurfaceEGL overrides.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;

 protected:
  ~NativeViewGLSurfaceEGLX11() override;

  Display* GetXNativeDisplay() const;

 private:
  // NativeViewGLSurfaceEGL overrides:
  std::unique_ptr<gfx::VSyncProvider> CreateVsyncProviderInternal() override;

  // XEventDispatcher:
  bool DispatchXEvent(XEvent* xev) override;

  std::vector<x11::Window> children_;

  // Indicates if the dispatcher has been set.
  bool dispatcher_set_ = false;

  bool has_swapped_buffers_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_X11_H_
