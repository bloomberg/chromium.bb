// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BEGIN_FRAME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BEGIN_FRAME_PROVIDER_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

struct PLATFORM_EXPORT BeginFrameProviderParams final {
  viz::FrameSinkId parent_frame_sink_id;
  viz::FrameSinkId frame_sink_id;
};

class PLATFORM_EXPORT BeginFrameProvider
    : public viz::mojom::blink::CompositorFrameSinkClient,
      public mojom::blink::OffscreenCanvasSurfaceClient {
 public:
  explicit BeginFrameProvider(
      const BeginFrameProviderParams& begin_frame_provider_params);

  void CreateCompositorFrameSink();

  // viz::mojom::blink::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const WTF::Vector<viz::ReturnedResource>& resources) final {
    NOTIMPLEMENTED();
  }
  void DidPresentCompositorFrame(uint32_t presentation_token,
                                 mojo_base::mojom::blink::TimeTicksPtr,
                                 WTF::TimeDelta refresh,
                                 uint32_t flags) final {
    NOTIMPLEMENTED();
  }
  void DidDiscardCompositorFrame(uint32_t presentation_token) final {
    NOTIMPLEMENTED();
  }
  void OnBeginFrame(const viz::BeginFrameArgs&) final;
  void OnBeginFramePausedChanged(bool paused) final { NOTIMPLEMENTED(); }
  void ReclaimResources(
      const WTF::Vector<viz::ReturnedResource>& resources) final {
    NOTIMPLEMENTED();
  }

  // viz::mojom::blink::OffscreenCanvasSurfaceClient implementation.
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) final {
    NOTIMPLEMENTED();
  }

  ~BeginFrameProvider() = default;

 private:
  mojo::Binding<viz::mojom::blink::CompositorFrameSinkClient> cfs_binding_;
  mojo::Binding<mojom::blink::OffscreenCanvasSurfaceClient> ocs_binding_;
  const viz::FrameSinkId frame_sink_id_;
  const viz::FrameSinkId parent_frame_sink_id_;
  viz::mojom::blink::CompositorFrameSinkPtr compositor_frame_sink_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BEGIN_FRAME_PROVIDER_H_
