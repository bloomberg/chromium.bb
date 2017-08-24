// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollHitTestDisplayItem_h
#define ScrollHitTestDisplayItem_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"

namespace blink {

class GraphicsContext;

// Placeholder display item for creating a special cc::Layer marked as being
// scrollable in PaintArtifactCompositor. A display item is needed because
// scroll hit testing must be in paint order.
//
// The scroll hit test display item keeps track of the scroll offset translation
// node which also has a reference to the associated scroll node. The scroll hit
// test display item should be in the non-scrolled transform space and therefore
// should not be scrolled by the associated scroll offset transform.
class PLATFORM_EXPORT ScrollHitTestDisplayItem final : public DisplayItem {
 public:
  ScrollHitTestDisplayItem(
      const DisplayItemClient&,
      Type,
      PassRefPtr<const TransformPaintPropertyNode> scroll_offset_node);
  ~ScrollHitTestDisplayItem();

  const TransformPaintPropertyNode& scroll_offset_node() const {
    return *scroll_offset_node_;
  }

  // DisplayItem
  void Replay(GraphicsContext&) const override;
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const override;
  bool Equals(const DisplayItem&) const override;
#ifndef NDEBUG
  void DumpPropertiesAsDebugString(StringBuilder&) const override;
#endif

  // Create and append a ScrollHitTestDisplayItem onto the context. This is
  // similar to a recorder class (e.g., DrawingRecorder) but just emits a single
  // item.
  static void Record(
      GraphicsContext&,
      const DisplayItemClient&,
      DisplayItem::Type,
      PassRefPtr<const TransformPaintPropertyNode> scroll_offset_node);

 private:
  const ScrollPaintPropertyNode& scroll_node() const {
    return *scroll_offset_node_->ScrollNode();
  }

  RefPtr<const TransformPaintPropertyNode> scroll_offset_node_;
};

}  // namespace blink

#endif  // ScrollHitTestDisplayItem_h
