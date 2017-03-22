// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CompositorFrameSink;
class RenderPass;
}

namespace ui {
namespace ws {

class ServerWindow;

// Responsible for redrawing the display in response to the redraw requests by
// submitting CompositorFrames to the owned CompositorFrameSink.
class FrameGenerator : public cc::CompositorFrameSinkClient,
                       public cc::BeginFrameObserver {
 public:
  FrameGenerator(
      ServerWindow* root_window,
      std::unique_ptr<cc::CompositorFrameSink> compositor_frame_sink);
  ~FrameGenerator() override;

  void SetDeviceScaleFactor(float device_scale_factor);
  void SetHighContrastMode(bool enabled);

  // Updates the WindowManager's SurfaceInfo.
  void OnSurfaceCreated(const cc::SurfaceInfo& surface_info);

  void OnWindowDamaged();

 private:
  // cc::CompositorFrameSinkClient implementation:
  void SetBeginFrameSource(cc::BeginFrameSource* source) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void SetTreeActivationCallback(const base::Closure& callback) override;
  void DidReceiveCompositorFrameAck() override;
  void DidLoseCompositorFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw) override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;

  // cc::BeginFrameObserver implementation:
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  const cc::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  // Generates the CompositorFrame.
  cc::CompositorFrame GenerateCompositorFrame(const gfx::Rect& output_rect);

  // DrawWindow creates SurfaceDrawQuad for the window manager and appends it to
  // the provided cc::RenderPass.
  void DrawWindow(cc::RenderPass* pass);

  // SetNeedsBeginFrame sets observing_begin_frames_ and add/remove
  // FrameGenerator as an observer to/from begin_frame_source_ accordingly.
  void SetNeedsBeginFrame(bool needs_begin_frame);

  ServerWindow* const root_window_;
  float device_scale_factor_ = 1.f;

  std::unique_ptr<cc::CompositorFrameSink> compositor_frame_sink_;
  cc::BeginFrameArgs last_begin_frame_args_;
  cc::BeginFrameAck current_begin_frame_ack_;
  cc::BeginFrameSource* begin_frame_source_ = nullptr;
  bool observing_begin_frames_ = false;
  bool high_contrast_mode_enabled_ = false;

  cc::SurfaceInfo window_manager_surface_info_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
