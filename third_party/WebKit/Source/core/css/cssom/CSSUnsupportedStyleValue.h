// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnsupportedStyleValue_h
#define CSSUnsupportedStyleValue_h

#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CORE_EXPORT CSSUnsupportedStyleValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSUnsupportedStyleValue);

 public:
  static CSSUnsupportedStyleValue* Create(const String& css_text) {
    return new CSSUnsupportedStyleValue(css_text);
  }

  StyleValueType GetType() const override { return StyleValueType::kUnknown; }
  const CSSValue* ToCSSValue() const override;
  const CSSValue* ToCSSValueWithProperty(CSSPropertyID) const override;
  String cssText() const override { return css_text_; }

 private:
  CSSUnsupportedStyleValue(const String& css_text) : css_text_(css_text) {}

  String css_text_;
};

}  // namespace blink

#endif  // CSSUnsupportedStyleValue_h
