// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTableBoxComponent_h
#define LayoutTableBoxComponent_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutTable.h"
#include "core/paint/PaintResult.h"
#include "platform/graphics/paint/CullRect.h"

namespace blink {

// Common super class for LayoutTableCol, LayoutTableSection and LayoutTableRow.
// Also provides utility functions for all table parts.
class CORE_EXPORT LayoutTableBoxComponent : public LayoutBox {
 public:
  static void InvalidateCollapsedBordersOnStyleChange(
      const LayoutObject& table_part,
      LayoutTable&,
      const StyleDifference&,
      const ComputedStyle& old_style);

  static bool DoCellsHaveDirtyWidth(const LayoutObject& table_part,
                                    const LayoutTable&,
                                    const StyleDifference&,
                                    const ComputedStyle& old_style);

  class MutableForPainting : public LayoutObject::MutableForPainting {
   public:
    void UpdatePaintResult(PaintResult, const CullRect& paint_rect);

   private:
    friend class LayoutTableBoxComponent;
    MutableForPainting(const LayoutTableBoxComponent& box)
        : LayoutObject::MutableForPainting(box) {}
  };
  MutableForPainting GetMutableForPainting() const {
    return MutableForPainting(*this);
  }

  // Should use TableStyle() instead of own style to determine cell order.
  const ComputedStyle& TableStyle() const { return Table()->StyleRef(); }

  BorderValue BorderStartInTableDirection() const {
    return StyleRef().BorderStartUsing(TableStyle());
  }
  BorderValue BorderEndInTableDirection() const {
    return StyleRef().BorderEndUsing(TableStyle());
  }
  BorderValue BorderBeforeInTableDirection() const {
    return StyleRef().BorderBeforeUsing(TableStyle());
  }
  BorderValue BorderAfterInTableDirection() const {
    return StyleRef().BorderAfterUsing(TableStyle());
  }

 protected:
  explicit LayoutTableBoxComponent(Element* element)
      : LayoutBox(element), last_paint_result_(kFullyPainted) {}

  const LayoutObjectChildList* Children() const { return &children_; }
  LayoutObjectChildList* Children() { return &children_; }

  LayoutObject* FirstChild() const {
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

 private:
  // If you have a LayoutTableBoxComponent, use firstChild or lastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  LayoutObjectChildList* VirtualChildren() override { return Children(); }
  const LayoutObjectChildList* VirtualChildren() const override {
    return Children();
  }

  virtual LayoutTable* Table() const = 0;

  LayoutObjectChildList children_;

  friend class MutableForPainting;
  PaintResult last_paint_result_;
  CullRect last_paint_rect_;
};

}  // namespace blink

#endif  // LayoutTableBoxComponent_h
