// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasSurfaceLayerBridge_h
#define CanvasSurfaceLayerBridge_h

#include <memory>
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cc/ipc/display_compositor.mojom-blink.h"
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
    : NON_EXPORTED_BASE(public cc::mojom::blink::DisplayCompositorClient) {
 public:
  explicit CanvasSurfaceLayerBridge(CanvasSurfaceLayerBridgeObserver*,
                                    WebLayerTreeView*);
  ~CanvasSurfaceLayerBridge();
  void createSolidColorLayer();
  WebLayer* getWebLayer() const { return m_webLayer.get(); }
  const cc::FrameSinkId& getFrameSinkId() const { return m_frameSinkId; }

  // Implementation of cc::mojom::blink::DisplayCompositorClient
  void OnSurfaceCreated(const cc::SurfaceInfo&) override;

  void satisfyCallback(const cc::SurfaceSequence&);
  void requireCallback(const cc::SurfaceId&, const cc::SurfaceSequence&);

 private:
  scoped_refptr<cc::Layer> m_CCLayer;
  std::unique_ptr<WebLayer> m_webLayer;

  scoped_refptr<cc::SurfaceReferenceFactory> m_refFactory;
  base::WeakPtrFactory<CanvasSurfaceLayerBridge> m_weakFactory;

  CanvasSurfaceLayerBridgeObserver* m_observer;

  mojom::blink::OffscreenCanvasSurfacePtr m_service;
  mojo::Binding<cc::mojom::blink::DisplayCompositorClient> m_binding;

  const cc::FrameSinkId m_frameSinkId;
  cc::SurfaceId m_currentSurfaceId;
  const cc::FrameSinkId m_parentFrameSinkId;
};

}  // namespace blink

#endif  // CanvasSurfaceLayerBridge_h
