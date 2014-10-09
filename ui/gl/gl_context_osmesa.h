// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_OSMESA_H_
#define UI_GL_GL_CONTEXT_OSMESA_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gl/gl_context.h"

typedef struct osmesa_context* OSMesaContext;

namespace gfx {

class GLShareGroup;
class GLSurface;

// Encapsulates an OSMesa OpenGL context that uses software rendering.
class GLContextOSMesa : public GLContextReal {
 public:
  explicit GLContextOSMesa(GLShareGroup* share_group);

  // Implement GLContext.
  virtual bool Initialize(GLSurface* compatible_surface,
                          GpuPreference gpu_preference) override;
  virtual void Destroy() override;
  virtual bool MakeCurrent(GLSurface* surface) override;
  virtual void ReleaseCurrent(GLSurface* surface) override;
  virtual bool IsCurrent(GLSurface* surface) override;
  virtual void* GetHandle() override;
  virtual void SetSwapInterval(int interval) override;

 protected:
  virtual ~GLContextOSMesa();

 private:
  OSMesaContext context_;

  DISALLOW_COPY_AND_ASSIGN(GLContextOSMesa);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_OSMESA_H_
