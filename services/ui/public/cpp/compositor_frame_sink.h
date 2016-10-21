// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_PUBLIC_CPP_COMPOSITOR_FRAME_SINK_H_

#include "base/macros.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/context_provider.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/cpp/window_surface.h"
#include "services/ui/public/cpp/window_surface_client.h"

namespace gpu {
class GpuChannelHost;
}

namespace ui {

class CompositorFrameSink : public cc::CompositorFrameSink,
                            public WindowSurfaceClient {
 public:
  CompositorFrameSink(scoped_refptr<cc::ContextProvider> context_provider,
                      std::unique_ptr<WindowSurface> surface);
  ~CompositorFrameSink() override;

  // cc::CompositorFrameSink implementation.
  bool BindToClient(cc::CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;

 private:
  // WindowSurfaceClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;

  std::unique_ptr<cc::BeginFrameSource> begin_frame_source_;
  std::unique_ptr<WindowSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSink);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_COMPOSITOR_FRAME_SINK_H_
