// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_CGL_H_
#define UI_GFX_GL_GL_SURFACE_CGL_H_

#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/size.h"

namespace gfx {

// Base class for CGL surfaces.
class GLSurfaceCGL : public GLSurface {
 public:
  GLSurfaceCGL();
  virtual ~GLSurfaceCGL();

  static bool InitializeOneOff();
  static void* GetPixelFormat();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceCGL);
};

// A "no-op" surface. It is not required that a CGLContextObj have an
// associated drawable (pbuffer or fullscreen context) in order to be
// made current. Everywhere this surface type is used, we allocate an
// FBO at the user level as the drawable of the associated context.
class NoOpGLSurfaceCGL : public GLSurfaceCGL {
 public:
  explicit NoOpGLSurfaceCGL(const gfx::Size& size);
  virtual ~NoOpGLSurfaceCGL();

  // Implement GLSurface.
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void* GetHandle() OVERRIDE;

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(NoOpGLSurfaceCGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_CGL_H_
