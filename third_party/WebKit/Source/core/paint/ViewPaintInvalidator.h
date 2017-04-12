// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewPaintInvalidator_h
#define ViewPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutView;
struct PaintInvalidatorContext;

class ViewPaintInvalidator {
  STACK_ALLOCATED();

 public:
  ViewPaintInvalidator(const LayoutView& view,
                       const PaintInvalidatorContext& context)
      : view_(view), context_(context) {}

  PaintInvalidationReason InvalidatePaintIfNeeded();

 private:
  void InvalidateBackgroundIfNeeded();

  const LayoutView& view_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
