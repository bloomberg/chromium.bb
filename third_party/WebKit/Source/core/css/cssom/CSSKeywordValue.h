// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSKeywordValue_h
#define CSSKeywordValue_h

#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSKeywordValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSKeywordValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSKeywordValue* Create(const AtomicString& keyword);
  static CSSKeywordValue* Create(const AtomicString& keyword, ExceptionState&);
  static CSSKeywordValue* FromCSSValue(const CSSValue&);

  StyleValueType GetType() const override { return kKeywordType; }

  const AtomicString& keywordValue() const;
  CSSValueID KeywordValueID() const;

  CSSValue* ToCSSValue() const override;

 private:
  explicit CSSKeywordValue(const AtomicString& keyword)
      : keyword_value_(keyword) {}

  AtomicString keyword_value_;
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
