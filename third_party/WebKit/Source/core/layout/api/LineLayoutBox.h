// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBox_h
#define LineLayoutBox_h

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutBox;

class LineLayoutBox : public LineLayoutBoxModel {
 public:
  explicit LineLayoutBox(LayoutBox* layout_box)
      : LineLayoutBoxModel(layout_box) {}

  explicit LineLayoutBox(const LineLayoutItem& item)
      : LineLayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.IsBox());
  }

  explicit LineLayoutBox(std::nullptr_t) : LineLayoutBoxModel(nullptr) {}

  LineLayoutBox() {}

  LayoutPoint Location() const { return ToBox()->Location(); }

  LayoutSize Size() const { return ToBox()->Size(); }

  void SetLogicalHeight(LayoutUnit size) { ToBox()->SetLogicalHeight(size); }

  LayoutUnit LogicalHeight() const { return ToBox()->LogicalHeight(); }

  LayoutUnit LogicalTop() const { return ToBox()->LogicalTop(); }

  LayoutUnit LogicalBottom() const { return ToBox()->LogicalBottom(); }

  LayoutUnit FlipForWritingMode(LayoutUnit unit) const {
    return ToBox()->FlipForWritingMode(unit);
  }

  void FlipForWritingMode(FloatRect& rect) const {
    ToBox()->FlipForWritingMode(rect);
  }

  FloatPoint FlipForWritingMode(const FloatPoint& point) const {
    return ToBox()->FlipForWritingMode(point);
  }

  void FlipForWritingMode(LayoutRect& rect) const {
    ToBox()->FlipForWritingMode(rect);
  }

  LayoutPoint FlipForWritingMode(const LayoutPoint& point) const {
    return ToBox()->FlipForWritingMode(point);
  }

  LayoutPoint FlipForWritingModeForChild(const LineLayoutBox& child,
                                         LayoutPoint child_point) const {
    return ToBox()->FlipForWritingModeForChild(
        ToLayoutBox(child.GetLayoutObject()), child_point);
  }

  void MoveWithEdgeOfInlineContainerIfNecessary(bool is_horizontal) {
    ToBox()->MoveWithEdgeOfInlineContainerIfNecessary(is_horizontal);
  }

  void Move(const LayoutUnit& width, const LayoutUnit& height) {
    ToBox()->Move(width, height);
  }

  bool HasOverflowModel() const { return ToBox()->HasOverflowModel(); }
  LayoutRect LogicalVisualOverflowRectForPropagation() const {
    return ToBox()->LogicalVisualOverflowRectForPropagation();
  }
  LayoutRect LogicalLayoutOverflowRectForPropagation() const {
    return ToBox()->LogicalLayoutOverflowRectForPropagation();
  }

  void SetLocation(const LayoutPoint& location) {
    return ToBox()->SetLocation(location);
  }

  void SetSize(const LayoutSize& size) { return ToBox()->SetSize(size); }

  IntSize ScrolledContentOffset() const {
    return ToBox()->ScrolledContentOffset();
  }

  InlineBox* CreateInlineBox() { return ToBox()->CreateInlineBox(); }

  InlineBox* InlineBoxWrapper() const { return ToBox()->InlineBoxWrapper(); }

  void SetInlineBoxWrapper(InlineBox* box) {
    return ToBox()->SetInlineBoxWrapper(box);
  }

#ifndef NDEBUG

  void ShowLineTreeAndMark(const InlineBox* marked_box1,
                           const char* marked_label1) const {
    if (GetLayoutObject()->IsLayoutBlockFlow())
      ToLayoutBlockFlow(GetLayoutObject())
          ->ShowLineTreeAndMark(marked_box1, marked_label1);
  }

#endif

 private:
  LayoutBox* ToBox() { return ToLayoutBox(GetLayoutObject()); }

  const LayoutBox* ToBox() const { return ToLayoutBox(GetLayoutObject()); }
};

inline LineLayoutBox LineLayoutItem::ContainingBlock() const {
  return LineLayoutBox(GetLayoutObject()->ContainingBlock());
}

}  // namespace blink

#endif  // LineLayoutBox_h
