// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
#define SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_

#include "base/macros.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/output/context_provider.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {
namespace ws {

class ServerWindow;
class ServerWindowCompositorFrameSink;
class ServerWindowCompositorFrameSinkManagerTestApi;

struct CompositorFrameSinkData {
  CompositorFrameSinkData();
  CompositorFrameSinkData(CompositorFrameSinkData&& other);
  ~CompositorFrameSinkData();

  CompositorFrameSinkData& operator=(CompositorFrameSinkData&& other);

  cc::SurfaceInfo latest_submitted_surface_info;
  cc::mojom::MojoCompositorFrameSinkPrivatePtr compositor_frame_sink;
  cc::mojom::MojoCompositorFrameSinkPrivateRequest
      pending_compositor_frame_sink_request;
  cc::FrameSinkId frame_sink_id;
};

// ServerWindowCompositorFrameSinkManager tracks the surfaces associated with a
// ServerWindow.
class ServerWindowCompositorFrameSinkManager {
 public:
  explicit ServerWindowCompositorFrameSinkManager(ServerWindow* window);
  ~ServerWindowCompositorFrameSinkManager();

  // Creates a new CompositorFrameSink of the specified type, replacing the
  // existing one of the specified type.
  void CreateDisplayCompositorFrameSink(
      gfx::AcceleratedWidget widget,
      cc::mojom::MojoCompositorFrameSinkAssociatedRequest sink_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_request);

  void CreateOffscreenCompositorFrameSink(
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  // Adds the provided |frame_sink_id| to this ServerWindow's associated
  // CompositorFrameSink if possible. If this ServerWindow does not have
  // an associated CompositorFrameSink then this method will recursively
  // walk up the window hierarchy and register itself with the first ancestor
  // that has a CompositorFrameSink of the same type. This method returns
  // the FrameSinkId that is the first composited ancestor of the ServerWindow
  // assocaited with the provided |frame_sink_id|.
  void AddChildFrameSinkId(const cc::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkId(const cc::FrameSinkId& frame_sink_id);

  ServerWindow* window() { return window_; }

  bool HasCompositorFrameSink() const;

  gfx::Size GetLatestFrameSize() const;
  cc::SurfaceId GetLatestSurfaceId() const;
  void SetLatestSurfaceInfo(const cc::SurfaceInfo& surface_info);

  void OnRootChanged(ServerWindow* old_root, ServerWindow* new_root);

 private:
  friend class ServerWindowCompositorFrameSinkManagerTestApi;
  friend class ServerWindowCompositorFrameSink;

  ServerWindow* window_;

  std::unique_ptr<CompositorFrameSinkData> frame_sink_data_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowCompositorFrameSinkManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
