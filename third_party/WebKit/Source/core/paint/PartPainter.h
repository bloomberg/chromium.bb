// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PartPainter_h
#define PartPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutPart;

class PartPainter {
  STACK_ALLOCATED();

 public:
  PartPainter(const LayoutPart& layout_part) : layout_part_(layout_part) {}

  void Paint(const PaintInfo&, const LayoutPoint&);
  void PaintContents(const PaintInfo&, const LayoutPoint&);

 private:
  bool IsSelected() const;

  const LayoutPart& layout_part_;
};

}  // namespace blink

#endif  // PartPainter_h
