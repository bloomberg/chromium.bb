// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutBoxItem_h
#define LayoutBoxItem_h

#include "core/layout/LayoutBox.h"
#include "core/layout/api/LayoutBoxModel.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LayoutPoint;
class LayoutSize;
class LayoutUnit;

class LayoutBoxItem : public LayoutBoxModel {
 public:
  explicit LayoutBoxItem(LayoutBox* layout_box) : LayoutBoxModel(layout_box) {}

  explicit LayoutBoxItem(const LayoutItem& item) : LayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.IsBox());
  }

  explicit LayoutBoxItem(std::nullptr_t) : LayoutBoxModel(nullptr) {}

  LayoutBoxItem() {}

  LayoutBoxItem EnclosingBox() const {
    return LayoutBoxItem(ToBox()->EnclosingBox());
  }

  ScrollResult Scroll(ScrollGranularity granularity, const FloatSize& delta) {
    return ToBox()->Scroll(granularity, delta);
  }

  LayoutSize Size() const { return ToBox()->Size(); }

  LayoutPoint Location() const { return ToBox()->Location(); }

  LayoutUnit LogicalWidth() const { return ToBox()->LogicalWidth(); }

  LayoutUnit LogicalHeight() const { return ToBox()->LogicalHeight(); }

  LayoutUnit MinPreferredLogicalWidth() const {
    return ToBox()->MinPreferredLogicalWidth();
  }

  LayoutRect OverflowClipRect(const LayoutPoint& location,
                              OverlayScrollbarClipBehavior behavior =
                                  kIgnorePlatformOverlayScrollbarSize) const {
    return ToBox()->OverflowClipRect(location, behavior);
  }

  LayoutSize ContentBoxOffset() const { return ToBox()->ContentBoxOffset(); }

  void MapLocalToAncestor(
      const LayoutBoxModelObject* ancestor,
      TransformState& state,
      MapCoordinatesFlags flags = kApplyContainerFlip) const {
    ToBox()->MapLocalToAncestor(ancestor, state, flags);
  }

  FloatQuad AbsoluteContentQuad(MapCoordinatesFlags flags = 0) const {
    return ToBox()->AbsoluteContentQuad(flags);
  }

 private:
  LayoutBox* ToBox() { return ToLayoutBox(GetLayoutObject()); }

  const LayoutBox* ToBox() const { return ToLayoutBox(GetLayoutObject()); }
};

}  // namespace blink

#endif  // LayoutBoxItem_h
