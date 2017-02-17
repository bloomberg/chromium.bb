// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewPaintInvalidator_h
#define ViewPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutView;
struct PaintInvalidatorContext;

class ViewPaintInvalidator {
  STACK_ALLOCATED();

 public:
  ViewPaintInvalidator(const LayoutView& view,
                       const PaintInvalidatorContext& context)
      : m_view(view), m_context(context) {}

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  void invalidateBackgroundIfNeeded();

  const LayoutView& m_view;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
