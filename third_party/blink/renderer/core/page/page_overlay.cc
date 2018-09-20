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

#include "third_party/blink/renderer/core/page/page_overlay.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
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
#include "third_party/blink/renderer/platform/scroll/main_thread_scrolling_reason.h"

namespace blink {

std::unique_ptr<PageOverlay> PageOverlay::Create(
    LocalFrame* local_frame,
    std::unique_ptr<PageOverlay::Delegate> delegate) {
  return base::WrapUnique(new PageOverlay(local_frame, std::move(delegate)));
}

PageOverlay::PageOverlay(LocalFrame* local_frame,
                         std::unique_ptr<PageOverlay::Delegate> delegate)
    : frame_(local_frame), delegate_(std::move(delegate)) {}

PageOverlay::~PageOverlay() {
  if (!layer_)
    return;
  layer_->RemoveFromParent();
  layer_ = nullptr;
}

void PageOverlay::Update() {
  if (!frame_)
    return;

  auto* local_root_frame_widget =
      WebLocalFrameImpl::FromFrame(frame_)->LocalRootFrameWidget();
  if (!local_root_frame_widget->IsAcceleratedCompositingActive())
    return;

  if (!layer_) {
    GraphicsLayer* parent_layer =
        frame_->IsMainFrame()
            ? frame_->GetPage()->GetVisualViewport().ContainerLayer()
            : local_root_frame_widget->RootGraphicsLayer();
    if (!parent_layer)
      return;

    layer_ = GraphicsLayer::Create(*this);
    layer_->SetDrawsContent(true);
    parent_layer->AddChild(layer_.get());

    // This is required for contents of overlay to stay in sync with the page
    // while scrolling.
    cc::Layer* cc_layer = layer_->CcLayer();
    cc_layer->AddMainThreadScrollingReasons(
        MainThreadScrollingReason::kPageOverlay);

    layer_->SetLayerState(PropertyTreeState(PropertyTreeState::Root()),
                          IntPoint());
  }

  gfx::Size size(frame_->GetPage()->GetVisualViewport().Size());
  if (size != layer_->Size())
    layer_->SetSize(size);

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    layer_->SetNeedsDisplay();
}

LayoutRect PageOverlay::VisualRect() const {
  DCHECK(layer_.get());
  return LayoutRect(IntPoint(), IntSize(layer_->Size()));
}

IntRect PageOverlay::ComputeInterestRect(const GraphicsLayer* graphics_layer,
                                         const IntRect&) const {
  return IntRect(IntPoint(), IntSize(layer_->Size()));
}

void PageOverlay::PaintContents(const GraphicsLayer* graphics_layer,
                                GraphicsContext& gc,
                                GraphicsLayerPaintingPhase phase,
                                const IntRect& interest_rect) const {
  DCHECK(layer_);
  delegate_->PaintPageOverlay(*this, gc, interest_rect.Size());
}

String PageOverlay::DebugName(const GraphicsLayer*) const {
  return "Page Overlay Content Layer";
}

}  // namespace blink
