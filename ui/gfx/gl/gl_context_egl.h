// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_EGL_H_
#define UI_GFX_GL_GL_CONTEXT_EGL_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/size.h"

typedef void* EGLContext;

namespace gfx {

class GLSurfaceEGL;

// Encapsulates an EGL OpenGL ES context that renders to a view.
class GLContextEGL : public GLContext {
 public:
  // Takes ownership of surface. TODO(apatrick): separate notion of surface
  // from context.
  explicit GLContextEGL(GLSurfaceEGL* surface);

  virtual ~GLContextEGL();

  // Initialize an EGL context.
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
  scoped_ptr<GLSurfaceEGL> surface_;
  EGLContext context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextEGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_EGL_H_
