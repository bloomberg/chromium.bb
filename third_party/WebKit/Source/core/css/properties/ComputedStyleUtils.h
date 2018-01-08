// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStyleUtils_h
#define ComputedStyleUtils_h

#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSValueList.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/Allocator.h"

namespace blink {

using namespace cssvalue;

class ComputedStyle;
class CSSValue;
class StyleColor;

class ComputedStyleUtils {
  STATIC_ONLY(ComputedStyleUtils);

 public:
  static inline AtomicString SerializeAsFragmentIdentifier(
      const AtomicString& resource) {
    return "#" + resource;
  }
  static CSSValue* CurrentColorOrValidColor(const ComputedStyle&,
                                            const StyleColor&);
  static const blink::Color BorderSideColor(const ComputedStyle&,
                                            const StyleColor&,
                                            EBorderStyle,
                                            bool visited_link);
  static CSSValue* ZoomAdjustedPixelValueForLength(const Length&,
                                                   const ComputedStyle&);
  static const CSSValue* BackgroundImageOrWebkitMaskImage(const FillLayer&);
  static const CSSValue* ValueForFillSize(const FillSize&,
                                          const ComputedStyle&);
  static const CSSValue* BackgroundImageOrWebkitMaskSize(const ComputedStyle&,
                                                         const FillLayer&);
  static const CSSValueList* CreatePositionListForLayer(const CSSProperty&,
                                                        const FillLayer&,
                                                        const ComputedStyle&);
  static const CSSValue* ValueForFillRepeat(EFillRepeat x_repeat,
                                            EFillRepeat y_repeat);
  static const CSSValueList* ValuesForBackgroundShorthand(
      const ComputedStyle&,
      const LayoutObject*,
      Node*,
      bool allow_visited_style);
  static const CSSValue* BackgroundRepeatOrWebkitMaskRepeat(const FillLayer*);
  static const CSSValue* BackgroundPositionOrWebkitMaskPosition(
      const CSSProperty&,
      const ComputedStyle&,
      const FillLayer*);
  static const CSSValue* BackgroundPositionXOrWebkitMaskPositionX(
      const ComputedStyle&,
      const FillLayer*);
  static const CSSValue* BackgroundPositionYOrWebkitMaskPositionY(
      const ComputedStyle&,
      const FillLayer*);
  static cssvalue::CSSBorderImageSliceValue* ValueForNinePieceImageSlice(
      const NinePieceImage&);
  static CSSQuadValue* ValueForNinePieceImageQuad(const BorderImageLengthBox&,
                                                  const ComputedStyle&);
  static CSSValue* ValueForNinePieceImageRepeat(const NinePieceImage&);
  static CSSValue* ValueForNinePieceImage(const NinePieceImage&,
                                          const ComputedStyle&);
  static CSSValue* ValueForReflection(const StyleReflection*,
                                      const ComputedStyle&);
};

}  // namespace blink

#endif  // ComputedStyleUtils_h
