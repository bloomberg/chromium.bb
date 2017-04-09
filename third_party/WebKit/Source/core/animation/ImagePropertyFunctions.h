// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImagePropertyFunctions_h
#define ImagePropertyFunctions_h

#include "core/CSSPropertyNames.h"
#include "core/style/ComputedStyle.h"

namespace blink {

class ImagePropertyFunctions {
 public:
  static const StyleImage* GetInitialStyleImage(CSSPropertyID) {
    return nullptr;
  }

  static const StyleImage* GetStyleImage(CSSPropertyID property,
                                         const ComputedStyle& style) {
    switch (property) {
      case CSSPropertyBorderImageSource:
        return style.BorderImageSource();
      case CSSPropertyListStyleImage:
        return style.ListStyleImage();
      case CSSPropertyWebkitMaskBoxImageSource:
        return style.MaskBoxImageSource();
      default:
        NOTREACHED();
        return nullptr;
    }
  }

  static void SetStyleImage(CSSPropertyID property,
                            ComputedStyle& style,
                            StyleImage* image) {
    switch (property) {
      case CSSPropertyBorderImageSource:
        style.SetBorderImageSource(image);
        break;
      case CSSPropertyListStyleImage:
        style.SetListStyleImage(image);
        break;
      case CSSPropertyWebkitMaskBoxImageSource:
        style.SetMaskBoxImageSource(image);
        break;
      default:
        NOTREACHED();
    }
  }
};

}  // namespace blink

#endif  // ImagePropertyFunctions_h
