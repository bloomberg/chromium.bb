// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TablePaintInvalidator_h
#define TablePaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutTable;
struct PaintInvalidatorContext;

class TablePaintInvalidator {
  STACK_ALLOCATED();

 public:
  TablePaintInvalidator(const LayoutTable& table,
                        const PaintInvalidatorContext& context)
      : table_(table), context_(context) {}

  PaintInvalidationReason InvalidatePaint();

 private:
  const LayoutTable& table_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
