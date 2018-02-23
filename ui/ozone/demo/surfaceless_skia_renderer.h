// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SURFACELESS_SKIA_RENDERER_H_
#define UI_OZONE_DEMO_SURFACELESS_SKIA_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/demo/skia_renderer.h"

namespace ui {
class OverlayCandidatesOzone;

class SurfacelessSkiaRenderer : public SkiaRenderer {
 public:
  SurfacelessSkiaRenderer(gfx::AcceleratedWidget widget,
                          const scoped_refptr<gl::GLSurface>& surface,
                          const gfx::Size& size);
  ~SurfacelessSkiaRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  // SkiaRenderer:
  void RenderFrame() override;
  void PostRenderFrameTask(gfx::SwapResult result) override;

  class BufferWrapper;

  std::unique_ptr<BufferWrapper> buffers_[2];

  std::unique_ptr<BufferWrapper> overlay_buffer_[2];
  bool disable_primary_plane_ = false;
  gfx::Rect primary_plane_rect_;

  std::unique_ptr<OverlayCandidatesOzone> overlay_checker_;

  int back_buffer_ = 0;

  base::WeakPtrFactory<SurfacelessSkiaRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfacelessSkiaRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SURFACELESS_SKIA_RENDERER_H_
