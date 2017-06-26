/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "platform/graphics/ContentLayerDelegate.h"

#include "platform/bindings/RuntimeCallStats.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebRect.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

ContentLayerDelegate::ContentLayerDelegate(GraphicsLayer* graphics_layer)
    : graphics_layer_(graphics_layer) {}

ContentLayerDelegate::~ContentLayerDelegate() {}

gfx::Rect ContentLayerDelegate::PaintableRegion() {
  IntRect interest_rect = graphics_layer_->InterestRect();
  return gfx::Rect(interest_rect.X(), interest_rect.Y(), interest_rect.Width(),
                   interest_rect.Height());
}

void ContentLayerDelegate::PaintContents(
    WebDisplayItemList* web_display_item_list,
    WebContentLayerClient::PaintingControlSetting painting_control) {
  TRACE_EVENT0("blink,benchmark", "ContentLayerDelegate::paintContents");
  RUNTIME_CALL_TIMER_SCOPE(V8PerIsolateData::MainThreadIsolate(),
                           RuntimeCallStats::CounterId::kPaintContents);

  PaintController& paint_controller = graphics_layer_->GetPaintController();
  paint_controller.SetDisplayItemConstructionIsDisabled(
      painting_control ==
      WebContentLayerClient::kDisplayListConstructionDisabled);
  paint_controller.SetSubsequenceCachingIsDisabled(
      painting_control == WebContentLayerClient::kSubsequenceCachingDisabled);

  if (painting_control == WebContentLayerClient::kPartialInvalidation)
    graphics_layer_->Client()->InvalidateTargetElementForTesting();

  // We also disable caching when Painting or Construction are disabled. In both
  // cases we would like to compare assuming the full cost of recording, not the
  // cost of re-using cached content.
  if (painting_control == WebContentLayerClient::kDisplayListCachingDisabled ||
      painting_control == WebContentLayerClient::kDisplayListPaintingDisabled ||
      painting_control ==
          WebContentLayerClient::kDisplayListConstructionDisabled)
    paint_controller.InvalidateAll();

  GraphicsContext::DisabledMode disabled_mode =
      GraphicsContext::kNothingDisabled;
  if (painting_control == WebContentLayerClient::kDisplayListPaintingDisabled ||
      painting_control ==
          WebContentLayerClient::kDisplayListConstructionDisabled)
    disabled_mode = GraphicsContext::kFullyDisabled;

  // Anything other than PaintDefaultBehavior is for testing. In non-testing
  // scenarios, it is an error to call GraphicsLayer::paint. Actual painting
  // occurs in FrameView::paintTree(); this method merely copies the painted
  // output to the WebDisplayItemList.
  if (painting_control != kPaintDefaultBehavior)
    graphics_layer_->Paint(nullptr, disabled_mode);

  paint_controller.GetPaintArtifact().AppendToWebDisplayItemList(
      graphics_layer_->OffsetFromLayoutObjectWithSubpixelAccumulation(),
      web_display_item_list);

  paint_controller.SetDisplayItemConstructionIsDisabled(false);
  paint_controller.SetSubsequenceCachingIsDisabled(false);
}

size_t ContentLayerDelegate::ApproximateUnsharedMemoryUsage() const {
  return graphics_layer_->GetPaintController().ApproximateUnsharedMemoryUsage();
}

}  // namespace blink
