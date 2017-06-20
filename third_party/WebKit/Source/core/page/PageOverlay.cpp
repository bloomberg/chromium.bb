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

#include "core/page/PageOverlay.h"

#include <memory>
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebFrameWidgetBase.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebLayer.h"

namespace blink {

std::unique_ptr<PageOverlay> PageOverlay::Create(
    WebLocalFrameBase* frame_impl,
    std::unique_ptr<PageOverlay::Delegate> delegate) {
  return WTF::WrapUnique(new PageOverlay(frame_impl, std::move(delegate)));
}

PageOverlay::PageOverlay(WebLocalFrameBase* frame_impl,
                         std::unique_ptr<PageOverlay::Delegate> delegate)
    : frame_impl_(frame_impl), delegate_(std::move(delegate)) {}

PageOverlay::~PageOverlay() {
  if (!layer_)
    return;
  layer_->RemoveFromParent();
  layer_ = nullptr;
}

void PageOverlay::Update() {
  if (!frame_impl_->FrameWidget()->IsAcceleratedCompositingActive())
    return;

  LocalFrame* frame = frame_impl_->GetFrame();
  if (!frame)
    return;

  if (!layer_) {
    layer_ = GraphicsLayer::Create(this);
    layer_->SetDrawsContent(true);

    // This is required for contents of overlay to stay in sync with the page
    // while scrolling.
    WebLayer* platform_layer = layer_->PlatformLayer();
    platform_layer->AddMainThreadScrollingReasons(
        MainThreadScrollingReason::kPageOverlay);
    if (frame->IsMainFrame()) {
      frame->GetPage()->GetVisualViewport().ContainerLayer()->AddChild(
          layer_.get());
    } else {
      frame_impl_->FrameWidget()->RootGraphicsLayer()->AddChild(layer_.get());
    }
  }

  FloatSize size(frame->GetPage()->GetVisualViewport().Size());
  if (size != layer_->Size())
    layer_->SetSize(size);

  layer_->SetNeedsDisplay();
}

LayoutRect PageOverlay::VisualRect() const {
  DCHECK(layer_.get());
  return LayoutRect(FloatPoint(), layer_->Size());
}

IntRect PageOverlay::ComputeInterestRect(const GraphicsLayer* graphics_layer,
                                         const IntRect&) const {
  return IntRect(IntPoint(), ExpandedIntSize(layer_->Size()));
}

void PageOverlay::PaintContents(const GraphicsLayer* graphics_layer,
                                GraphicsContext& gc,
                                GraphicsLayerPaintingPhase phase,
                                const IntRect& interest_rect) const {
  DCHECK(layer_);
  delegate_->PaintPageOverlay(*this, gc, interest_rect.Size());
}

String PageOverlay::DebugName(const GraphicsLayer*) const {
  return "WebViewImpl Page Overlay Content Layer";
}

}  // namespace blink
