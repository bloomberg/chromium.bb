// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontVariationValue.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

CSSFontVariationValue::CSSFontVariationValue(const AtomicString& tag,
                                             float value)
    : CSSValue(kFontVariationClass), tag_(tag), value_(value) {}

String CSSFontVariationValue::CustomCSSText() const {
  StringBuilder builder;
  builder.Append('\'');
  builder.Append(tag_);
  builder.Append("' ");
  builder.AppendNumber(value_);
  return builder.ToString();
}

bool CSSFontVariationValue::Equals(const CSSFontVariationValue& other) const {
  return tag_ == other.tag_ && value_ == other.value_;
}

}  // namespace blink
