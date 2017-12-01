// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FloatClipRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/FloatClipDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

FloatClipRecorder::FloatClipRecorder(GraphicsContext& context,
                                     const DisplayItemClient& client,
                                     PaintPhase paint_phase,
                                     const FloatRect& clip_rect)
    : FloatClipRecorder(context,
                        client,
                        DisplayItem::PaintPhaseToFloatClipType(paint_phase),
                        clip_rect) {}

FloatClipRecorder::FloatClipRecorder(GraphicsContext& context,
                                     const DisplayItemClient& client,
                                     DisplayItem::Type clip_type,
                                     const FloatRect& clip_rect)
    : context_(context), client_(client), clip_type_(clip_type) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  context_.GetPaintController().CreateAndAppend<FloatClipDisplayItem>(
      client_, clip_type_, clip_rect);
}

FloatClipRecorder::~FloatClipRecorder() {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  DisplayItem::Type end_type =
      DisplayItem::FloatClipTypeToEndFloatClipType(clip_type_);
  context_.GetPaintController().EndItem<EndFloatClipDisplayItem>(client_,
                                                                 end_type);
}

}  // namespace blink
