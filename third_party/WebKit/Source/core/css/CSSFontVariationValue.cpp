// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontVariationValue.h"

#include "wtf/text/StringBuilder.h"

namespace blink {

CSSFontVariationValue::CSSFontVariationValue(const AtomicString& tag,
                                             float value)
    : CSSValue(FontVariationClass), m_tag(tag), m_value(value) {}

String CSSFontVariationValue::customCSSText() const {
  StringBuilder builder;
  builder.append('\'');
  builder.append(m_tag);
  builder.append("' ");
  builder.appendNumber(m_value);
  return builder.toString();
}

bool CSSFontVariationValue::equals(const CSSFontVariationValue& other) const {
  return m_tag == other.m_tag && m_value == other.m_value;
}

}  // namespace blink
