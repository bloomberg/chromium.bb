// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_GPU_OFFSCREEN_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_SURFACES_GPU_OFFSCREEN_COMPOSITOR_FRAME_SINK_H_

#include "services/ui/surfaces/gpu_compositor_frame_sink.h"

namespace ui {

class GpuOffscreenCompositorFrameSink : public GpuCompositorFrameSink {
 public:
  GpuOffscreenCompositorFrameSink(
      DisplayCompositor* display_compositor,
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  ~GpuOffscreenCompositorFrameSink() override;

 private:
  mojo::Binding<cc::mojom::MojoCompositorFrameSink> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuOffscreenCompositorFrameSink);
};

}  // namespace ui

#endif  // SERVICES_UI_SURFACES_GPU_OFFSCREEN_COMPOSITOR_FRAME_SINK_H_
