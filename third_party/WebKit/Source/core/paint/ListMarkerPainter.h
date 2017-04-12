// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListMarkerPainter_h
#define ListMarkerPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutListMarker;
class LayoutPoint;

class ListMarkerPainter {
  STACK_ALLOCATED();

 public:
  ListMarkerPainter(const LayoutListMarker& layout_list_marker)
      : layout_list_marker_(layout_list_marker) {}

  void Paint(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const LayoutListMarker& layout_list_marker_;
};

}  // namespace blink

#endif  // ListMarkerPainter_h
