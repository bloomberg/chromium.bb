// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SURFACE_LAYER_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SURFACE_LAYER_BRIDGE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class Layer;
}  // namespace cc

namespace viz {
class SurfaceInfo;
}  // namespace viz

namespace blink {

class WebLayer;
class WebLayerTreeView;

// The SurfaceLayerBridge facilitates communication about changes to a Surface
// between the Render and Browser processes.
class PLATFORM_EXPORT SurfaceLayerBridge
    : public blink::mojom::blink::OffscreenCanvasSurfaceClient,
      public WebSurfaceLayerBridge {
 public:
  SurfaceLayerBridge(WebLayerTreeView*, WebSurfaceLayerBridgeObserver*);
  virtual ~SurfaceLayerBridge();

  void CreateSolidColorLayer();

  // Implementation of blink::mojom::blink::OffscreenCanvasSurfaceClient
  void OnFirstSurfaceActivation(const viz::SurfaceInfo&) override;

  // Implementation of WebSurfaceLayerBridge.
  WebLayer* GetWebLayer() const override { return web_layer_.get(); }

  const viz::FrameSinkId& GetFrameSinkId() const override {
    return frame_sink_id_;
  }

 private:
  scoped_refptr<cc::Layer> cc_layer_;
  std::unique_ptr<WebLayer> web_layer_;

  WebSurfaceLayerBridgeObserver* observer_;

  mojo::Binding<blink::mojom::blink::OffscreenCanvasSurfaceClient> binding_;

  const viz::FrameSinkId frame_sink_id_;
  viz::SurfaceId current_surface_id_;
  const viz::FrameSinkId parent_frame_sink_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SURFACE_LAYER_BRIDGE_H_
