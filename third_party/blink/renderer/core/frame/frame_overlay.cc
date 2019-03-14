/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/frame_overlay.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

FrameOverlay::FrameOverlay(LocalFrame* local_frame,
                           std::unique_ptr<FrameOverlay::Delegate> delegate)
    : frame_(local_frame), delegate_(std::move(delegate)) {
  DCHECK(frame_);
}

FrameOverlay::~FrameOverlay() {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  if (!layer_)
    return;
  layer_->RemoveFromParent();
  layer_ = nullptr;
}

void FrameOverlay::Update() {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    delegate_->Invalidate();
    return;
  }

  auto* local_root_frame_widget =
      WebLocalFrameImpl::FromFrame(frame_)->LocalRootFrameWidget();
  if (!local_root_frame_widget->IsAcceleratedCompositingActive())
    return;

  GraphicsLayer* parent_layer =
      frame_->IsMainFrame()
          ? frame_->GetPage()->GetVisualViewport().ContainerLayer()
          : local_root_frame_widget->RootGraphicsLayer();
  if (!layer_) {
    if (!parent_layer)
      return;
    layer_ = GraphicsLayer::Create(*this);
    layer_->SetDrawsContent(true);

    if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
      // This is required for contents of overlay to stay in sync with the page
      // while scrolling. When BlinkGenPropertyTrees is enabled, scrolling is
      // prevented by using the root scroll node in the root property tree
      // state.
      cc::Layer* cc_layer = layer_->CcLayer();
      cc_layer->AddMainThreadScrollingReasons(
          cc::MainThreadScrollingReason::kFrameOverlay);
    }

    layer_->SetLayerState(PropertyTreeState::Root(), IntPoint());
  }

  layer_->SetSize(gfx::Size(Size()));
  layer_->SetNeedsDisplay();
  if (parent_layer)
    parent_layer->SetPaintArtifactCompositorNeedsUpdate();
}

IntSize FrameOverlay::Size() const {
  if (frame_->IsMainFrame())
    return frame_->GetPage()->GetVisualViewport().Size();
  return frame_->GetPage()->GetVisualViewport().Size().ExpandedTo(
      frame_->View()->Size());
}

LayoutRect FrameOverlay::VisualRect() const {
  return LayoutRect(IntPoint(), Size());
}

IntRect FrameOverlay::ComputeInterestRect(const GraphicsLayer* graphics_layer,
                                          const IntRect&) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  return IntRect(IntPoint(), Size());
}

void FrameOverlay::EnsureOverlayAttached() const {
  DCHECK(layer_);
  auto* local_root_frame_widget =
      WebLocalFrameImpl::FromFrame(frame_)->LocalRootFrameWidget();
  GraphicsLayer* parent_layer =
      frame_->GetPage()->MainFrame()->IsLocalFrame()
          ? frame_->GetPage()->GetVisualViewport().ContainerLayer()
          : local_root_frame_widget->RootGraphicsLayer();
  DCHECK(parent_layer);
  if (layer_->Parent() != parent_layer)
    parent_layer->AddChild(layer_.get());
}

void FrameOverlay::PaintContents(const GraphicsLayer* graphics_layer,
                                 GraphicsContext& gc,
                                 GraphicsLayerPaintingPhase phase,
                                 const IntRect& interest_rect) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(layer_);
  EnsureOverlayAttached();
  delegate_->PaintFrameOverlay(*this, gc, Size());
}

String FrameOverlay::DebugName(const GraphicsLayer*) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  return "Frame Overlay Content Layer";
}

void FrameOverlay::Paint(GraphicsContext& context) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  delegate_->PaintFrameOverlay(*this, context, Size());
}

}  // namespace blink
