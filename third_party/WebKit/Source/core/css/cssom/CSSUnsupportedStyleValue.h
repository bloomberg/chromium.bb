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
  static CSSUnsupportedStyleValue* Create(const CSSValue& css_value) {
    return new CSSUnsupportedStyleValue(css_value);
  }

  StyleValueType GetType() const override {
    return StyleValueType::kUnknownType;
  }
  const CSSValue* ToCSSValue() const override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(css_value_);
    CSSStyleValue::Trace(visitor);
  }

 private:
  CSSUnsupportedStyleValue(const CSSValue& css_value) : css_value_(css_value) {}

  Member<const CSSValue> css_value_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnsupportedStyleValue);
};

}  // namespace blink

#endif  // CSSUnsupportedStyleValue_h
