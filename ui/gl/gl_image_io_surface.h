// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <IOSurface/IOSurfaceAPI.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  GLImageIOSurface(const gfx::Size& size, unsigned internalformat);

  bool Initialize(IOSurfaceRef io_surface, GpuMemoryBuffer::Format format);

  // Overridden from GLImage:
  void Destroy(bool have_context) override;
  gfx::Size GetSize() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  void WillUseTexImage() override {}
  void DidUseTexImage() override {}
  void WillModifyTexImage() override {}
  void DidModifyTexImage() override {}
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            OverlayTransform transform,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;

 protected:
  ~GLImageIOSurface() override;

 private:
  const gfx::Size size_;
  const unsigned internalformat_;
  GpuMemoryBuffer::Format format_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GLImageIOSurface);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_
