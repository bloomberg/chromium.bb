// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/scoped_box_clipper.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

DISABLE_CFI_PERF
ScopedBoxClipper::ScopedBoxClipper(const LayoutBox& box,
                                   const PaintInfo& paint_info) {
  InitializeScopedProperties(paint_info.FragmentToPaint(box), box, paint_info);
}

DISABLE_CFI_PERF
ScopedBoxClipper::ScopedBoxClipper(const NGPaintFragment& fragment,
                                   const PaintInfo& paint_info) {
  DCHECK(fragment.GetLayoutObject());
  InitializeScopedProperties(
      paint_info.FragmentToPaint(*fragment.GetLayoutObject()), fragment,
      paint_info);
}

void ScopedBoxClipper::InitializeScopedProperties(
    const FragmentData* fragment_data,
    const DisplayItemClient& client,
    const PaintInfo& paint_info) {
  DCHECK(paint_info.phase != PaintPhase::kSelfBlockBackgroundOnly &&
         paint_info.phase != PaintPhase::kSelfOutlineOnly &&
         paint_info.phase != PaintPhase::kMask);
  if (!fragment_data)
    return;

  const auto* properties = fragment_data->PaintProperties();
  if (!properties)
    return;

  const auto* clip = properties->OverflowClip()
                         ? properties->OverflowClip()
                         : properties->InnerBorderRadiusClip();
  if (!clip)
    return;

  scoped_properties_.emplace(paint_info.context.GetPaintController(), clip,
                             client, paint_info.DisplayItemTypeForClipping());
}

}  // namespace blink
