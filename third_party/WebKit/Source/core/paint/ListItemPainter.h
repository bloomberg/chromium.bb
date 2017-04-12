// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListItemPainter_h
#define ListItemPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutListItem;
class LayoutPoint;

class ListItemPainter {
  STACK_ALLOCATED();

 public:
  ListItemPainter(const LayoutListItem& layout_list_item)
      : layout_list_item_(layout_list_item) {}

  void Paint(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const LayoutListItem& layout_list_item_;
};

}  // namespace blink

#endif  // ListItemPainter_h
