// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_GLX_H_
#define UI_GFX_GL_GL_SURFACE_GLX_H_

#include "ui/gfx/gl/gl_surface.h"

#include "ui/base/x/x11_util.h"
#include "ui/gfx/gl/gl_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {

// Base class for GLX surfaces.
class GL_EXPORT GLSurfaceGLX : public GLSurface {
 public:
  GLSurfaceGLX();
  virtual ~GLSurfaceGLX();

  static bool InitializeOneOff();

  // These aren't particularly tied to surfaces, but since we already
  // have the static InitializeOneOff here, it's easiest to reuse its
  // initialization guards.
  static const char* GetGLXExtensions();
  static bool HasGLXExtension(const char* name);
  static bool IsCreateContextRobustnessSupported();

  virtual void* GetDisplay();

  // Get the FB config that the surface was created with or NULL if it is not
  // a GLX drawable.
  virtual void* GetConfig() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceGLX);
};

// A surface used to render to a view.
class GL_EXPORT NativeViewGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit NativeViewGLSurfaceGLX(gfx::PluginWindowHandle window);
  virtual ~NativeViewGLSurfaceGLX();

  // Implement GLSurfaceGLX.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual std::string GetExtensions();
  virtual void* GetConfig();
  virtual bool PostSubBuffer(int x, int y, int width, int height);

 protected:
  NativeViewGLSurfaceGLX();

  gfx::PluginWindowHandle window_;

 private:
  void* config_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceGLX);
};

// A surface used to render to an offscreen pbuffer.
class GL_EXPORT PbufferGLSurfaceGLX : public GLSurfaceGLX {
 public:
  explicit PbufferGLSurfaceGLX(const gfx::Size& size);
  virtual ~PbufferGLSurfaceGLX();

  // Implement GLSurfaceGLX.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void* GetConfig();

 private:
  gfx::Size size_;
  void* config_;
  XID pbuffer_;
  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceGLX);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_GLX_H_
