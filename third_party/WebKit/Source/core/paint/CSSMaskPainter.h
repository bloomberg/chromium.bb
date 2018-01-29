// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSMaskPainter_h
#define CSSMaskPainter_h

#include "core/CoreExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Optional.h"

namespace blink {

class LayoutObject;
class LayoutPoint;

class CORE_EXPORT CSSMaskPainter {
 public:
  // Returns the bounding box of the computed mask, which could be
  // smaller or bigger than the reference box. Returns nullopt if the
  // there is no mask or the mask is invalid.
  static Optional<IntRect> MaskBoundingBox(const LayoutObject&,
                                           const LayoutPoint& paint_offset);

  // Returns the color filter used to interpret mask pixel values as opaqueness.
  // The return value is undefined if there is no mask or the mask is invalid.
  static ColorFilter MaskColorFilter(const LayoutObject&);
};

}  // namespace blink

#endif  // CSSMaskPainter_h
