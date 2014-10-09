// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  explicit GLImageIOSurface(const gfx::Size& size);

  bool Initialize(IOSurfaceRef io_surface);

  // Overridden from GLImage:
  virtual void Destroy(bool have_context) override;
  virtual gfx::Size GetSize() override;
  virtual bool BindTexImage(unsigned target) override;
  virtual void ReleaseTexImage(unsigned target) override {}
  virtual bool CopyTexImage(unsigned target) override;
  virtual void WillUseTexImage() override {}
  virtual void DidUseTexImage() override {}
  virtual void WillModifyTexImage() override {}
  virtual void DidModifyTexImage() override {}
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int z_order,
                                    OverlayTransform transform,
                                    const Rect& bounds_rect,
                                    const RectF& crop_rect) override;

 protected:
  virtual ~GLImageIOSurface();

 private:
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(GLImageIOSurface);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_
