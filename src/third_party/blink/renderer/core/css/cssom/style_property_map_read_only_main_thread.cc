// Copyright 2018 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map_read_only_main_thread.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

class StylePropertyMapIterationSource final
    : public PairIterable<String, CSSStyleValueVector>::IterationSource {
 public:
  explicit StylePropertyMapIterationSource(
      HeapVector<StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry>
          values)
      : index_(0), values_(values) {}

  bool Next(ScriptState*,
            String& key,
            CSSStyleValueVector& value,
            ExceptionState&) override {
    if (index_ >= values_.size())
      return false;

    const StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry& pair =
        values_.at(index_++);
    key = pair.first;
    value = pair.second;
    return true;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(values_);
    PairIterable<String, CSSStyleValueVector>::IterationSource::Trace(visitor);
  }

 private:
  wtf_size_t index_;
  const HeapVector<StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry>
      values_;
};

}  // namespace

CSSStyleValue* StylePropertyMapReadOnlyMainThread::get(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  base::Optional<CSSPropertyName> name = CSSPropertyName::From(property_name);

  if (!name) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return nullptr;
  }

  const CSSProperty& property = CSSProperty::Get(name->Id());
  if (property.IsShorthand())
    return GetShorthandProperty(property);

  const CSSValue* value =
      (name->IsCustomProperty())
          ? GetCustomProperty(*execution_context, name->ToAtomicString())
          : GetProperty(name->Id());
  if (!value)
    return nullptr;

  // Custom properties count as repeated whenever we have a CSSValueList.
  if (property.IsRepeated() ||
      (name->IsCustomProperty() && value->IsValueList())) {
    CSSStyleValueVector values =
        StyleValueFactory::CssValueToStyleValueVector(*name, *value);
    return values.IsEmpty() ? nullptr : values[0];
  }

  return StyleValueFactory::CssValueToStyleValue(*name, *value);
}

CSSStyleValueVector StylePropertyMapReadOnlyMainThread::getAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  base::Optional<CSSPropertyName> name = CSSPropertyName::From(property_name);

  if (!name) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return CSSStyleValueVector();
  }

  const CSSProperty& property = CSSProperty::Get(name->Id());
  if (property.IsShorthand()) {
    CSSStyleValueVector values;
    if (CSSStyleValue* value = GetShorthandProperty(property))
      values.push_back(value);
    return values;
  }

  const CSSValue* value =
      (name->IsCustomProperty())
          ? GetCustomProperty(*execution_context, name->ToAtomicString())
          : GetProperty(name->Id());
  if (!value)
    return CSSStyleValueVector();

  return StyleValueFactory::CssValueToStyleValueVector(*name, *value);
}

bool StylePropertyMapReadOnlyMainThread::has(
    const ExecutionContext* execution_context,
    const String& property_name,
    ExceptionState& exception_state) const {
  return !getAll(execution_context, property_name, exception_state).IsEmpty();
}

const CSSValue* StylePropertyMapReadOnlyMainThread::GetCustomProperty(
    const ExecutionContext& execution_context,
    const AtomicString& property_name) const {
  const CSSValue* value = GetCustomProperty(property_name);

  const auto* document = DynamicTo<Document>(execution_context);
  if (!document)
    return value;

  return PropertyRegistry::ParseIfRegistered(*document, property_name, value);
}

StylePropertyMapReadOnlyMainThread::IterationSource*
StylePropertyMapReadOnlyMainThread::StartIteration(ScriptState* script_state,
                                                   ExceptionState&) {
  HeapVector<StylePropertyMapReadOnlyMainThread::StylePropertyMapEntry> result;

  const ExecutionContext& execution_context =
      *ExecutionContext::From(script_state);

  ForEachProperty([&result, &execution_context](const CSSPropertyName& name,
                                                const CSSValue& css_value) {
    const CSSValue* value = &css_value;

    // TODO(andruud): Refactor this. ForEachProperty should yield the correct,
    // already-parsed value in the first place.
    if (name.IsCustomProperty()) {
      const auto* document = DynamicTo<Document>(execution_context);
      if (document) {
        value = PropertyRegistry::ParseIfRegistered(
            *document, name.ToAtomicString(), value);
      }
    }

    auto values = StyleValueFactory::CssValueToStyleValueVector(name, *value);
    result.emplace_back(name.ToAtomicString(), std::move(values));
  });

  return MakeGarbageCollected<StylePropertyMapIterationSource>(result);
}

CSSStyleValue* StylePropertyMapReadOnlyMainThread::GetShorthandProperty(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  const auto serialization = SerializationForShorthand(property);
  if (serialization.IsEmpty())
    return nullptr;
  return CSSUnsupportedStyleValue::Create(
      CSSPropertyName(property.PropertyID()), serialization);
}

}  // namespace blink
