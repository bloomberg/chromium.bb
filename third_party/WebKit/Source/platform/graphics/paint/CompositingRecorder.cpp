// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/CompositingRecorder.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

CompositingRecorder::CompositingRecorder(GraphicsContext& graphics_context,
                                         const DisplayItemClient& client,
                                         const SkBlendMode xfer_mode,
                                         const float opacity,
                                         const FloatRect* bounds,
                                         ColorFilter color_filter)
    : client_(client), graphics_context_(graphics_context) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  graphics_context.GetPaintController()
      .CreateAndAppend<BeginCompositingDisplayItem>(client_, xfer_mode, opacity,
                                                    bounds, color_filter);
}

CompositingRecorder::~CompositingRecorder() {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return;
  graphics_context_.GetPaintController().EndItem<EndCompositingDisplayItem>(
      client_);
}

}  // namespace blink
