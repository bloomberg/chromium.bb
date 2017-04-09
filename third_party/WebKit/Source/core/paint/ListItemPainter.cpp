// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ListItemPainter.h"

#include "core/layout/LayoutListItem.h"
#include "core/paint/BlockPainter.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void ListItemPainter::Paint(const PaintInfo& paint_info,
                            const LayoutPoint& paint_offset) {
  if (!layout_list_item_.LogicalHeight() && layout_list_item_.HasOverflowClip())
    return;

  BlockPainter(layout_list_item_).Paint(paint_info, paint_offset);
}

}  // namespace blink
