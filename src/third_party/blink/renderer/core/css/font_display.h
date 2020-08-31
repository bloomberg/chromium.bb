// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_DISPLAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_DISPLAY_H_

namespace blink {

class CSSValue;

enum FontDisplay {
  kFontDisplayAuto,
  kFontDisplayBlock,
  kFontDisplaySwap,
  kFontDisplayFallback,
  kFontDisplayOptional,
  kFontDisplayEnumMax,
};

FontDisplay CSSValueToFontDisplay(const CSSValue*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_DISPLAY_H_
