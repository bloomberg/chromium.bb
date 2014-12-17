// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
#define UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_

#include "ui/ozone/demo/gl_renderer.h"
#include "ui/ozone/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"

namespace gfx {
class GLImage;
}  // namespace gfx

namespace ui {

class SurfacelessGlRenderer : public GlRenderer {
 public:
  SurfacelessGlRenderer(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~SurfacelessGlRenderer() override;

  // Renderer:
  bool Initialize() override;
  void RenderFrame() override;

 private:
  class BufferWrapper {
   public:
    BufferWrapper();
    ~BufferWrapper();

    bool Initialize(GpuMemoryBufferFactoryOzoneNativeBuffer* buffer_factory,
                    gfx::AcceleratedWidget widget,
                    const gfx::Size& size);
    void BindFramebuffer();
    void SchedulePlane();

   private:
    gfx::AcceleratedWidget widget_;
    gfx::Size size_;

    scoped_refptr<gfx::GLImage> image_;
    unsigned int gl_fb_;
    unsigned int gl_tex_;
  };

  GpuMemoryBufferFactoryOzoneNativeBuffer buffer_factory_;
  BufferWrapper buffers_[2];
  int back_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SurfacelessGlRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
