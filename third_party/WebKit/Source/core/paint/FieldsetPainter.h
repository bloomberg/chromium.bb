// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FieldsetPainter_h
#define FieldsetPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutFieldset;

class FieldsetPainter {
  STACK_ALLOCATED();

 public:
  FieldsetPainter(const LayoutFieldset& layout_fieldset)
      : layout_fieldset_(layout_fieldset) {}

  void PaintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
  void PaintMask(const PaintInfo&, const LayoutPoint&);

 private:
  const LayoutFieldset& layout_fieldset_;
};

}  // namespace blink

#endif  // FieldsetPainter_h
