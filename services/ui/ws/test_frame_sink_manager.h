// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_TEST_FRAME_SINK_MANAGER_H_
#define SERVICES_UI_WS_TEST_FRAME_SINK_MANAGER_H_

#include "base/macros.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"

namespace ui {
namespace ws {

class TestFrameSinkManagerImpl : public viz::mojom::FrameSinkManager {
 public:
  TestFrameSinkManagerImpl() = default;
  ~TestFrameSinkManagerImpl() override = default;

 private:
  // viz::mojom::FrameSinkManager:
  void RegisterFrameSinkId(const viz::FrameSinkId& frame_sink_id) override {}
  void InvalidateFrameSinkId(const viz::FrameSinkId& frame_sink_id) override {}
  void CreateRootCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      const viz::RendererSettings& renderer_settings,
      viz::mojom::CompositorFrameSinkAssociatedRequest request,
      viz::mojom::CompositorFrameSinkClientPtr client,
      viz::mojom::DisplayPrivateAssociatedRequest display_private_request)
      override {}
  void CreateCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id,
      viz::mojom::CompositorFrameSinkRequest request,
      viz::mojom::CompositorFrameSinkClientPtr client) override {}
  void RegisterFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override {}
  void UnregisterFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override {}
  void AssignTemporaryReference(const viz::SurfaceId& surface_id,
                                const viz::FrameSinkId& owner) override {}
  void DropTemporaryReference(const viz::SurfaceId& surface_id) override {}

  DISALLOW_COPY_AND_ASSIGN(TestFrameSinkManagerImpl);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_TEST_FRAME_SINK_MANAGER_H_
