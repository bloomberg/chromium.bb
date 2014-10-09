// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_CGL_H_
#define UI_GL_GL_CONTEXT_CGL_H_

#include <OpenGL/CGLTypes.h>

#include "ui/gl/gl_context.h"

namespace gfx {

class GLSurface;

// Encapsulates a CGL OpenGL context.
class GLContextCGL : public GLContextReal {
 public:
  explicit GLContextCGL(GLShareGroup* share_group);

  // Implement GLContext.
  virtual bool Initialize(GLSurface* compatible_surface,
                          GpuPreference gpu_preference) override;
  virtual void Destroy() override;
  virtual bool MakeCurrent(GLSurface* surface) override;
  virtual void ReleaseCurrent(GLSurface* surface) override;
  virtual bool IsCurrent(GLSurface* surface) override;
  virtual void* GetHandle() override;
  virtual void SetSwapInterval(int interval) override;
  virtual bool GetTotalGpuMemory(size_t* bytes) override;
  virtual void SetSafeToForceGpuSwitch() override;

 protected:
  virtual ~GLContextCGL();

 private:
  GpuPreference GetGpuPreference();

  void* context_;
  GpuPreference gpu_preference_;

  CGLPixelFormatObj discrete_pixelformat_;

  int screen_;
  int renderer_id_;
  bool safe_to_force_gpu_switch_;

  DISALLOW_COPY_AND_ASSIGN(GLContextCGL);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_CGL_H_
