// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMapReadOnly.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSUnparsedValue.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/properties/CSSProperty.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final
    : public PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::
          IterationSource {
 public:
  explicit StylePropertyMapIterationSource(
      HeapVector<StylePropertyMapReadOnly::StylePropertyMapEntry> values)
      : index_(0), values_(values) {}

  bool Next(ScriptState*,
            String& key,
            CSSStyleValueOrCSSStyleValueSequence& value,
            ExceptionState&) override {
    if (index_ >= values_.size())
      return false;

    const StylePropertyMapReadOnly::StylePropertyMapEntry& pair =
        values_.at(index_++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(values_);
    PairIterable<String, CSSStyleValueOrCSSStyleValueSequence>::
        IterationSource::Trace(visitor);
  }

 private:
  size_t index_;
  const HeapVector<StylePropertyMapReadOnly::StylePropertyMapEntry> values_;
};

bool ComparePropertyNames(const String& a, const String& b) {
  if (a.StartsWith("--") == b.StartsWith("--"))
    return WTF::CodePointCompareLessThan(a, b);
  return b.StartsWith("--");
}

}  // namespace

CSSStyleValue* StylePropertyMapReadOnly::get(const String& property_name,
                                             ExceptionState& exception_state) {
  CSSStyleValueVector style_vector = getAll(property_name, exception_state);
  return style_vector.IsEmpty() ? nullptr : style_vector[0];
}

CSSStyleValueVector StylePropertyMapReadOnly::getAll(
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return CSSStyleValueVector();
  }

  DCHECK(isValidCSSPropertyID(property_id));
  const CSSValue* value = (property_id == CSSPropertyVariable)
                              ? GetCustomProperty(AtomicString(property_name))
                              : GetProperty(property_id);
  if (!value)
    return CSSStyleValueVector();

  return StyleValueFactory::CssValueToStyleValueVector(property_id, *value);
}

bool StylePropertyMapReadOnly::has(const String& property_name,
                                   ExceptionState& exception_state) {
  return !getAll(property_name, exception_state).IsEmpty();
}

Vector<String> StylePropertyMapReadOnly::getProperties() {
  Vector<String> result;

  ForEachProperty([&result](const String& name, const CSSValue&) {
    result.push_back(name);
  });

  std::sort(result.begin(), result.end(), ComparePropertyNames);
  return result;
}

StylePropertyMapReadOnly::IterationSource*
StylePropertyMapReadOnly::StartIteration(ScriptState*, ExceptionState&) {
  HeapVector<StylePropertyMapReadOnly::StylePropertyMapEntry> result;

  ForEachProperty([&result](const String& property_name,
                            const CSSValue& css_value) {
    const auto property_id = cssPropertyID(property_name);
    CSSStyleValueOrCSSStyleValueSequence value;
    if (property_id == CSSPropertyVariable) {
      const CSSCustomPropertyDeclaration& decl =
          ToCSSCustomPropertyDeclaration(css_value);
      DCHECK(decl.Value());
      value.SetCSSStyleValue(CSSUnparsedValue::FromCSSValue(*decl.Value()));
    } else {
      auto style_value_vector =
          StyleValueFactory::CssValueToStyleValueVector(property_id, css_value);
      if (style_value_vector.size() == 1)
        value.SetCSSStyleValue(style_value_vector[0]);
      else
        value.SetCSSStyleValueSequence(style_value_vector);
    }
    result.emplace_back(property_name, value);
  });

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return ComparePropertyNames(a.first, b.first);
  });
  return new StylePropertyMapIterationSource(result);
}

}  // namespace blink
