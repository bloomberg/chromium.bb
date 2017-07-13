// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/SurfaceLayerBridge.h"

#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/surfaces/sequence_surface_reference_factory.h"
#include "cc/surfaces/surface_sequence.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/mojo/MojoHelper.h"
#include "platform/wtf/Functional.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {
class SequenceSurfaceReferenceFactoryImpl
    : public cc::SequenceSurfaceReferenceFactory {
 public:
  SequenceSurfaceReferenceFactoryImpl(base::WeakPtr<SurfaceLayerBridge> bridge)
      : bridge_(bridge) {}

 private:
  ~SequenceSurfaceReferenceFactoryImpl() override = default;

  // cc::SequenceSurfaceReferenceFactory implementation:
  void RequireSequence(const viz::SurfaceId& id,
                       const cc::SurfaceSequence& sequence) const override {
    DCHECK(bridge_);
    bridge_->RequireCallback(id, sequence);
  }

  void SatisfySequence(const cc::SurfaceSequence& sequence) const override {
    if (bridge_)
      bridge_->SatisfyCallback(sequence);
  }

  base::WeakPtr<SurfaceLayerBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(SequenceSurfaceReferenceFactoryImpl);
};

}  // namespace

SurfaceLayerBridge::SurfaceLayerBridge(SurfaceLayerBridgeObserver* observer,
                                       WebLayerTreeView* layer_tree_view)
    : weak_factory_(this),
      observer_(observer),
      binding_(this),
      frame_sink_id_(Platform::Current()->GenerateFrameSinkId()),
      parent_frame_sink_id_(layer_tree_view ? layer_tree_view->GetFrameSinkId()
                                            : viz::FrameSinkId()) {
  ref_factory_ =
      new SequenceSurfaceReferenceFactoryImpl(weak_factory_.GetWeakPtr());

  DCHECK(!service_.is_bound());
  mojom::blink::OffscreenCanvasProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));
  // TODO(xlai): Ensure OffscreenCanvas commit() is still functional when a
  // frame-less HTML canvas's document is reparenting under another frame.
  // See crbug.com/683172.
  blink::mojom::blink::OffscreenCanvasSurfaceClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  provider->CreateOffscreenCanvasSurface(parent_frame_sink_id_, frame_sink_id_,
                                         std::move(client),
                                         mojo::MakeRequest(&service_));
}

SurfaceLayerBridge::~SurfaceLayerBridge() {
  observer_ = nullptr;
  if (web_layer_) {
    GraphicsLayer::UnregisterContentsLayer(web_layer_.get());
  }
}

void SurfaceLayerBridge::SatisfyCallback(const cc::SurfaceSequence& sequence) {
  service_->Satisfy(sequence);
}

void SurfaceLayerBridge::RequireCallback(const viz::SurfaceId& surface_id,
                                         const cc::SurfaceSequence& sequence) {
  service_->Require(surface_id, sequence);
}

void SurfaceLayerBridge::CreateSolidColorLayer() {
  cc_layer_ = cc::SolidColorLayer::Create();
  cc_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);

  web_layer_ = Platform::Current()->CompositorSupport()->CreateLayerFromCCLayer(
      cc_layer_.get());
  GraphicsLayer::RegisterContentsLayer(web_layer_.get());
}

void SurfaceLayerBridge::OnSurfaceCreated(
    const viz::SurfaceInfo& surface_info) {
  if (!current_surface_id_.is_valid() && surface_info.is_valid()) {
    // First time a SurfaceId is received
    current_surface_id_ = surface_info.id();
    GraphicsLayer::UnregisterContentsLayer(web_layer_.get());
    web_layer_->RemoveFromParent();

    scoped_refptr<cc::SurfaceLayer> surface_layer =
        cc::SurfaceLayer::Create(ref_factory_);
    surface_layer->SetPrimarySurfaceInfo(surface_info);
    surface_layer->SetFallbackSurfaceInfo(surface_info);
    surface_layer->SetStretchContentToFillBounds(true);
    cc_layer_ = surface_layer;

    web_layer_ =
        Platform::Current()->CompositorSupport()->CreateLayerFromCCLayer(
            cc_layer_.get());
    GraphicsLayer::RegisterContentsLayer(web_layer_.get());
  } else if (current_surface_id_ != surface_info.id()) {
    // A different SurfaceId is received, prompting change to existing
    // SurfaceLayer
    current_surface_id_ = surface_info.id();
    cc::SurfaceLayer* surface_layer =
        static_cast<cc::SurfaceLayer*>(cc_layer_.get());
    surface_layer->SetPrimarySurfaceInfo(surface_info);
    surface_layer->SetFallbackSurfaceInfo(surface_info);
  }

  if (observer_)
    observer_->OnWebLayerReplaced();
  cc_layer_->SetBounds(surface_info.size_in_pixels());
}

}  // namespace blink
