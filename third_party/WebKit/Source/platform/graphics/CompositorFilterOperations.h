// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterOperations_h
#define CompositorFilterOperations_h

#include "cc/output/filter_operations.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/Color.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "wtf/text/WTFString.h"

class SkImageFilter;

namespace blink {

// An ordered list of filter operations.
class PLATFORM_EXPORT CompositorFilterOperations {
 public:
  const cc::FilterOperations& asCcFilterOperations() const;
  cc::FilterOperations releaseCcFilterOperations();

  void appendGrayscaleFilter(float amount);
  void appendSepiaFilter(float amount);
  void appendSaturateFilter(float amount);
  void appendHueRotateFilter(float amount);
  void appendInvertFilter(float amount);
  void appendBrightnessFilter(float amount);
  void appendContrastFilter(float amount);
  void appendOpacityFilter(float amount);
  void appendBlurFilter(float amount);
  void appendDropShadowFilter(IntPoint offset, float stdDeviation, Color);
  void appendColorMatrixFilter(SkScalar matrix[20]);
  void appendZoomFilter(float amount, int inset);
  void appendSaturatingBrightnessFilter(float amount);

  void appendReferenceFilter(sk_sp<SkImageFilter>);

  void clear();
  bool isEmpty() const;

  // Returns a rect covering the destination pixels that can be affected by
  // source pixels in |inputRect|.
  FloatRect mapRect(const FloatRect& inputRect) const;

  bool hasFilterThatMovesPixels() const;

  // For reference filters, this equality operator compares pointers of the
  // image_filter fields instead of their values.
  bool operator==(const CompositorFilterOperations&) const;
  bool operator!=(const CompositorFilterOperations& o) const {
    return !(*this == o);
  }

  bool equalsIgnoringReferenceFilters(const CompositorFilterOperations&) const;

  String toString() const;

 private:
  cc::FilterOperations m_filterOperations;
};

}  // namespace blink

#endif  // CompositorFilterOperations_h
