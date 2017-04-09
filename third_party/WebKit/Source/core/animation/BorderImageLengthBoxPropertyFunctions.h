// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BorderImageLengthBoxPropertyFunctions_h
#define BorderImageLengthBoxPropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class BorderImageLengthBoxPropertyFunctions {
 public:
  static const BorderImageLengthBox& GetInitialBorderImageLengthBox(
      CSSPropertyID property) {
    return GetBorderImageLengthBox(property, ComputedStyle::InitialStyle());
  }

  static const BorderImageLengthBox& GetBorderImageLengthBox(
      CSSPropertyID property,
      const ComputedStyle& style) {
    switch (property) {
      case CSSPropertyBorderImageOutset:
        return style.BorderImageOutset();
      case CSSPropertyBorderImageWidth:
        return style.BorderImageWidth();
      case CSSPropertyWebkitMaskBoxImageOutset:
        return style.MaskBoxImageOutset();
      case CSSPropertyWebkitMaskBoxImageWidth:
        return style.MaskBoxImageWidth();
      default:
        NOTREACHED();
        return GetInitialBorderImageLengthBox(CSSPropertyBorderImageOutset);
    }
  }

  static void SetBorderImageLengthBox(CSSPropertyID property,
                                      ComputedStyle& style,
                                      const BorderImageLengthBox& box) {
    switch (property) {
      case CSSPropertyBorderImageOutset:
        style.SetBorderImageOutset(box);
        break;
      case CSSPropertyWebkitMaskBoxImageOutset:
        style.SetMaskBoxImageOutset(box);
        break;
      case CSSPropertyBorderImageWidth:
        style.SetBorderImageWidth(box);
        break;
      case CSSPropertyWebkitMaskBoxImageWidth:
        style.SetMaskBoxImageWidth(box);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
};

}  // namespace blink

#endif  // BorderImageLengthBoxPropertyFunctions_h
