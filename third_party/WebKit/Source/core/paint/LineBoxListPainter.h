// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineBoxListPainter_h
#define LineBoxListPainter_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class LayoutBoxModelObject;
class LineBoxList;

class LineBoxListPainter {
  STACK_ALLOCATED();

 public:
  LineBoxListPainter(const LineBoxList& line_box_list)
      : line_box_list_(line_box_list) {}

  void Paint(const LayoutBoxModelObject&,
             const PaintInfo&,
             const LayoutPoint&) const;

 private:
  const LineBoxList& line_box_list_;
};

}  // namespace blink

#endif  // LineBoxListPainter_h
