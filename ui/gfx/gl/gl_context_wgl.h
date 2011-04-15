// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_WGL_H_
#define UI_GFX_GL_GL_CONTEXT_WGL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface_wgl.h"
#include "ui/gfx/size.h"

namespace gfx {

// This class is a wrapper around a GL context.
class GLContextWGL : public GLContext {
 public:
  explicit GLContextWGL(GLSurfaceWGL* surface);
  virtual ~GLContextWGL();

  // Initializes the GL context.
  bool Initialize(GLContext* shared_context);

  // Implement GLContext.
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);
  virtual std::string GetExtensions();

 private:
  scoped_ptr<GLSurfaceWGL> surface_;
  HGLRC context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextWGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_WGL_H_
