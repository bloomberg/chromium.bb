// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_H_

#include <IOSurface/IOSurface.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gl/gl_image.h"

#if defined(__OBJC__)
@class CALayer;
#else
typedef void* CALayer;
#endif

namespace gl {

class GL_EXPORT GLImageIOSurface : public GLImage {
 public:
  GLImageIOSurface(const gfx::Size& size, unsigned internalformat);

  bool Initialize(IOSurfaceRef io_surface,
                  gfx::GenericSharedMemoryId io_surface_id,
                  gfx::BufferFormat format);

  // Overridden from GLImage:
  void Destroy(bool have_context) override;
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

  gfx::GenericSharedMemoryId io_surface_id() const { return io_surface_id_; }
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface();

  static void SetLayerForWidget(gfx::AcceleratedWidget widget, CALayer* layer);

 protected:
  ~GLImageIOSurface() override;

 private:
  const gfx::Size size_;
  const unsigned internalformat_;
  gfx::BufferFormat format_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;
  gfx::GenericSharedMemoryId io_surface_id_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(GLImageIOSurface);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_H_
