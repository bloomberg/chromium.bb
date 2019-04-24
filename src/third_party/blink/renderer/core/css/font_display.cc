// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_display.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"

namespace blink {

FontDisplay CSSValueToFontDisplay(const CSSValue* value) {
  if (value && value->IsIdentifierValue()) {
    switch (ToCSSIdentifierValue(value)->GetValueID()) {
      case CSSValueAuto:
        return kFontDisplayAuto;
      case CSSValueBlock:
        return kFontDisplayBlock;
      case CSSValueSwap:
        return kFontDisplaySwap;
      case CSSValueFallback:
        return kFontDisplayFallback;
      case CSSValueOptional:
        return kFontDisplayOptional;
      default:
        break;
    }
  }
  return kFontDisplayAuto;
}

}  // namespace blink
