// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
#define UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/ozone/demo/gl_renderer.h"

namespace gl {
class GLImage;
}

namespace ui {
class OverlayCandidatesOzone;

class SurfacelessGlRenderer : public GlRenderer {
 public:
  SurfacelessGlRenderer(gfx::AcceleratedWidget widget,
                        const scoped_refptr<gl::GLSurface>& surface,
                        const gfx::Size& size);
  ~SurfacelessGlRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  // GlRenderer:
  void RenderFrame() override;
  void PostRenderFrameTask(gfx::SwapResult result) override;

  class BufferWrapper {
   public:
    BufferWrapper();
    ~BufferWrapper();

    gl::GLImage* image() const { return image_.get(); }

    bool Initialize(gfx::AcceleratedWidget widget, const gfx::Size& size);
    void BindFramebuffer();

    const gfx::Size size() const { return size_; }

   private:
    gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
    gfx::Size size_;

    scoped_refptr<gl::GLImage> image_;
    unsigned int gl_fb_ = 0;
    unsigned int gl_tex_ = 0;
  };

  std::unique_ptr<BufferWrapper> buffers_[2];

  std::unique_ptr<BufferWrapper> overlay_buffer_[2];
  bool disable_primary_plane_ = false;
  gfx::Rect primary_plane_rect_;

  std::unique_ptr<OverlayCandidatesOzone> overlay_checker_;

  int back_buffer_ = 0;

  base::WeakPtrFactory<SurfacelessGlRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfacelessGlRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
