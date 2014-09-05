// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_SURFACE_TEXTURE_H_
#define UI_GL_GL_IMAGE_SURFACE_TEXTURE_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class SurfaceTexture;

class GL_EXPORT GLImageSurfaceTexture : public GLImage {
 public:
  explicit GLImageSurfaceTexture(const gfx::Size& size);

  bool Initialize(const gfx::GpuMemoryBufferHandle& handle);

  // Overridden from GLImage:
  virtual void Destroy(bool have_context) OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE {}
  virtual bool CopyTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE {}
  virtual void DidUseTexImage() OVERRIDE {}
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int z_order,
                                    OverlayTransform transform,
                                    const Rect& bounds_rect,
                                    const RectF& crop_rect) OVERRIDE;

 protected:
  virtual ~GLImageSurfaceTexture();

 private:
  scoped_refptr<SurfaceTexture> surface_texture_;
  const gfx::Size size_;
  GLint texture_id_;

  DISALLOW_COPY_AND_ASSIGN(GLImageSurfaceTexture);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_SURFACE_TEXTURE_H_
