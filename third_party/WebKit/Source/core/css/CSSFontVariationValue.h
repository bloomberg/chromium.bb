// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontVariationValue_h
#define CSSFontVariationValue_h

#include "core/css/CSSValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSFontVariationValue : public CSSValue {
 public:
  static CSSFontVariationValue* create(const AtomicString& tag, float value) {
    return new CSSFontVariationValue(tag, value);
  }

  const AtomicString& tag() const { return m_tag; }
  float value() const { return m_value; }
  String customCSSText() const;

  bool equals(const CSSFontVariationValue&) const;

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    CSSValue::traceAfterDispatch(visitor);
  }

 private:
  CSSFontVariationValue(const AtomicString& tag, float value);

  AtomicString m_tag;
  const float m_value;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontVariationValue, isFontVariationValue());

}  // namespace blink

#endif
