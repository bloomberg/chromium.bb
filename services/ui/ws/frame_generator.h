// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/embedded_surface_tracker.h"
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

  // Adds a reference to new surface with |surface_id| for |window|. This
  // reference is to ensure the surface is not deleted while it's still being
  // displayed. The display root surface has a reference from the top level
  // root. All child surfaces are embedded by the display root and receive a
  // reference from it.
  //
  // If a new display root Surface is created, then all child surfaces will
  // receive a reference from the new display root so they are not deleted with
  // the old display root.
  //
  // If there is an existing reference to an old surface with the same
  // FrameSinkId then that reference will be removed after the next
  // CompositorFrame is submitted.
  void OnSurfaceCreated(const cc::SurfaceId& surface_id, ServerWindow* window);

 private:
  friend class ui::ws::test::FrameGeneratorTest;

  // cc::mojom::MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface() override;

  // Updates the display surface SurfaceId using new value in |local_frame_id_|.
  void UpdateDisplaySurfaceId();

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
  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink_;
  cc::mojom::DisplayPrivatePtr display_private_;

  // Tracks surface references for embedded surfaces.
  cc::EmbeddedSurfaceTracker surface_tracker_;

  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> binding_;

  base::WeakPtrFactory<FrameGenerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
