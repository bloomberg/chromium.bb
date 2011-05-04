// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/size.h"

namespace gfx {

class GLSurfaceGLX;

// Encapsulates a GLX OpenGL context.
class GLContextGLX : public GLContext {
 public:
  // Takes ownership of surface. TODO(apatrick): separate notion of surface
  // from context.
  explicit GLContextGLX(GLSurfaceGLX* surface);

  virtual ~GLContextGLX();

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
  scoped_ptr<GLSurfaceGLX> surface_;
  void* context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextGLX);
};

}  // namespace gfx
