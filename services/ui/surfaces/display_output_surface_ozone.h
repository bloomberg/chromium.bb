// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DISPLAY_OUTPUT_SURFACE_OZONE_H_
#define SERVICES_UI_SURFACES_DISPLAY_OUTPUT_SURFACE_OZONE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "cc/output/context_provider.h"
#include "cc/output/in_process_context_provider.h"
#include "cc/output/output_surface.h"
#include "components/display_compositor/gl_helper.h"
#include "services/ui/surfaces/display_output_surface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace cc {
class SyntheticBeginFrameSource;
}

namespace display_compositor {
class BufferQueue;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace ui {

// An OutputSurface implementation that directly draws and swap to a GL
// "surfaceless" surface (aka one backed by a buffer managed explicitly in
// mus/ozone. This class is adapted from
// GpuSurfacelessBrowserCompositorOutputSurface.
class DisplayOutputSurfaceOzone : public DisplayOutputSurface {
 public:
  DisplayOutputSurfaceOzone(
      scoped_refptr<cc::InProcessContextProvider> context_provider,
      gfx::AcceleratedWidget widget,
      cc::SyntheticBeginFrameSource* synthetic_begin_frame_source,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      uint32_t target,
      uint32_t internalformat);

  ~DisplayOutputSurfaceOzone() override;

  // TODO(rjkroege): Implement the equivalent of Reflector.

 private:
  // cc::OutputSurface implementation.
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;

  // DisplayOutputSurface:
  void DidReceiveSwapBuffersAck(gfx::SwapResult result) override;

  display_compositor::GLHelper gl_helper_;
  std::unique_ptr<display_compositor::BufferQueue> buffer_queue_;

  gfx::Size reshape_size_;
  gfx::Size swap_size_;

  DISALLOW_COPY_AND_ASSIGN(DisplayOutputSurfaceOzone);
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_DISPLAY_OUTPUT_SURFACE_OZONE_H_
