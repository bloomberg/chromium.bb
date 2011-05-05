// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/size.h"

namespace gfx {

class GLSurfaceCGL;

// Encapsulates a CGL OpenGL context.
class GLContextCGL : public GLContext {
 public:
  explicit GLContextCGL(GLSurfaceCGL* surface);
  virtual ~GLContextCGL();

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

 private:
  scoped_ptr<GLSurfaceCGL> surface_;
  void* context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextCGL);
};

}  // namespace gfx
