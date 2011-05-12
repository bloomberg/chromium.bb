// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_STUB_H_
#define UI_GFX_GL_GL_SURFACE_STUB_H_
#pragma once

#include "ui/gfx/gl/gl_surface.h"

namespace gfx {

// A GLSurface that does nothing for unit tests.
class GLSurfaceStub : public GLSurface {
 public:
  virtual ~GLSurfaceStub();

  void SetSize(const gfx::Size& size) { size_ = size; }

  // Implement GLSurface.
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

 private:
  gfx::Size size_;
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_STUB_H_
