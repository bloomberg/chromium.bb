// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SurfaceLayerBridge_h
#define SurfaceLayerBridge_h

#include <memory>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_reference_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/PlatformExport.h"
#include "public/platform/WebSurfaceLayerBridge.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"

namespace cc {
class Layer;
}  // namespace cc

namespace viz {
class SurfaceInfo;
}  // namespace viz

namespace blink {

class WebLayer;
class WebLayerTreeView;

class PLATFORM_EXPORT SurfaceLayerBridgeObserver {
 public:
  SurfaceLayerBridgeObserver() {}
  virtual ~SurfaceLayerBridgeObserver() {}

  virtual void OnWebLayerReplaced() = 0;
};

class PLATFORM_EXPORT SurfaceLayerBridge
    : public NON_EXPORTED_BASE(
          blink::mojom::blink::OffscreenCanvasSurfaceClient),
      public WebSurfaceLayerBridge {
 public:
  SurfaceLayerBridge(SurfaceLayerBridgeObserver*, WebLayerTreeView*);
  virtual ~SurfaceLayerBridge();

  void CreateSolidColorLayer();

  // Implementation of blink::mojom::blink::OffscreenCanvasSurfaceClient
  void OnFirstSurfaceActivation(const viz::SurfaceInfo&) override;
  void SatisfyCallback(const viz::SurfaceSequence&);
  void RequireCallback(const viz::SurfaceId&, const viz::SurfaceSequence&);

  // Implementation of WebSurfaceLayerBridge.
  WebLayer* GetWebLayer() const override { return web_layer_.get(); }

  const viz::FrameSinkId& GetFrameSinkId() const { return frame_sink_id_; }

 private:
  mojom::blink::OffscreenCanvasSurfacePtr service_;

  scoped_refptr<cc::Layer> cc_layer_;
  std::unique_ptr<WebLayer> web_layer_;

  scoped_refptr<viz::SurfaceReferenceFactory> ref_factory_;
  base::WeakPtrFactory<SurfaceLayerBridge> weak_factory_;

  SurfaceLayerBridgeObserver* observer_;

  mojo::Binding<blink::mojom::blink::OffscreenCanvasSurfaceClient> binding_;

  const viz::FrameSinkId frame_sink_id_;
  viz::SurfaceId current_surface_id_;
  const viz::FrameSinkId parent_frame_sink_id_;
};

}  // namespace blink

#endif  // SurfaceLayerBridge_h
