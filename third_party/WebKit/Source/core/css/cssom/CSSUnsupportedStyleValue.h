// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnsupportedStyleValue_h
#define CSSUnsupportedStyleValue_h

#include "base/macros.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

// CSSUnsupportedStyleValue is the internal representation of a base
// CSSStyleValue that is returned when we do not yet support a CSS Typed OM type
// for a given CSS Value.
class CORE_EXPORT CSSUnsupportedStyleValue final : public CSSStyleValue {
 public:
  static CSSUnsupportedStyleValue* Create(const String& css_text) {
    return new CSSUnsupportedStyleValue(css_text);
  }

  StyleValueType GetType() const override {
    return StyleValueType::kUnknownType;
  }
  const CSSValue* ToCSSValue(SecureContextMode) const override;
  const CSSValue* ToCSSValueWithProperty(CSSPropertyID,
                                         SecureContextMode) const override;
  String toString(const ExecutionContext*) const override { return css_text_; }

 private:
  CSSUnsupportedStyleValue(const String& css_text) : css_text_(css_text) {}

  String css_text_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnsupportedStyleValue);
};

}  // namespace blink

#endif  // CSSUnsupportedStyleValue_h
