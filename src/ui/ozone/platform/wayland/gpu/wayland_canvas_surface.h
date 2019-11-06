// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

class WaylandBufferManagerGpu;

class WaylandCanvasSurface : public SurfaceOzoneCanvas {
 public:
  WaylandCanvasSurface(WaylandBufferManagerGpu* buffer_manager,
                       gfx::AcceleratedWidget widget);
  ~WaylandCanvasSurface() override;

  // SurfaceOzoneCanvas
  sk_sp<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  void OnGetSizeForWidget(const gfx::Size& widget_size) { size_ = widget_size; }

  WaylandBufferManagerGpu* const buffer_manager_;
  const gfx::AcceleratedWidget widget_;

  gfx::Size size_;
  sk_sp<SkSurface> sk_surface_;

  // The id of the current existing buffer. Even though, there can only be one
  // buffer (SkSurface) at a time, the buffer manager on the browser process
  // side requires buffer id to be passed.
  uint32_t buffer_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WaylandCanvasSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CANVAS_SURFACE_H_
