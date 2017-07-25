/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSFontStyleRangeValue_h
#define CSSFontStyleRangeValue_h

#include "CSSIdentifierValue.h"
#include "CSSValueList.h"

namespace blink {

class CSSFontStyleRangeValue final : public CSSValue {
 public:
  static CSSFontStyleRangeValue* Create(
      const CSSIdentifierValue& font_style_value) {
    return new CSSFontStyleRangeValue(font_style_value);
  }
  static CSSFontStyleRangeValue* Create(
      const CSSIdentifierValue& font_style_value,
      const CSSValueList& oblique_values) {
    return new CSSFontStyleRangeValue(font_style_value, oblique_values);
  }

  const CSSIdentifierValue* GetFontStyleValue() const {
    return font_style_value_.Get();
  }
  const CSSValueList* GetObliqueValues() const { return oblique_values_.Get(); }

  String CustomCSSText() const;

  bool Equals(const CSSFontStyleRangeValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSFontStyleRangeValue(const CSSIdentifierValue& font_style_value,
                         const CSSValueList& oblique_values)
      : CSSValue(kFontStyleRangeClass),
        font_style_value_(&font_style_value),
        oblique_values_(&oblique_values) {}

  CSSFontStyleRangeValue(const CSSIdentifierValue& font_style_value)
      : CSSValue(kFontStyleRangeClass),
        font_style_value_(&font_style_value),
        oblique_values_(nullptr) {}

  Member<const CSSIdentifierValue> font_style_value_;
  Member<const CSSValueList> oblique_values_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontStyleRangeValue, IsFontStyleRangeValue());

}  // namespace blink

#endif
