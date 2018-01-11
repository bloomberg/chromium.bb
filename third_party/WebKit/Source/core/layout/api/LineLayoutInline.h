// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutInline_h
#define LineLayoutInline_h

#include "core/layout/LayoutInline.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "platform/LayoutUnit.h"

namespace blink {

class LayoutInline;

class LineLayoutInline : public LineLayoutBoxModel {
 public:
  explicit LineLayoutInline(LayoutInline* layout_inline)
      : LineLayoutBoxModel(layout_inline) {}

  explicit LineLayoutInline(const LineLayoutItem& item)
      : LineLayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.IsLayoutInline());
  }

  explicit LineLayoutInline(std::nullptr_t) : LineLayoutBoxModel(nullptr) {}

  LineLayoutInline() = default;

  LineLayoutItem FirstChild() const {
    return LineLayoutItem(ToInline()->FirstChild());
  }

  LineLayoutItem LastChild() const {
    return LineLayoutItem(ToInline()->LastChild());
  }

  LayoutUnit MarginStart() const { return ToInline()->MarginStart(); }

  LayoutUnit MarginEnd() const { return ToInline()->MarginEnd(); }

  LayoutUnit BorderStart() const { return ToInline()->BorderStart(); }

  LayoutUnit BorderEnd() const { return ToInline()->BorderEnd(); }

  LayoutUnit PaddingStart() const { return ToInline()->PaddingStart(); }

  LayoutUnit PaddingEnd() const { return ToInline()->PaddingEnd(); }

  bool HasInlineDirectionBordersPaddingOrMargin() const {
    return ToInline()->HasInlineDirectionBordersPaddingOrMargin();
  }

  bool AlwaysCreateLineBoxes() const {
    return ToInline()->AlwaysCreateLineBoxes();
  }

  InlineBox* FirstLineBoxIncludingCulling() const {
    return ToInline()->FirstLineBoxIncludingCulling();
  }

  InlineBox* LastLineBoxIncludingCulling() const {
    return ToInline()->LastLineBoxIncludingCulling();
  }

  LineBoxList* LineBoxes() { return ToInline()->LineBoxes(); }

  bool HitTestCulledInline(HitTestResult& result,
                           const HitTestLocation& location_in_container,
                           const LayoutPoint& accumulated_offset) {
    return ToInline()->HitTestCulledInline(result, location_in_container,
                                           accumulated_offset);
  }

  LayoutBoxModelObject* Continuation() const {
    return ToInline()->Continuation();
  }

  InlineBox* CreateAndAppendInlineFlowBox() {
    return ToInline()->CreateAndAppendInlineFlowBox();
  }

  InlineFlowBox* LastLineBox() { return ToInline()->LastLineBox(); }

 protected:
  LayoutInline* ToInline() { return ToLayoutInline(GetLayoutObject()); }

  const LayoutInline* ToInline() const {
    return ToLayoutInline(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutInline_h
