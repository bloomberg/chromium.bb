// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ForeignLayerDisplayItem.h"

#include <utility>
#include "cc/layers/layer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/WebLayer.h"

namespace blink {

ForeignLayerDisplayItem::ForeignLayerDisplayItem(
    const DisplayItemClient& client,
    Type type,
    scoped_refptr<cc::Layer> layer,
    const FloatPoint& location,
    const IntSize& bounds)
    : DisplayItem(client, type, sizeof(*this)),
      layer_(std::move(layer)),
      location_(location),
      bounds_(bounds) {
  DCHECK(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
  DCHECK(IsForeignLayerType(type));
  DCHECK(layer_);
}

ForeignLayerDisplayItem::~ForeignLayerDisplayItem() {}

void ForeignLayerDisplayItem::Replay(GraphicsContext&) const {
  NOTREACHED();
}

void ForeignLayerDisplayItem::AppendToWebDisplayItemList(
    const IntRect&,
    WebDisplayItemList*) const {
  NOTREACHED();
}

bool ForeignLayerDisplayItem::DrawsContent() const {
  return true;
}

bool ForeignLayerDisplayItem::Equals(const DisplayItem& other) const {
  return DisplayItem::Equals(other) &&
         layer_ == static_cast<const ForeignLayerDisplayItem&>(other).layer_;
}

#ifndef NDEBUG
void ForeignLayerDisplayItem::DumpPropertiesAsDebugString(
    StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(String::Format(", layer: %d", layer_->id()));
}
#endif  // NDEBUG

void RecordForeignLayer(GraphicsContext& context,
                        const DisplayItemClient& client,
                        DisplayItem::Type type,
                        WebLayer* web_layer,
                        const FloatPoint& location,
                        const IntSize& bounds) {
  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  paint_controller.CreateAndAppend<ForeignLayerDisplayItem>(
      client, type, web_layer->CcLayer(), location, bounds);
}

}  // namespace blink
