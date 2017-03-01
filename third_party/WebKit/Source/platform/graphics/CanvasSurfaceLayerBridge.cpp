// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridge.h"

#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/surfaces/sequence_surface_reference_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_sequence.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "ui/gfx/geometry/size.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

class OffscreenCanvasSurfaceReferenceFactory
    : public cc::SequenceSurfaceReferenceFactory {
 public:
  OffscreenCanvasSurfaceReferenceFactory(
      base::WeakPtr<CanvasSurfaceLayerBridge> bridge)
      : m_bridge(bridge) {}

 private:
  ~OffscreenCanvasSurfaceReferenceFactory() override = default;

  // cc::SequenceSurfaceReferenceFactory implementation:
  void RequireSequence(const cc::SurfaceId& id,
                       const cc::SurfaceSequence& sequence) const override {
    DCHECK(m_bridge);
    m_bridge->requireCallback(id, sequence);
  }

  void SatisfySequence(const cc::SurfaceSequence& sequence) const override {
    DCHECK(m_bridge);
    m_bridge->satisfyCallback(sequence);
  }

  base::WeakPtr<CanvasSurfaceLayerBridge> m_bridge;

  DISALLOW_COPY_AND_ASSIGN(OffscreenCanvasSurfaceReferenceFactory);
};

}  // namespace

CanvasSurfaceLayerBridge::CanvasSurfaceLayerBridge(
    CanvasSurfaceLayerBridgeObserver* observer,
    WebLayerTreeView* layerTreeView)
    : m_weakFactory(this),
      m_observer(observer),
      m_binding(this),
      m_frameSinkId(Platform::current()->generateFrameSinkId()),
      m_parentFrameSinkId(layerTreeView->getFrameSinkId()) {
  m_refFactory =
      new OffscreenCanvasSurfaceReferenceFactory(m_weakFactory.GetWeakPtr());

  DCHECK(!m_service.is_bound());
  mojom::blink::OffscreenCanvasSurfaceFactoryPtr serviceFactory;
  Platform::current()->interfaceProvider()->getInterface(
      mojo::MakeRequest(&serviceFactory));
  // TODO(xlai): Ensure OffscreenCanvas commit() is still functional when a
  // frame-less HTML canvas's document is reparenting under another frame.
  // See crbug.com/683172.
  serviceFactory->CreateOffscreenCanvasSurface(
      m_parentFrameSinkId, m_frameSinkId, m_binding.CreateInterfacePtrAndBind(),
      mojo::MakeRequest(&m_service));
}

CanvasSurfaceLayerBridge::~CanvasSurfaceLayerBridge() {
  m_observer = nullptr;
}

void CanvasSurfaceLayerBridge::createSolidColorLayer() {
  m_CCLayer = cc::SolidColorLayer::Create();
  m_CCLayer->SetBackgroundColor(SK_ColorTRANSPARENT);
  m_webLayer = Platform::current()->compositorSupport()->createLayerFromCCLayer(
      m_CCLayer.get());
  GraphicsLayer::registerContentsLayer(m_webLayer.get());
}

void CanvasSurfaceLayerBridge::OnSurfaceCreated(
    const cc::SurfaceInfo& surfaceInfo) {
  if (!m_currentSurfaceId.is_valid() && surfaceInfo.is_valid()) {
    // First time a SurfaceId is received
    m_currentSurfaceId = surfaceInfo.id();
    GraphicsLayer::unregisterContentsLayer(m_webLayer.get());
    m_webLayer->removeFromParent();

    scoped_refptr<cc::SurfaceLayer> surfaceLayer =
        cc::SurfaceLayer::Create(m_refFactory);
    surfaceLayer->SetPrimarySurfaceInfo(surfaceInfo);
    surfaceLayer->SetStretchContentToFillBounds(true);
    m_CCLayer = surfaceLayer;

    m_webLayer =
        Platform::current()->compositorSupport()->createLayerFromCCLayer(
            m_CCLayer.get());
    GraphicsLayer::registerContentsLayer(m_webLayer.get());
  } else if (m_currentSurfaceId != surfaceInfo.id()) {
    // A different SurfaceId is received, prompting change to existing
    // SurfaceLayer
    m_currentSurfaceId = surfaceInfo.id();
    cc::SurfaceLayer* surfaceLayer =
        static_cast<cc::SurfaceLayer*>(m_CCLayer.get());
    surfaceLayer->SetPrimarySurfaceInfo(surfaceInfo);
  }

  m_observer->OnWebLayerReplaced();
  m_CCLayer->SetBounds(surfaceInfo.size_in_pixels());
}

void CanvasSurfaceLayerBridge::satisfyCallback(
    const cc::SurfaceSequence& sequence) {
  m_service->Satisfy(sequence);
}

void CanvasSurfaceLayerBridge::requireCallback(
    const cc::SurfaceId& surfaceId,
    const cc::SurfaceSequence& sequence) {
  m_service->Require(surfaceId, sequence);
}

}  // namespace blink
