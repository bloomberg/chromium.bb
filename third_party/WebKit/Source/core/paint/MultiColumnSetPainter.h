// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MultiColumnSetPainter_h
#define MultiColumnSetPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutMultiColumnSet;

class MultiColumnSetPainter {
  STACK_ALLOCATED();

 public:
  MultiColumnSetPainter(const LayoutMultiColumnSet& layout_multi_column_set)
      : layout_multi_column_set_(layout_multi_column_set) {}
  void PaintObject(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  void PaintColumnRules(const PaintInfo&, const LayoutPoint& paint_offset);

  const LayoutMultiColumnSet& layout_multi_column_set_;
};

}  // namespace blink

#endif  // MultiColumnSetPainter_h
