// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGModelObjectPaintInvalidator_h
#define SVGModelObjectPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutSVGModelObject;
struct PaintInvalidatorContext;

class SVGModelObjectPaintInvalidator {
  STACK_ALLOCATED();

 public:
  SVGModelObjectPaintInvalidator(const LayoutSVGModelObject& object,
                                 const PaintInvalidatorContext& context)
      : m_object(object), m_context(context) {}

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  const LayoutSVGModelObject& m_object;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
