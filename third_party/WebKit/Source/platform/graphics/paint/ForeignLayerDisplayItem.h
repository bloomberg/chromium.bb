// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForeignLayerDisplayItem_h
#define ForeignLayerDisplayItem_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace cc {
class Layer;
}

namespace blink {

class GraphicsContext;
class WebLayer;

// Represents foreign content (produced outside Blink) which draws to a layer.
// A client supplies a layer which can be unwrapped and inserted into the full
// layer tree.
//
// Before SPv2, this content is not painted, but is instead inserted into the
// GraphicsLayer tree.
class PLATFORM_EXPORT ForeignLayerDisplayItem final : public DisplayItem {
 public:
  ForeignLayerDisplayItem(const DisplayItemClient&,
                          Type,
                          scoped_refptr<cc::Layer>,
                          const FloatPoint& location,
                          const IntSize& bounds);
  ~ForeignLayerDisplayItem();

  cc::Layer* GetLayer() const { return layer_.get(); }
  const FloatPoint& Location() const { return location_; }
  const IntSize& Bounds() const { return bounds_; }

  // DisplayItem
  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;
  bool DrawsContent() const override;
  bool Equals(const DisplayItem&) const override;
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif

 private:
  scoped_refptr<cc::Layer> layer_;
  FloatPoint location_;
  IntSize bounds_;
};

// Records a foreign layer into a GraphicsContext.
// Use this where you would use a recorder class.
PLATFORM_EXPORT void RecordForeignLayer(GraphicsContext&,
                                        const DisplayItemClient&,
                                        DisplayItem::Type,
                                        WebLayer*,
                                        const FloatPoint& location,
                                        const IntSize& bounds);

}  // namespace blink

#endif  // ForeignLayerDisplayItem_h
