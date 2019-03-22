// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_

#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/common/linux/gbm_buffer.h"

namespace ui {

class WaylandSurfaceFactory;
class WaylandConnectionProxy;

class GbmPixmapWayland : public gfx::NativePixmap {
 public:
  GbmPixmapWayland(WaylandSurfaceFactory* surface_manager,
                   WaylandConnectionProxy* connection);

  // Creates a buffer object and initializes the pixmap buffer.
  bool InitializeBuffer(gfx::Size size,
                        gfx::BufferFormat format,
                        gfx::BufferUsage usage);

  // gfx::NativePixmap overrides:
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
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  gfx::NativePixmapHandle ExportHandle() override;

 private:
  ~GbmPixmapWayland() override;

  // Asks Wayland to create a dmabuf based wl_buffer.
  void CreateZwpLinuxDmabuf();

  // gbm_bo wrapper for struct gbm_bo.
  std::unique_ptr<GbmBuffer> gbm_bo_;

  WaylandSurfaceFactory* surface_manager_ = nullptr;

  // Represents a connection to Wayland.
  WaylandConnectionProxy* connection_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmapWayland);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GBM_PIXMAP_WAYLAND_H_
