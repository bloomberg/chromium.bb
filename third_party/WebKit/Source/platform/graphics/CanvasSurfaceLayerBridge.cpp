// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasSurfaceLayerBridge.h"

#include "cc/layers/surface_layer.h"
#include "cc/surfaces/sequence_surface_reference_factory.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_info.h"
#include "cc/surfaces/surface_sequence.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
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
    mojom::blink::OffscreenCanvasSurfacePtr service)
    : m_service(std::move(service)), m_weakFactory(this) {
  m_refFactory =
      new OffscreenCanvasSurfaceReferenceFactory(m_weakFactory.GetWeakPtr());
}

CanvasSurfaceLayerBridge::~CanvasSurfaceLayerBridge() {}

bool CanvasSurfaceLayerBridge::createSurfaceLayer(int canvasWidth,
                                                  int canvasHeight) {
  if (!m_service->GetSurfaceId(&m_surfaceId))
    return false;

  m_surfaceLayer = cc::SurfaceLayer::Create(m_refFactory);
  cc::SurfaceInfo info(m_surfaceId, 1.f, gfx::Size(canvasWidth, canvasHeight));
  m_surfaceLayer->SetSurfaceInfo(
      info, true /* scale layer bounds with surface size */);

  m_webLayer = Platform::current()->compositorSupport()->createLayerFromCCLayer(
      m_surfaceLayer.get());
  GraphicsLayer::registerContentsLayer(m_webLayer.get());
  return true;
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
