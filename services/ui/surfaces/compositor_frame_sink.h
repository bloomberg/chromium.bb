// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_SURFACES_COMPOSITOR_FRAME_SINK_H_

#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "services/ui/surfaces/display_compositor.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Display;
}

namespace gpu {
class GpuChannelHost;
}

namespace ui {
namespace surfaces {

// TODO(fsamuel): This should become a mojo interface for the mus-gpu split.
// TODO(fsamuel): This should not be a SurfaceFactoryClient.
// The CompositorFrameSink receives CompositorFrames from all sources,
// creates a top-level CompositorFrame once per tick, and generates graphical
// output.
class CompositorFrameSink : public cc::SurfaceFactoryClient,
                            public cc::DisplayClient {
 public:
  CompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      gfx::AcceleratedWidget widget,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel,
      const scoped_refptr<DisplayCompositor>& display_compositor);
  ~CompositorFrameSink() override;

  // CompositorFrameSink embedders submit a CompositorFrame when content on the
  // display should be changed. A well-behaving embedder should only submit
  // a CompositorFrame once per BeginFrame tick. The callback is called the
  // first time this frame is used to draw, or if the frame is discarded.
  void SubmitCompositorFrame(cc::CompositorFrame frame,
                             const base::Callback<void()>& callback);

  // TODO(fsamuel): This is used for surface hittesting and should not be
  // exposed outside of CompositorFrameSink.
  const cc::LocalFrameId& local_frame_id() const { return local_frame_id_; }

  // This requests the display CompositorFrame be rendered and given to the
  // callback within CopyOutputRequest.
  void RequestCopyOfOutput(
      std::unique_ptr<cc::CopyOutputRequest> output_request);

  // TODO(fsamuel): Invent an async way to create a SurfaceNamespace
  // A SurfaceNamespace can create CompositorFrameSinks where the client can
  // make up the ID.

 private:
  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const cc::RenderPassList& render_passes) override;
  void DisplayDidDrawAndSwap() override;

  const cc::FrameSinkId frame_sink_id_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<DisplayCompositor> display_compositor_;
  cc::SurfaceFactory factory_;
  cc::SurfaceIdAllocator allocator_;
  cc::LocalFrameId local_frame_id_;

  gfx::Size display_size_;
  std::unique_ptr<cc::Display> display_;
  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSink);
};

}  // nanmespace surfaces
}  // namespace ui

#endif  // SERVICES_UI_SURFACES_COMPOSITOR_FRAME_SINK_H_
