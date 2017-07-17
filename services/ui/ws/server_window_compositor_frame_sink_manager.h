// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
#define SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_

#include "base/macros.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {
namespace ws {

class ServerWindow;

// ServerWindowCompositorFrameSinkManager tracks the surfaces associated with a
// ServerWindow.
class ServerWindowCompositorFrameSinkManager {
 public:
  explicit ServerWindowCompositorFrameSinkManager(ServerWindow* window);
  ~ServerWindowCompositorFrameSinkManager();

  // Creates a new CompositorFrameSink of the specified type, replacing the
  // existing one of the specified type.
  void CreateRootCompositorFrameSink(
      gfx::AcceleratedWidget widget,
      cc::mojom::CompositorFrameSinkAssociatedRequest sink_request,
      cc::mojom::CompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_request);

  void CreateCompositorFrameSink(
      cc::mojom::CompositorFrameSinkRequest request,
      cc::mojom::CompositorFrameSinkClientPtr client);

  // Claims this FrameSinkId will embed |surface_id| so it should own the
  // temporary reference to |surface_id|.
  void ClaimTemporaryReference(const viz::SurfaceId& surface_id);

 private:
  ServerWindow* const window_;
  cc::mojom::CompositorFrameSinkPrivatePtr compositor_frame_sink_;
  cc::mojom::CompositorFrameSinkPrivateRequest
      pending_compositor_frame_sink_request_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowCompositorFrameSinkManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
