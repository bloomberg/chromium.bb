// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_GLX_H_
#define UI_GL_GL_CONTEXT_GLX_H_

#include <string>

#include "base/compiler_specific.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

namespace gfx {

class GLSurface;

// Encapsulates a GLX OpenGL context.
class GL_EXPORT GLContextGLX : public GLContextReal {
 public:
  explicit GLContextGLX(GLShareGroup* share_group);

  XDisplay* display();

  // Implement GLContext.
  virtual bool Initialize(
      GLSurface* compatible_surface, GpuPreference gpu_preference) override;
  virtual void Destroy() override;
  virtual bool MakeCurrent(GLSurface* surface) override;
  virtual void ReleaseCurrent(GLSurface* surface) override;
  virtual bool IsCurrent(GLSurface* surface) override;
  virtual void* GetHandle() override;
  virtual void SetSwapInterval(int interval) override;
  virtual std::string GetExtensions() override;
  virtual bool GetTotalGpuMemory(size_t* bytes) override;
  virtual bool WasAllocatedUsingRobustnessExtension() override;

 protected:
  virtual ~GLContextGLX();

 private:
  void* context_;
  XDisplay* display_;

  DISALLOW_COPY_AND_ASSIGN(GLContextGLX);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_GLX_H_
