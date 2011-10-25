// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_WGL_H_
#define UI_GFX_GL_GL_SURFACE_WGL_H_

#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

// Base interface for WGL surfaces.
class GLSurfaceWGL : public GLSurface {
 public:
  GLSurfaceWGL();
  virtual ~GLSurfaceWGL();

  static bool InitializeOneOff();
  static HDC GetDisplay();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceWGL);
};

// A surface used to render to a view.
class NativeViewGLSurfaceWGL : public GLSurfaceWGL {
 public:
  explicit NativeViewGLSurfaceWGL(gfx::PluginWindowHandle window);
  virtual ~NativeViewGLSurfaceWGL();

  // Implement GLSurface.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

 private:
  gfx::PluginWindowHandle window_;
  gfx::PluginWindowHandle child_window_;
  HDC device_context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceWGL);
};


// A surface used to render to an offscreen pbuffer.
class PbufferGLSurfaceWGL : public GLSurfaceWGL {
 public:
  explicit PbufferGLSurfaceWGL(const gfx::Size& size);
  virtual ~PbufferGLSurfaceWGL();

  // Implement GLSurface.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

 private:
  gfx::Size size_;
  HDC device_context_;
  void* pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceWGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_WGL_H_
