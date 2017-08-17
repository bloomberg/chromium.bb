// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_SINK_MANAGER_CLIENT_BINDING_H_
#define SERVICES_UI_WS_FRAME_SINK_MANAGER_CLIENT_BINDING_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"

namespace ui {
namespace ws {

class GpuHost;
class WindowServer;

// FrameSinkManagerClientBinding manages the binding between a WindowServer and
// its FrameSinkManager. FrameSinkManagerClientBinding exists so that a mock
// implementation of FrameSinkManager can be injected for tests. WindowServer
// owns its associated FrameSinkManagerClientBinding.
class FrameSinkManagerClientBinding : public viz::mojom::FrameSinkManager {
 public:
  FrameSinkManagerClientBinding(
      viz::mojom::FrameSinkManagerClient* frame_sink_manager_client,
      GpuHost* gpu_host);
  ~FrameSinkManagerClientBinding() override;

 private:
  // viz::mojom::FrameSinkManager:
  void RegisterFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void InvalidateFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void CreateRootCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      const viz::RendererSettings& renderer_settings,
      viz::mojom::CompositorFrameSinkAssociatedRequest request,
      viz::mojom::CompositorFrameSinkClientPtr client,
      viz::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override;
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      viz::mojom::CompositorFrameSinkRequest request,
      viz::mojom::CompositorFrameSinkClientPtr client) override;
  void RegisterFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void AssignTemporaryReference(const viz::SurfaceId& surface_id,
                                const viz::FrameSinkId& owner) override;
  void DropTemporaryReference(const viz::SurfaceId& surface_id) override;

  mojo::Binding<viz::mojom::FrameSinkManagerClient>
      frame_sink_manager_client_binding_;
  viz::mojom::FrameSinkManagerPtr frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerClientBinding);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_SINK_MANAGER_CLIENT_BINDING_H_
