// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_EGL_H_
#define UI_GFX_GL_GL_CONTEXT_EGL_H_
#pragma once

#include <string>

#include "ui/gfx/gl/gl_context.h"

typedef void* EGLContext;

namespace gfx {

class GLSurface;

// Encapsulates an EGL OpenGL ES context.
class GLContextEGL : public GLContext {
 public:
  GLContextEGL();
  virtual ~GLContextEGL();

  // Implement GLContext.
  virtual bool Initialize(GLContext* shared_context,
                          GLSurface* compatible_surface);
  virtual void Destroy();
  virtual bool MakeCurrent(GLSurface* surface);
  virtual void ReleaseCurrent(GLSurface* surface);
  virtual bool IsCurrent(GLSurface* surface);
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);
  virtual std::string GetExtensions();

 private:
  EGLContext context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextEGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_EGL_H_
