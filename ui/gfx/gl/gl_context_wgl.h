// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_WGL_H_
#define UI_GFX_GL_GL_CONTEXT_WGL_H_

#include <string>

#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class GLSurface;

// This class is a wrapper around a GL context.
class GLContextWGL : public GLContext {
 public:
  GLContextWGL();
  virtual ~GLContextWGL();

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
  HGLRC context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextWGL);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_WGL_H_
