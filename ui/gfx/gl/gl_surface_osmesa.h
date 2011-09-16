// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_OSMESA_H_
#define UI_GFX_GL_GL_SURFACE_OSMESA_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/size.h"

namespace gfx {

// A surface that the Mesa software renderer draws to. This is actually just a
// buffer in system memory. GetHandle returns a pointer to the buffer. These
// surfaces can be resized and resizing preserves the contents.
class GL_EXPORT GLSurfaceOSMesa : public GLSurface {
 public:
  GLSurfaceOSMesa(unsigned format, const gfx::Size& size);
  virtual ~GLSurfaceOSMesa();

  // Resize the back buffer, preserving the old content. Does nothing if the
  // size is unchanged.
  void Resize(const gfx::Size& new_size);

  // Implement GLSurface.
  virtual bool Initialize();
  virtual void Destroy();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();

  // Get the surface's format.
  unsigned GetFormat();

 private:
  void AllocateBuffer(const Size& size);

  unsigned format_;
  gfx::Size size_;
  scoped_array<int32> buffer_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOSMesa);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_SURFACE_OSMESA_H_
