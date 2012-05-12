// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_CGL_H_
#define UI_GL_GL_CONTEXT_CGL_H_
#pragma once

#include "ui/gl/gl_context.h"

namespace gfx {

class GLSurface;

// Encapsulates a CGL OpenGL context.
class GLContextCGL : public GLContext {
 public:
  explicit GLContextCGL(GLShareGroup* share_group);

  // Implement GLContext.
  virtual bool Initialize(GLSurface* compatible_surface,
                          GpuPreference gpu_preference) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool MakeCurrent(GLSurface* surface) OVERRIDE;
  virtual void ReleaseCurrent(GLSurface* surface) OVERRIDE;
  virtual bool IsCurrent(GLSurface* surface) OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual void SetSwapInterval(int interval) OVERRIDE;

 protected:
  virtual ~GLContextCGL();

 private:
  // Expose ForceUseOfDiscreteGPU only to GLContext implementation.
  friend class GLContext;

  GpuPreference GetGpuPreference();

  // Helper for dual-GPU support on systems where this is necessary
  // for stability reasons.
  static void ForceUseOfDiscreteGPU();

  void* context_;
  GpuPreference gpu_preference_;

  DISALLOW_COPY_AND_ASSIGN(GLContextCGL);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_CGL_H_
