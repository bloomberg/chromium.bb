// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window_delegate.h"
#include "services/ui/ws/server_window_tracker.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CompositorFrame;
class RenderPass;
class SurfaceId;
}

namespace gpu {
class GpuChannelHost;
}

namespace ui {

class DisplayCompositor;

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

  // Generates the CompositorFrame.
  cc::CompositorFrame GenerateCompositorFrame(const gfx::Rect& output_rect);

  // DrawWindowTree recursively visits ServerWindows, creating a SurfaceDrawQuad
  // for each that lacks one.
  void DrawWindowTree(cc::RenderPass* pass,
                      ServerWindow* window,
                      const gfx::Vector2d& parent_to_root_origin_offset,
                      float opacity);

  // Finds the parent surface id for |window|.
  cc::SurfaceId FindParentSurfaceId(ServerWindow* window);

  // Adds surface reference to local cache and surface manager.
  void AddSurfaceReference(const cc::SurfaceId& parent_id,
                           const cc::SurfaceId& child_id);

  // Does work necessary for adding the first surface reference.
  void AddFirstReference(const cc::SurfaceId& surface_id, ServerWindow* window);

  // Finds all Surfaces with references from |old_surface_id| and adds a new
  // reference from |new_surface_id|. The caller should remove any references
  // to |old_surface_id| afterwards to finish cleanup.
  void AddNewParentReferences(const cc::SurfaceId& old_surface_id,
                              const cc::SurfaceId& new_surface_id);

  // Removes all references to surfaces in |dead_references_|.
  void RemoveDeadSurfaceReferences();

  // Removes any retained references for the provided FrameSink.
  void RemoveFrameSinkReference(const cc::FrameSinkId& frame_sink_id);

  // Removes all retained references to surfaces.
  void RemoveAllSurfaceReferences();

  cc::mojom::DisplayCompositor* GetDisplayCompositor();

  // ServerWindowObserver implementation.
  void OnWindowDestroying(ServerWindow* window) override;

  FrameGeneratorDelegate* delegate_;
  ServerWindow* const root_window_;

  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink_;

  // Represents the top level root surface id that should reference the display
  // root surface. We don't know the actual value, because it's generated in
  // another process, this is used internally as a placeholder.
  const cc::SurfaceId top_level_root_surface_id_;

  struct SurfaceReference {
    cc::SurfaceId parent_id;
    cc::SurfaceId child_id;
  };

  // Active references held by this client to surfaces that could be embedded in
  // a CompositorFrame submitted from FrameGenerator.
  std::unordered_map<cc::FrameSinkId, SurfaceReference, cc::FrameSinkIdHash>
      active_references_;

  // References to surfaces that should be removed after a CompositorFrame has
  // been submitted and the surfaces are not being used.
  std::vector<SurfaceReference> dead_references_;

  // If a CompositorFrame for a child surface is submitted before the first
  // display root CompositorFrame, we can't add a reference from the unknown
  // display root SurfaceId. Track the child SurfaceId here and add a reference
  // to it when the display root SurfaceId is available.
  std::vector<cc::SurfaceId> waiting_for_references_;

  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> binding_;

  base::WeakPtrFactory<FrameGenerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
