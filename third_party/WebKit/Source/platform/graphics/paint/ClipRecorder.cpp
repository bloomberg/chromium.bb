// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

ClipRecorder::ClipRecorder(GraphicsContext& context,
                           const DisplayItemClient& client,
                           DisplayItem::Type type,
                           const IntRect& clip_rect)
    : client_(client), context_(context), type_(type) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  context_.GetPaintController().CreateAndAppend<ClipDisplayItem>(client_, type,
                                                                 clip_rect);
}

ClipRecorder::~ClipRecorder() {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  context_.GetPaintController().EndItem<EndClipDisplayItem>(
      client_, DisplayItem::ClipTypeToEndClipType(type_));
}

}  // namespace blink
