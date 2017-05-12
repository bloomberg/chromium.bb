// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableReferenceValue_h
#define CSSVariableReferenceValue_h

#include "core/css/CSSValue.h"
#include "core/css/CSSVariableData.h"
#include "core/css/parser/CSSParserContext.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CSSVariableReferenceValue : public CSSValue {
 public:
  static CSSVariableReferenceValue* Create(PassRefPtr<CSSVariableData> data,
                                           const CSSParserContext& context) {
    return new CSSVariableReferenceValue(std::move(data), context);
  }

  CSSVariableData* VariableDataValue() const { return data_.Get(); }
  const CSSParserContext* ParserContext() const {
    return parser_context_.Get();
  }

  bool Equals(const CSSVariableReferenceValue& other) const {
    return data_ == other.data_;
  }
  String CustomCSSText() const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSVariableReferenceValue(PassRefPtr<CSSVariableData> data,
                            const CSSParserContext& context)
      : CSSValue(kVariableReferenceClass),
        data_(std::move(data)),
        parser_context_(context) {
    DCHECK(parser_context_);
  }

  RefPtr<CSSVariableData> data_;
  Member<const CSSParserContext> parser_context_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSVariableReferenceValue,
                            IsVariableReferenceValue());

}  // namespace blink

#endif  // CSSVariableReferenceValue_h
