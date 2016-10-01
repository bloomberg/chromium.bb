// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGModelObjectPaintInvalidator.h"

#include "core/layout/svg/LayoutSVGModelObject.h"
#include "core/paint/ObjectPaintInvalidator.h"

namespace blink {

PaintInvalidationReason
SVGModelObjectPaintInvalidator::invalidatePaintIfNeeded() {
  ObjectPaintInvalidatorWithContext objectPaintInvalidator(m_object, m_context);
  PaintInvalidationReason reason =
      objectPaintInvalidator.computePaintInvalidationReason();

  // Disable incremental invalidation for SVG objects to prevent under-
  // invalidation. Unlike boxes, it is non-trivial (and rare) for SVG objects
  // to be able to be incrementally invalidated (e.g., on height changes).
  if (reason == PaintInvalidationIncremental &&
      m_context.oldBounds != m_context.newBounds)
    reason = PaintInvalidationFull;

  return objectPaintInvalidator.invalidatePaintIfNeededWithComputedReason(
      reason);
}

}  // namespace blink
