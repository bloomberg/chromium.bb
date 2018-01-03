// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnparsedValue_h
#define CSSUnparsedValue_h

#include "base/macros.h"
#include "bindings/core/v8/string_or_css_variable_reference_value.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CSSVariableReferenceValue;
class CSSVariableData;

class CORE_EXPORT CSSUnparsedValue final : public CSSStyleValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSUnparsedValue* Create(
      const HeapVector<StringOrCSSVariableReferenceValue>& tokens) {
    return new CSSUnparsedValue(tokens);
  }

  static CSSUnparsedValue* FromCSSValue(const CSSVariableReferenceValue&);
  static CSSUnparsedValue* FromCSSValue(const CSSVariableData&);

  const CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kUnparsedType; }

  StringOrCSSVariableReferenceValue AnonymousIndexedGetter(
      unsigned index,
      ExceptionState& exception_state) const {
    if (index < tokens_.size())
      return tokens_[index];
    return {};
  }

  size_t length() const { return tokens_.size(); }

  virtual void Trace(Visitor* visitor) {
    visitor->Trace(tokens_);
    CSSStyleValue::Trace(visitor);
  }

 protected:
  CSSUnparsedValue(const HeapVector<StringOrCSSVariableReferenceValue>& tokens)
      : CSSStyleValue(), tokens_(tokens) {}

 private:
  static CSSUnparsedValue* FromString(const String& string) {
    HeapVector<StringOrCSSVariableReferenceValue> tokens;
    tokens.push_back(StringOrCSSVariableReferenceValue::FromString(string));
    return Create(tokens);
  }

  String ToString() const;

  FRIEND_TEST_ALL_PREFIXES(CSSVariableReferenceValueTest, MixedList);

  HeapVector<StringOrCSSVariableReferenceValue> tokens_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnparsedValue);
};

}  // namespace blink

#endif  // CSSUnparsedValue_h
