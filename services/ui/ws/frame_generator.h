// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_reference.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_tracker.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class RenderPass;
class SurfaceId;
}

namespace ui {
namespace ws {

namespace test {
class FrameGeneratorTest;
}

class FrameGeneratorDelegate;
class ServerWindow;

// Responsible for redrawing the display in response to the redraw requests by
// submitting CompositorFrames to the owned CompositorFrameSink.
class FrameGenerator : public ServerWindowTracker,
                       public cc::mojom::MojoCompositorFrameSinkClient {
 public:
  FrameGenerator(FrameGeneratorDelegate* delegate, ServerWindow* root_window);
  ~FrameGenerator() override;

  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

  // Schedules a redraw for the provided region.
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget);

  // If |window| corresponds to the active WM for the display then update
  // |window_manager_surface_id_|.
  void OnSurfaceCreated(const cc::SurfaceId& surface_id, ServerWindow* window);

 private:
  friend class ui::ws::test::FrameGeneratorTest;

  // cc::mojom::MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface() override;

  // Generates the CompositorFrame.
  cc::CompositorFrame GenerateCompositorFrame(const gfx::Rect& output_rect);

  // DrawWindow creates SurfaceDrawQuad for the provided ServerWindow and
  // appends it to the provided cc::RenderPass.
  void DrawWindow(cc::RenderPass* pass, ServerWindow* window);

  // ServerWindowObserver implementation.
  void OnWindowDestroying(ServerWindow* window) override;

  FrameGeneratorDelegate* delegate_;
  ServerWindow* const root_window_;
  float device_scale_factor_ = 1.f;

  gfx::Size last_submitted_frame_size_;
  cc::LocalFrameId local_frame_id_;
  cc::SurfaceIdAllocator id_allocator_;
  cc::mojom::MojoCompositorFrameSinkAssociatedPtr compositor_frame_sink_;
  cc::mojom::DisplayPrivateAssociatedPtr display_private_;

  cc::SurfaceId window_manager_surface_id_;

  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> binding_;

  base::WeakPtrFactory<FrameGenerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
