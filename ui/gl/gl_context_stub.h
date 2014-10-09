// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_STUB_H_
#define UI_GL_GL_CONTEXT_STUB_H_

#include "ui/gl/gl_context.h"

namespace gfx {

// A GLContext that does nothing for unit tests.
class GL_EXPORT GLContextStub : public GLContextReal {
 public:
  GLContextStub();

  // Implement GLContext.
  virtual bool Initialize(GLSurface* compatible_surface,
                          GpuPreference gpu_preference) override;
  virtual void Destroy() override;
  virtual bool MakeCurrent(GLSurface* surface) override;
  virtual void ReleaseCurrent(GLSurface* surface) override;
  virtual bool IsCurrent(GLSurface* surface) override;
  virtual void* GetHandle() override;
  virtual void SetSwapInterval(int interval) override;
  virtual std::string GetExtensions() override;
  virtual std::string GetGLRenderer() override;

 protected:
  virtual ~GLContextStub();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLContextStub);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_STUB_H_
