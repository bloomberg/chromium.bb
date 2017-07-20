// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_LOCAL_LAYER_TREE_FRAME_SINK_LOCAL_H_
#define UI_AURA_LOCAL_LAYER_TREE_FRAME_SINK_LOCAL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_client.h"
#include "ui/aura/window_port.h"
#include "ui/base/property_data.h"

namespace viz {
class CompositorFrameSinkSupport;
class HostFrameSinkManager;
}

namespace aura {

// cc::LayerTreeFrameSink implementation for classic aura, e.g. not mus.
// aura::Window::CreateLayerTreeFrameSink creates this class for a given
// aura::Window, and then the sink can be used for submitting frames to the
// aura::Window's ui::Layer.
class LayerTreeFrameSinkLocal : public cc::LayerTreeFrameSink,
                                public viz::CompositorFrameSinkSupportClient,
                                public cc::ExternalBeginFrameSourceClient {
 public:
  LayerTreeFrameSinkLocal(const viz::FrameSinkId& frame_sink_id,
                          viz::HostFrameSinkManager* host_frame_sink_manager);
  ~LayerTreeFrameSinkLocal() override;

  using SurfaceChangedCallback =
      base::Callback<void(const viz::SurfaceId&, const gfx::Size&)>;
  // Set a callback which will be called when the surface is changed.
  void SetSurfaceChangedCallback(const SurfaceChangedCallback& callback);

  // cc::LayerTreeFrameSink:
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const cc::BeginFrameAck& ack) override;

  // viz::CompositorFrameSinkSupportClient:
  void DidReceiveCompositorFrameAck(
      const std::vector<cc::ReturnedResource>& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(
      const std::vector<cc::ReturnedResource>& resources) override;
  void WillDrawSurface(const viz::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override {}
  void OnBeginFramePausedChanged(bool paused) override;

  // cc::ExternalBeginFrameSourceClient:
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

 private:
  const viz::FrameSinkId frame_sink_id_;
  viz::HostFrameSinkManager* const host_frame_sink_manager_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  gfx::Size surface_size_;
  float device_scale_factor_ = 0;
  viz::LocalSurfaceIdAllocator id_allocator_;
  viz::LocalSurfaceId local_surface_id_;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;
  std::unique_ptr<base::ThreadChecker> thread_checker_;
  SurfaceChangedCallback surface_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeFrameSinkLocal);
};

}  // namespace aura

#endif  // UI_AURA_LOCAL_LAYER_TREE_FRAME_SINK_LOCAL_H_
