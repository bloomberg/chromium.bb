// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageSlicePropertyFunctions_h
#define ImageSlicePropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

// This struct doesn't retain ownership of the slices, treat it like a
// reference.
struct ImageSlice {
  ImageSlice(const LengthBox& slices, bool fill) : slices(slices), fill(fill) {}

  const LengthBox& slices;
  bool fill;
};

class ImageSlicePropertyFunctions {
 public:
  static ImageSlice GetInitialImageSlice(const CSSProperty& property) {
    return GetImageSlice(property, ComputedStyle::InitialStyle());
  }

  static ImageSlice GetImageSlice(const CSSProperty& property,
                                  const ComputedStyle& style) {
    switch (property.PropertyID()) {
      default:
        NOTREACHED();
      // Fall through.
      case CSSPropertyBorderImageSlice:
        return ImageSlice(style.BorderImageSlices(),
                          style.BorderImageSlicesFill());
      case CSSPropertyWebkitMaskBoxImageSlice:
        return ImageSlice(style.MaskBoxImageSlices(),
                          style.MaskBoxImageSlicesFill());
    }
  }

  static void SetImageSlice(const CSSProperty& property,
                            ComputedStyle& style,
                            const ImageSlice& slice) {
    switch (property.PropertyID()) {
      case CSSPropertyBorderImageSlice:
        style.SetBorderImageSlices(slice.slices);
        style.SetBorderImageSlicesFill(slice.fill);
        break;
      case CSSPropertyWebkitMaskBoxImageSlice:
        style.SetMaskBoxImageSlices(slice.slices);
        style.SetMaskBoxImageSlicesFill(slice.fill);
        break;
      default:
        NOTREACHED();
    }
  }
};

}  // namespace blink

#endif  // ImageSlicePropertyFunctions_h
