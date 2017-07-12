// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_SINK_MANAGER_CLIENT_BINDING_H_
#define SERVICES_UI_WS_FRAME_SINK_MANAGER_CLIENT_BINDING_H_

#include "base/macros.h"
#include "cc/ipc/frame_sink_manager.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ui {
namespace ws {

class GpuHost;
class WindowServer;

// FrameSinkManagerClientBinding manages the binding between a WindowServer and
// its FrameSinkManager. FrameSinkManagerClientBinding exists so that a mock
// implementation of FrameSinkManager can be injected for tests. WindowServer
// owns its associated FrameSinkManagerClientBinding.
class FrameSinkManagerClientBinding : public cc::mojom::FrameSinkManager {
 public:
  FrameSinkManagerClientBinding(
      cc::mojom::FrameSinkManagerClient* frame_sink_manager_client,
      GpuHost* gpu_host);
  ~FrameSinkManagerClientBinding() override;

 private:
  // cc::mojom::FrameSinkManager:
  void CreateRootCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      cc::mojom::CompositorFrameSinkAssociatedRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override;
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      cc::mojom::CompositorFrameSinkRequest request,
      cc::mojom::CompositorFrameSinkPrivateRequest private_request,
      cc::mojom::CompositorFrameSinkClientPtr client) override;
  void RegisterFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void UnregisterFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void DropTemporaryReference(const viz::SurfaceId& surface_id) override;

  mojo::Binding<cc::mojom::FrameSinkManagerClient>
      frame_sink_manager_client_binding_;
  cc::mojom::FrameSinkManagerPtr frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(FrameSinkManagerClientBinding);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_SINK_MANAGER_CLIENT_BINDING_H_
