// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {
class HTMLUListElement;
class HTMLOListElement;

LayoutNGOutsideListMarker::LayoutNGOutsideListMarker(Element* element)
    : LayoutNGBlockFlowMixin<LayoutBlockFlow>(element) {}

bool LayoutNGOutsideListMarker::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGOutsideListMarker ||
         LayoutNGMixin<LayoutBlockFlow>::IsOfType(type);
}

void LayoutNGOutsideListMarker::WillCollectInlines() {
  list_marker_.UpdateMarkerTextIfNeeded(*this);
}

LayoutBox::PaginationBreakability
LayoutNGOutsideListMarker::GetPaginationBreakability(
    FragmentationEngine engine) const {
  // Outside list markers are always monolithic.
  return kForbidBreaks;
}

bool LayoutNGOutsideListMarker::NeedsOccupyWholeLine() const {
  if (!GetDocument().InQuirksMode())
    return false;

  LayoutObject* next_sibling = NextSibling();
  if (next_sibling && next_sibling->GetNode() &&
      (IsA<HTMLUListElement>(*next_sibling->GetNode()) ||
       IsA<HTMLOListElement>(*next_sibling->GetNode())))
    return true;

  return false;
}

PositionWithAffinity LayoutNGOutsideListMarker::PositionForPoint(
    const PhysicalOffset&) const {
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  return PositionBeforeThis();
}

}  // namespace blink
