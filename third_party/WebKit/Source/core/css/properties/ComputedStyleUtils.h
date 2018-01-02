// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStyleUtils_h
#define ComputedStyleUtils_h

#include "core/style/ComputedStyle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class CSSValue;
class StyleColor;

class ComputedStyleUtils {
  STATIC_ONLY(ComputedStyleUtils);

 public:
  static CSSValue* CurrentColorOrValidColor(const ComputedStyle&,
                                            const StyleColor&);
  static const blink::Color BorderSideColor(const ComputedStyle&,
                                            const StyleColor&,
                                            EBorderStyle,
                                            bool visited_link);
  static CSSValue* ZoomAdjustedPixelValueForLength(const Length&,
                                                   const ComputedStyle&);
};

}  // namespace blink

#endif  // ComputedStyleUtils_h
