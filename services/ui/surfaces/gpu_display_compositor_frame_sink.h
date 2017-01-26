// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_GPU_DISPLAY_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_SURFACES_GPU_DISPLAY_COMPOSITOR_FRAME_SINK_H_

#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/ui/surfaces/gpu_compositor_frame_sink.h"

namespace ui {

class GpuDisplayCompositorFrameSink : public GpuCompositorFrameSink,
                                      public cc::mojom::DisplayPrivate {
 public:
  GpuDisplayCompositorFrameSink(
      DisplayCompositor* display_compositor,
      const cc::FrameSinkId& frame_sink_id,
      std::unique_ptr<cc::Display> display,
      std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
      cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client,
      cc::mojom::DisplayPrivateAssociatedRequest display_private_request);

  ~GpuDisplayCompositorFrameSink() override;

  // cc::mojom::DisplayPrivate:
  void SetDisplayVisible(bool visible) override;
  void ResizeDisplay(const gfx::Size& size) override;
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space) override;
  void SetOutputIsSecure(bool secure) override;

 private:
  mojo::AssociatedBinding<cc::mojom::MojoCompositorFrameSink> binding_;
  mojo::AssociatedBinding<cc::mojom::DisplayPrivate> display_private_binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuDisplayCompositorFrameSink);
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_GPU_DISPLAY_COMPOSITOR_FRAME_SINK_H_
