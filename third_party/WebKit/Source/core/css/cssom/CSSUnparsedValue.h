// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnparsedValue_h
#define CSSUnparsedValue_h

#include "bindings/core/v8/string_or_css_variable_reference_value.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "platform/wtf/Vector.h"

namespace blink {

class CORE_EXPORT CSSUnparsedValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSUnparsedValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSUnparsedValue* Create(
      const HeapVector<StringOrCSSVariableReferenceValue>& fragments) {
    return new CSSUnparsedValue(fragments);
  }

  static CSSUnparsedValue* FromCSSValue(const CSSVariableReferenceValue&);

  const CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kUnparsedType; }

  StringOrCSSVariableReferenceValue fragmentAtIndex(uint32_t index) const {
    return fragments_.at(index);
  }

  size_t length() const { return fragments_.size(); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(fragments_);
    CSSStyleValue::Trace(visitor);
  }

 protected:
  CSSUnparsedValue(
      const HeapVector<StringOrCSSVariableReferenceValue>& fragments)
      : CSSStyleValue(), fragments_(fragments) {}

 private:
  static CSSUnparsedValue* FromString(String string) {
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    fragments.push_back(StringOrCSSVariableReferenceValue::FromString(string));
    return Create(fragments);
  }

  FRIEND_TEST_ALL_PREFIXES(CSSUnparsedValueTest, ListOfStrings);
  FRIEND_TEST_ALL_PREFIXES(CSSUnparsedValueTest,
                           ListOfCSSVariableReferenceValues);
  FRIEND_TEST_ALL_PREFIXES(CSSUnparsedValueTest, MixedList);
  FRIEND_TEST_ALL_PREFIXES(CSSVariableReferenceValueTest, MixedList);

  HeapVector<StringOrCSSVariableReferenceValue> fragments_;
};

}  // namespace blink

#endif  // CSSUnparsedValue_h
