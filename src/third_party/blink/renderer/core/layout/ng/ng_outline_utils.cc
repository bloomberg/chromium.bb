// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {


bool NGOutlineUtils::HasPaintedOutline(const ComputedStyle& style,
                                       const Node* node) {
  if (!style.HasOutline() || style.Visibility() != EVisibility::kVisible)
    return false;
  if (style.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(node, style))
    return false;
  return true;
}

bool NGOutlineUtils::IsInlineOutlineNonpaintingFragment(
    const NGPhysicalFragment& physical_fragment) {
  LayoutObject* layout_object = physical_fragment.GetLayoutObject();
  if (!layout_object)
    return false;
  if (!layout_object->IsLayoutInline())
    return false;
  if (layout_object->IsElementContinuation()) {
    // If continuation root did generate a fragment,
    // this fragment should not paint.
    if (layout_object->GetNode()->GetLayoutObject()->FirstInlineFragment())
      return true;
  }
  if (!layout_object->FirstInlineFragment())
    return false;
  return &layout_object->FirstInlineFragment()->PhysicalFragment() !=
         &physical_fragment;
}

}  // namespace blink
