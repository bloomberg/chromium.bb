// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontVariationValue_h
#define CSSFontVariationValue_h

#include "core/css/CSSValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CSSFontVariationValue : public CSSValue {
 public:
  static CSSFontVariationValue* Create(const AtomicString& tag, float value) {
    return new CSSFontVariationValue(tag, value);
  }

  const AtomicString& Tag() const { return tag_; }
  float Value() const { return value_; }
  String CustomCSSText() const;

  bool Equals(const CSSFontVariationValue&) const;

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  CSSFontVariationValue(const AtomicString& tag, float value);

  AtomicString tag_;
  const float value_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontVariationValue, IsFontVariationValue());

}  // namespace blink

#endif
