// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTableBoxComponent_h
#define LayoutTableBoxComponent_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintResult.h"
#include "platform/graphics/paint/CullRect.h"

namespace blink {

class LayoutTable;

// Common super class for LayoutTableCol, LayoutTableSection and LayoutTableRow.
class CORE_EXPORT LayoutTableBoxComponent : public LayoutBox {
 public:
  static bool doCellsHaveDirtyWidth(const LayoutObject& tablePart,
                                    const LayoutTable&,
                                    const StyleDifference&,
                                    const ComputedStyle& oldStyle);

  class MutableForPainting : public LayoutObject::MutableForPainting {
   public:
    void updatePaintResult(PaintResult, const CullRect& paintRect);

   private:
    friend class LayoutTableBoxComponent;
    MutableForPainting(const LayoutTableBoxComponent& box)
        : LayoutObject::MutableForPainting(box) {}
  };
  MutableForPainting getMutableForPainting() const {
    return MutableForPainting(*this);
  }

 protected:
  explicit LayoutTableBoxComponent(Element* element)
      : LayoutBox(element), m_lastPaintResult(FullyPainted) {}

  const LayoutObjectChildList* children() const { return &m_children; }
  LayoutObjectChildList* children() { return &m_children; }

  LayoutObject* firstChild() const {
    DCHECK_EQ(children(), virtualChildren());
    return children()->firstChild();
  }
  LayoutObject* lastChild() const {
    DCHECK_EQ(children(), virtualChildren());
    return children()->lastChild();
  }

 private:
  // If you have a LayoutTableBoxComponent, use firstChild or lastChild instead.
  void slowFirstChild() const = delete;
  void slowLastChild() const = delete;

  LayoutObjectChildList* virtualChildren() override { return children(); }
  const LayoutObjectChildList* virtualChildren() const override {
    return children();
  }

  LayoutObjectChildList m_children;

  friend class MutableForPainting;
  PaintResult m_lastPaintResult;
  CullRect m_lastPaintRect;
};

}  // namespace blink

#endif  // LayoutTableBoxComponent_h
