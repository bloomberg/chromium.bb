// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_OSMESA_H_
#define UI_GFX_GL_GL_CONTEXT_OSMESA_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface_osmesa.h"
#include "ui/gfx/size.h"

typedef struct osmesa_context *OSMesaContext;

namespace gfx {

// Encapsulates an OSMesa OpenGL context that uses software rendering.
class GLContextOSMesa : public GLContext {
 public:
  explicit GLContextOSMesa(GLSurfaceOSMesa* surface);
  virtual ~GLContextOSMesa();

  // Initialize an OSMesa GL context.
  bool Initialize(GLuint format, GLContext* shared_context);

  // Implement GLContext.
  virtual void Destroy();
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval);

 private:
  scoped_ptr<GLSurfaceOSMesa> surface_;
  OSMesaContext context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextOSMesa);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_OSMESA_H_
