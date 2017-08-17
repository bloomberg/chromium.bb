// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
#define SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_

#include "base/macros.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame.mojom.h"

namespace ui {
namespace ws {

class ServerWindow;

// ServerWindowCompositorFrameSinkManager tracks the surfaces associated with a
// ServerWindow.
class ServerWindowCompositorFrameSinkManager {
 public:
  ServerWindowCompositorFrameSinkManager(
      const viz::FrameSinkId& frame_sink_id,
      viz::mojom::FrameSinkManager* frame_sink_manager);
  ~ServerWindowCompositorFrameSinkManager();

  // Creates a new CompositorFrameSink of the specified type, replacing the
  // existing one of the specified type.
  void CreateRootCompositorFrameSink(
      gfx::AcceleratedWidget widget,
      viz::mojom::CompositorFrameSinkAssociatedRequest sink_request,
      viz::mojom::CompositorFrameSinkClientPtr client,
      viz::mojom::DisplayPrivateAssociatedRequest display_request);

  void CreateCompositorFrameSink(
      viz::mojom::CompositorFrameSinkRequest request,
      viz::mojom::CompositorFrameSinkClientPtr client);

  // Claims this FrameSinkId will embed |surface_id| so it should own the
  // temporary reference to |surface_id|.
  void ClaimTemporaryReference(const viz::SurfaceId& surface_id);

  // TODO(kylechar): Add method to reregister |frame_sink_id_| when viz service
  // has crashed.

 private:
  const viz::FrameSinkId frame_sink_id_;
  viz::mojom::FrameSinkManager* const frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowCompositorFrameSinkManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
