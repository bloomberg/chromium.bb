// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_DISPLAY_OUTPUT_SURFACE_H_
#define SERVICES_UI_SURFACES_DISPLAY_OUTPUT_SURFACE_H_

#include <memory>

#include "cc/output/in_process_context_provider.h"
#include "cc/output/output_surface.h"

namespace cc {
class SyntheticBeginFrameSource;
}

namespace ui {

// An OutputSurface implementation that directly draws and
// swaps to an actual GL surface.
class DisplayOutputSurface : public cc::OutputSurface {
 public:
  DisplayOutputSurface(
      scoped_refptr<cc::InProcessContextProvider> context_provider,
      cc::SyntheticBeginFrameSource* synthetic_begin_frame_source);
  ~DisplayOutputSurface() override;

  // cc::OutputSurface implementation
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;

 protected:
  cc::OutputSurfaceClient* client() const { return client_; }

  // Called when a swap completion is signaled from ImageTransportSurface.
  virtual void DidReceiveSwapBuffersAck(gfx::SwapResult result);

 private:
  // Called when a swap completion is signaled from ImageTransportSurface.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac);
  void OnVSyncParametersUpdated(base::TimeTicks timebase,
                                base::TimeDelta interval);

  cc::OutputSurfaceClient* client_ = nullptr;
  cc::SyntheticBeginFrameSource* const synthetic_begin_frame_source_;
  bool set_draw_rectangle_for_frame_ = false;
  // True if the draw rectangle has been set at all since the last resize.
  bool has_set_draw_rectangle_since_last_resize_ = false;
  gfx::Size size_;

  base::WeakPtrFactory<DisplayOutputSurface> weak_ptr_factory_;
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_DISPLAY_OUTPUT_SURFACE_H_
