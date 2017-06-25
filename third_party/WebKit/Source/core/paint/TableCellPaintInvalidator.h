// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableCellPaintInvalidator_h
#define TableCellPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutObject;
class LayoutTableCell;
struct PaintInvalidatorContext;

class TableCellPaintInvalidator {
  STACK_ALLOCATED();

 public:
  TableCellPaintInvalidator(const LayoutTableCell& cell,
                            const PaintInvalidatorContext& context)
      : cell_(cell), context_(context) {}

  PaintInvalidationReason InvalidatePaint();

 private:
  void InvalidateContainerForCellGeometryChange(
      const LayoutObject& container,
      const PaintInvalidatorContext* container_context);

  const LayoutTableCell& cell_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
