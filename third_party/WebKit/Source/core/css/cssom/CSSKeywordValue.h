// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSKeywordValue_h
#define CSSKeywordValue_h

#include "base/macros.h"
#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSKeywordValue final : public CSSStyleValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSKeywordValue* Create(const String& keyword);
  static CSSKeywordValue* Create(const String& keyword, ExceptionState&);
  static CSSKeywordValue* FromCSSValue(const CSSValue&);

  StyleValueType GetType() const override { return kKeywordType; }

  const String& value() const;
  void setValue(const String& keyword, ExceptionState&);
  CSSValueID KeywordValueID() const;

  const CSSValue* ToCSSValue() const override;

 private:
  explicit CSSKeywordValue(const String& keyword) : keyword_value_(keyword) {}

  String keyword_value_;
  DISALLOW_COPY_AND_ASSIGN(CSSKeywordValue);
};

DEFINE_TYPE_CASTS(CSSKeywordValue,
                  CSSStyleValue,
                  value,
                  value->GetType() ==
                      CSSStyleValue::StyleValueType::kKeywordType,
                  value.GetType() ==
                      CSSStyleValue::StyleValueType::kKeywordType);

}  // namespace blink

#endif
