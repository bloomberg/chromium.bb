// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasSurfaceLayerBridge_h
#define CanvasSurfaceLayerBridge_h

#include <memory>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cc/ipc/frame_sink_manager.mojom-blink.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/PlatformExport.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"

namespace cc {
class Layer;
class SurfaceInfo;
}  // namespace cc

namespace blink {

class WebLayer;
class WebLayerTreeView;

class PLATFORM_EXPORT CanvasSurfaceLayerBridgeObserver {
 public:
  CanvasSurfaceLayerBridgeObserver() {}
  virtual ~CanvasSurfaceLayerBridgeObserver() {}

  virtual void OnWebLayerReplaced() = 0;
};

class PLATFORM_EXPORT CanvasSurfaceLayerBridge
    : NON_EXPORTED_BASE(public cc::mojom::blink::FrameSinkManagerClient) {
 public:
  explicit CanvasSurfaceLayerBridge(CanvasSurfaceLayerBridgeObserver*,
                                    WebLayerTreeView*);
  ~CanvasSurfaceLayerBridge();
  void CreateSolidColorLayer();
  WebLayer* GetWebLayer() const { return web_layer_.get(); }
  const cc::FrameSinkId& GetFrameSinkId() const { return frame_sink_id_; }

  // Implementation of cc::mojom::blink::FrameSinkManagerClient
  void OnSurfaceCreated(const cc::SurfaceInfo&) override;

  void SatisfyCallback(const cc::SurfaceSequence&);
  void RequireCallback(const cc::SurfaceId&, const cc::SurfaceSequence&);

 private:
  scoped_refptr<cc::Layer> cc_layer_;
  std::unique_ptr<WebLayer> web_layer_;

  scoped_refptr<cc::SurfaceReferenceFactory> ref_factory_;
  base::WeakPtrFactory<CanvasSurfaceLayerBridge> weak_factory_;

  CanvasSurfaceLayerBridgeObserver* observer_;

  mojom::blink::OffscreenCanvasSurfacePtr service_;
  mojo::Binding<cc::mojom::blink::FrameSinkManagerClient> binding_;

  const cc::FrameSinkId frame_sink_id_;
  cc::SurfaceId current_surface_id_;
  const cc::FrameSinkId parent_frame_sink_id_;
};

}  // namespace blink

#endif  // CanvasSurfaceLayerBridge_h
