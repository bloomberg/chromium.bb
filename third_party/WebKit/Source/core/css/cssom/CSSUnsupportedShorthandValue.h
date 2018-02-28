// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnsupportedShorthandValue_h
#define CSSUnsupportedShorthandValue_h

#include "base/macros.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

// CSSUnsupportedShorthandValue is the internal representation for an
// unsupported shorthand property. It is tied to that specific property and is
// only considered valid for that property.
class CORE_EXPORT CSSUnsupportedShorthandValue final : public CSSStyleValue {
 public:
  static CSSUnsupportedShorthandValue* Create(CSSPropertyID property) {
    return new CSSUnsupportedShorthandValue(property);
  }

  StyleValueType GetType() const override {
    return StyleValueType::kShorthandType;
  }
  CSSPropertyID GetProperty() const { return property_; }
  const HeapVector<Member<const CSSValue>>& LonghandValues() const {
    return css_values_;
  };
  // Shorthands cannot be converted into a single CSSValue.
  const CSSValue* ToCSSValue() const final {
    NOTREACHED();
    return nullptr;
  }
  String toString() const final {
    // TODO(816722): Implement serialization.
    return "";
  }

  void Append(const CSSValue* value) { css_values_.push_back(value); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(css_values_);
    CSSStyleValue::Trace(visitor);
  }

 private:
  CSSUnsupportedShorthandValue(CSSPropertyID property) : property_(property) {}

  const CSSPropertyID property_;
  // TODO(816722): Convert this to a CSSValueList if we can confirm that
  // we will never have null values.
  HeapVector<Member<const CSSValue>> css_values_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnsupportedShorthandValue);
};

DEFINE_TYPE_CASTS(CSSUnsupportedShorthandValue,
                  CSSStyleValue,
                  value,
                  value->GetType() ==
                      CSSStyleValue::StyleValueType::kShorthandType,
                  value.GetType() ==
                      CSSStyleValue::StyleValueType::kShorthandType);

}  // namespace blink

#endif  // CSSUnsupportedShorthandValue_h
