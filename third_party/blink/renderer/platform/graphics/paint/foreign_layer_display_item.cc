// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

#include <utility>

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

class ForeignLayerDisplayItemClient final : public DisplayItemClient {
 public:
  ForeignLayerDisplayItemClient(scoped_refptr<cc::Layer> layer)
      : layer_(std::move(layer)) {}

  String DebugName() const final { return "ForeignLayer"; }

  LayoutRect VisualRect() const final {
    const auto& offset = layer_->offset_to_transform_parent();
    return LayoutRect(LayoutPoint(offset.x(), offset.y()),
                      LayoutSize(IntSize(layer_->bounds())));
  }

  cc::Layer* GetLayer() const { return layer_.get(); }

 private:
  scoped_refptr<cc::Layer> layer_;
};

}  // anonymous namespace

ForeignLayerDisplayItem::ForeignLayerDisplayItem(Type type,
                                                 scoped_refptr<cc::Layer> layer)
    : DisplayItem(*new ForeignLayerDisplayItemClient(std::move(layer)),
                  type,
                  sizeof(*this)) {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
         RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());
  DCHECK(IsForeignLayerType(type));
  DCHECK(GetLayer());
}

ForeignLayerDisplayItem::~ForeignLayerDisplayItem() {
  delete &Client();
}

cc::Layer* ForeignLayerDisplayItem::GetLayer() const {
  return static_cast<const ForeignLayerDisplayItemClient&>(Client()).GetLayer();
}

void ForeignLayerDisplayItem::Replay(GraphicsContext&) const {
  NOTREACHED();
}

void ForeignLayerDisplayItem::AppendToDisplayItemList(
    const FloatSize&,
    cc::DisplayItemList&) const {
  NOTREACHED();
}

bool ForeignLayerDisplayItem::DrawsContent() const {
  return false;
}

bool ForeignLayerDisplayItem::Equals(const DisplayItem& other) const {
  return DisplayItem::Equals(other) &&
         GetLayer() ==
             static_cast<const ForeignLayerDisplayItem&>(other).GetLayer();
}

#if DCHECK_IS_ON()
void ForeignLayerDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetInteger("layer", GetLayer()->id());
}
#endif

void RecordForeignLayer(GraphicsContext& context,
                        DisplayItem::Type type,
                        scoped_refptr<cc::Layer> layer) {
  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  paint_controller.CreateAndAppend<ForeignLayerDisplayItem>(type,
                                                            std::move(layer));
}

}  // namespace blink
