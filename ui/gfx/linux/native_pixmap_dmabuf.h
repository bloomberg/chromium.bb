// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_NATIVE_PIXMAP_DMABUF_H_
#define UI_GFX_LINUX_NATIVE_PIXMAP_DMABUF_H_

#include <stdint.h>

#include <memory>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"

namespace gfx {

// This class converts a gfx::NativePixmapHandle to a gfx::NativePixmap.
// It is useful because gl::GLImageNativePixmap::Initialize only takes
// a gfx::NativePixmap as input.
class GFX_EXPORT NativePixmapDmaBuf : public gfx::NativePixmap {
 public:
  NativePixmapDmaBuf(const gfx::Size& size,
                     gfx::BufferFormat format,
                     const gfx::NativePixmapHandle& handle);

  // NativePixmap:
  void* GetEGLClientBuffer() const override;
  bool AreDmaBufFdsValid() const override;
  size_t GetDmaBufFdCount() const override;
  int GetDmaBufFd(size_t plane) const override;
  int GetDmaBufPitch(size_t plane) const override;
  int GetDmaBufOffset(size_t plane) const override;
  uint64_t GetDmaBufModifier(size_t plane) const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  uint32_t GetUniqueId() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend) override;
  gfx::NativePixmapHandle ExportHandle() override;

 protected:
  ~NativePixmapDmaBuf() override;

 private:
  gfx::Size size_;
  gfx::BufferFormat format_;
  std::vector<base::ScopedFD> fds_;
  std::vector<gfx::NativePixmapPlane> planes_;

  DISALLOW_COPY_AND_ASSIGN(NativePixmapDmaBuf);
};

}  // namespace gfx

#endif  // UI_GFX_LINUX_NATIVE_PIXMAP_DMABUF_H_
