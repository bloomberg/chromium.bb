// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorFilterOperations_h
#define CompositorFilterOperations_h

#include "cc/paint/filter_operations.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntPoint.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/paint/PaintFilter.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

// An ordered list of filter operations.
class PLATFORM_EXPORT CompositorFilterOperations {
 public:
  const cc::FilterOperations& AsCcFilterOperations() const;
  cc::FilterOperations ReleaseCcFilterOperations();

  void AppendGrayscaleFilter(float amount);
  void AppendSepiaFilter(float amount);
  void AppendSaturateFilter(float amount);
  void AppendHueRotateFilter(float amount);
  void AppendInvertFilter(float amount);
  void AppendBrightnessFilter(float amount);
  void AppendContrastFilter(float amount);
  void AppendOpacityFilter(float amount);
  void AppendBlurFilter(float amount);
  void AppendDropShadowFilter(IntPoint offset, float std_deviation, Color);
  void AppendColorMatrixFilter(const cc::FilterOperation::Matrix&);
  void AppendZoomFilter(float amount, int inset);
  void AppendSaturatingBrightnessFilter(float amount);

  void AppendReferenceFilter(sk_sp<PaintFilter>);

  void Clear();
  bool IsEmpty() const;

  // Returns a rect covering the destination pixels that can be affected by
  // source pixels in |inputRect|.
  FloatRect MapRect(const FloatRect& input_rect) const;

  bool HasFilterThatMovesPixels() const;

  void SetReferenceBox(const FloatRect& r) { reference_box_ = r; }
  FloatRect ReferenceBox() const { return reference_box_; }

  // For reference filters, this equality operator compares pointers of the
  // image_filter fields instead of their values.
  bool operator==(const CompositorFilterOperations&) const;
  bool operator!=(const CompositorFilterOperations& o) const {
    return !(*this == o);
  }

  bool EqualsIgnoringReferenceFilters(const CompositorFilterOperations&) const;

  String ToString() const;

 private:
  cc::FilterOperations filter_operations_;
  FloatRect reference_box_;
};

}  // namespace blink

#endif  // CompositorFilterOperations_h
