// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>

#include "base/macros.h"
#include "cc/ipc/compositor_frame_sink.mojom.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "services/ui/ws/compositor_frame_sink_client_binding.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class RenderPass;
}

namespace ui {
namespace ws {

// Responsible for redrawing the display in response to the redraw requests by
// submitting CompositorFrames to the owned CompositorFrameSink.
class FrameGenerator : public cc::mojom::CompositorFrameSinkClient {
 public:
  FrameGenerator();
  ~FrameGenerator() override;

  void SetDeviceScaleFactor(float device_scale_factor);
  void SetHighContrastMode(bool enabled);

  // Updates the WindowManager's SurfaceInfo.
  void OnSurfaceCreated(const viz::SurfaceInfo& surface_info);

  // Swaps the |window_manager_surface_info_| with that of |other|.
  void SwapSurfaceWith(FrameGenerator* other);

  void OnWindowDamaged();
  void OnWindowSizeChanged(const gfx::Size& pixel_size);
  void Bind(
      std::unique_ptr<cc::mojom::CompositorFrameSink> compositor_frame_sink);

 private:
  // cc::mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;

  // Generates the CompositorFrame.
  cc::CompositorFrame GenerateCompositorFrame();

  // DrawWindow creates SurfaceDrawQuad for the window manager and appends it to
  // the provided cc::RenderPass.
  void DrawWindow(cc::RenderPass* pass);

  void SetNeedsBeginFrame(bool needs_begin_frame);

  float device_scale_factor_ = 1.f;
  gfx::Size pixel_size_;

  std::unique_ptr<cc::mojom::CompositorFrameSink> compositor_frame_sink_;
  cc::BeginFrameArgs last_begin_frame_args_;
  cc::BeginFrameAck current_begin_frame_ack_;
  bool high_contrast_mode_enabled_ = false;
  gfx::Size last_submitted_frame_size_;
  viz::LocalSurfaceId local_surface_id_;
  viz::LocalSurfaceIdAllocator id_allocator_;
  float last_device_scale_factor_ = 0.0f;

  viz::SurfaceInfo window_manager_surface_info_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
