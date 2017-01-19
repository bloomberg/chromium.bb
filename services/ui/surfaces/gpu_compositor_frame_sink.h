// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_GPU_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_SURFACES_GPU_COMPOSITOR_FRAME_SINK_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/ipc/display_compositor.mojom.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/referenced_surface_tracker.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace cc {
class Display;
}

namespace gfx {
class Size;
class ColorSpace;
}

namespace ui {

class DisplayCompositor;

// Server side representation of a WindowSurface.
class GpuCompositorFrameSink : public cc::CompositorFrameSinkSupportClient,
                               public cc::mojom::MojoCompositorFrameSink,
                               public cc::mojom::MojoCompositorFrameSinkPrivate,
                               public cc::mojom::DisplayPrivate {
 public:
  GpuCompositorFrameSink(
      DisplayCompositor* display_compositor,
      const cc::FrameSinkId& frame_sink_id,
      std::unique_ptr<cc::Display> display,
      std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateRequest display_private_request);

  ~GpuCompositorFrameSink() override;

  // cc::mojom::MojoCompositorFrameSink:
  void EvictFrame() override;
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SubmitCompositorFrame(const cc::LocalFrameId& local_frame_id,
                             cc::CompositorFrame frame) override;
  void Require(const cc::LocalFrameId& local_frame_id,
               const cc::SurfaceSequence& sequence) override;
  void Satisfy(const cc::SurfaceSequence& sequence) override;

  // cc::mojom::MojoCompositorFrameSinkPrivate:
  void AddChildFrameSink(const cc::FrameSinkId& child_frame_sink_id) override;
  void RemoveChildFrameSink(
      const cc::FrameSinkId& child_frame_sink_id) override;

  // cc::mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override;
  void ResizeDisplay(const gfx::Size& size) override;
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space) override;
  void SetOutputIsSecure(bool secure) override;

 private:
  // cc::CompositorFrameSinkSupportClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface() override;

  void OnClientConnectionLost();
  void OnPrivateConnectionLost();

  DisplayCompositor* const display_compositor_;

  cc::CompositorFrameSinkSupport support_;

  // Track the surface references for the surface corresponding to this
  // compositor frame sink.
  cc::ReferencedSurfaceTracker surface_tracker_;

  bool client_connection_lost_ = false;
  bool private_connection_lost_ = false;

  cc::mojom::MojoCompositorFrameSinkClientPtr client_;
  mojo::Binding<cc::mojom::MojoCompositorFrameSink> binding_;
  mojo::Binding<cc::mojom::MojoCompositorFrameSinkPrivate>
      compositor_frame_sink_private_binding_;
  mojo::Binding<cc::mojom::DisplayPrivate> display_private_binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuCompositorFrameSink);
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_GPU_COMPOSITOR_FRAME_SINK_H_
