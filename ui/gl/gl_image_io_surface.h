// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <IOSurface/IOSurface.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_image.h"

#if defined(__OBJC__)
@class CALayer;
#else
typedef void* CALayer;
#endif

namespace gfx {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  GLImageIOSurface(const gfx::Size& size, unsigned internalformat);

  bool Initialize(IOSurfaceRef io_surface,
                  gfx::GenericSharedMemoryId io_surface_id,
                  BufferFormat format);

  // Overridden from GLImage:
  void Destroy(bool have_context) override;
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexSubImage(unsigned target,
                       const Point& offset,
                       const Rect& rect) override;
  void WillUseTexImage() override {}
  void DidUseTexImage() override {}
  void WillModifyTexImage() override {}
  void DidModifyTexImage() override {}
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            OverlayTransform transform,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

  base::ScopedCFTypeRef<IOSurfaceRef> io_surface();

  static void SetLayerForWidget(gfx::AcceleratedWidget widget,
                                CALayer* layer);

 protected:
  ~GLImageIOSurface() override;

 private:
  const gfx::Size size_;
  const unsigned internalformat_;
  BufferFormat format_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  gfx::GenericSharedMemoryId io_surface_id_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GLImageIOSurface);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_
