// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/SurfaceLayerBridge.h"

#include "base/feature_list.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "media/base/media_switches.h"
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

SurfaceLayerBridge::SurfaceLayerBridge(WebLayerTreeView* layer_tree_view,
                                       WebSurfaceLayerBridgeObserver* observer)
    : observer_(observer),
      binding_(this),
      frame_sink_id_(Platform::Current()->GenerateFrameSinkId()),
      parent_frame_sink_id_(layer_tree_view ? layer_tree_view->GetFrameSinkId()
                                            : viz::FrameSinkId()) {
  mojom::blink::OffscreenCanvasProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));
  // TODO(xlai): Ensure OffscreenCanvas commit() is still functional when a
  // frame-less HTML canvas's document is reparenting under another frame.
  // See crbug.com/683172.
  blink::mojom::blink::OffscreenCanvasSurfaceClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  provider->CreateOffscreenCanvasSurface(parent_frame_sink_id_, frame_sink_id_,
                                         std::move(client));
}

SurfaceLayerBridge::~SurfaceLayerBridge() {
  observer_ = nullptr;
}

void SurfaceLayerBridge::CreateSolidColorLayer() {
  cc_layer_ = cc::SolidColorLayer::Create();
  cc_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);

  web_layer_ = Platform::Current()->CompositorSupport()->CreateLayerFromCCLayer(
      cc_layer_.get());

  if (observer_)
    observer_->RegisterContentsLayer(web_layer_.get());
}

void SurfaceLayerBridge::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  if (!current_surface_id_.is_valid() && surface_info.is_valid()) {
    // First time a SurfaceId is received.
    current_surface_id_ = surface_info.id();
    if (web_layer_) {
      if (observer_)
        observer_->UnregisterContentsLayer(web_layer_.get());
      web_layer_->RemoveFromParent();
    }

    scoped_refptr<cc::SurfaceLayer> surface_layer = cc::SurfaceLayer::Create();
    surface_layer->SetPrimarySurfaceId(
        surface_info.id(), cc::DeadlinePolicy::UseDefaultDeadline());
    surface_layer->SetFallbackSurfaceId(surface_info.id());
    surface_layer->SetStretchContentToFillBounds(true);
    surface_layer->SetIsDrawable(true);
    cc_layer_ = surface_layer;

    web_layer_ =
        Platform::Current()->CompositorSupport()->CreateLayerFromCCLayer(
            cc_layer_.get());
    if (observer_)
      observer_->RegisterContentsLayer(web_layer_.get());
  } else if (current_surface_id_ != surface_info.id()) {
    // A different SurfaceId is received, prompting change to existing
    // SurfaceLayer.
    current_surface_id_ = surface_info.id();
    cc::SurfaceLayer* surface_layer =
        static_cast<cc::SurfaceLayer*>(cc_layer_.get());
    surface_layer->SetPrimarySurfaceId(
        surface_info.id(), cc::DeadlinePolicy::UseDefaultDeadline());
    surface_layer->SetFallbackSurfaceId(surface_info.id());
  }

  if (observer_) {
    observer_->OnWebLayerUpdated();
    observer_->OnSurfaceIdUpdated(surface_info.id());
  }

  cc_layer_->SetBounds(surface_info.size_in_pixels());
}

}  // namespace blink
