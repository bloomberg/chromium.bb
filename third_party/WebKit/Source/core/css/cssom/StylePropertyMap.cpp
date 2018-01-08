// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSProperty.h"

namespace blink {

namespace {

CSSValueList* CssValueListForPropertyID(CSSPropertyID property_id) {
  DCHECK(CSSProperty::Get(property_id).IsRepeated());
  char separator = CSSProperty::Get(property_id).RepetitionSeparator();
  switch (separator) {
    case ' ':
      return CSSValueList::CreateSpaceSeparated();
    case ',':
      return CSSValueList::CreateCommaSeparated();
    case '/':
      return CSSValueList::CreateSlashSeparated();
    default:
      NOTREACHED();
      return nullptr;
  }
}

const CSSValue* StyleValueToCSSValue(const CSSProperty& property,
                                     const CSSStyleValue& style_value) {
  const CSSPropertyID property_id = property.PropertyID();
  if (!CSSOMTypes::PropertyCanTake(property_id, style_value))
    return nullptr;
  return style_value.ToCSSValueWithProperty(property_id);
}

const CSSValue* CoerceStyleValueOrString(
    const CSSProperty& property,
    const CSSStyleValueOrString& value,
    const ExecutionContext& execution_context) {
  DCHECK(!property.IsRepeated());

  if (value.IsCSSStyleValue()) {
    if (!value.GetAsCSSStyleValue())
      return nullptr;

    return StyleValueToCSSValue(property, *value.GetAsCSSStyleValue());
  } else {
    DCHECK(value.IsString());
    const auto values = StyleValueFactory::FromString(
        property.PropertyID(), value.GetAsString(),
        CSSParserContext::Create(execution_context));
    if (values.size() != 1U)
      return nullptr;

    return StyleValueToCSSValue(property, *values[0]);
  }
}

const CSSValue* CoerceStyleValuesOrStrings(
    const CSSProperty& property,
    const HeapVector<CSSStyleValueOrString>& values,
    const ExecutionContext& execution_context) {
  DCHECK(property.IsRepeated());
  if (values.IsEmpty())
    return nullptr;

  const CSSParserContext* parser_context = nullptr;

  HeapVector<Member<const CSSValue>> css_values;
  for (const auto& value : values) {
    if (value.IsCSSStyleValue()) {
      if (!value.GetAsCSSStyleValue())
        return nullptr;

      css_values.push_back(
          StyleValueToCSSValue(property, *value.GetAsCSSStyleValue()));
    } else {
      DCHECK(value.IsString());
      if (!parser_context)
        parser_context = CSSParserContext::Create(execution_context);

      const auto subvalues = StyleValueFactory::FromString(
          property.PropertyID(), value.GetAsString(), parser_context);
      if (subvalues.IsEmpty())
        return nullptr;

      for (const auto& subvalue : subvalues) {
        DCHECK(subvalue);
        css_values.push_back(StyleValueToCSSValue(property, *subvalue));
      }
    }
  }

  CSSValueList* result = CssValueListForPropertyID(property.PropertyID());
  for (const auto& css_value : css_values) {
    if (!css_value)
      return nullptr;
    if (css_value->IsCSSWideKeyword())
      return css_values.size() == 1U ? css_value : nullptr;

    result->Append(*css_value);
  }

  return result;
}

}  // namespace

void StylePropertyMap::set(const ExecutionContext* execution_context,
                           const String& property_name,
                           const HeapVector<CSSStyleValueOrString>& values,
                           ExceptionState& exception_state) {
  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  const CSSProperty& property = CSSProperty::Get(property_id);

  const CSSValue* result = nullptr;
  if (property.IsRepeated())
    result = CoerceStyleValuesOrStrings(property, values, *execution_context);
  else if (values.size() == 1U)
    result = CoerceStyleValueOrString(property, values[0], *execution_context);

  if (!result) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  if (property_id == CSSPropertyVariable)
    SetCustomProperty(AtomicString(property_name), *result);
  else
    SetProperty(property_id, *result);
}

void StylePropertyMap::append(const ExecutionContext* execution_context,
                              const String& property_name,
                              const HeapVector<CSSStyleValueOrString>& values,
                              ExceptionState& exception_state) {
  if (values.IsEmpty())
    return;

  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  if (property_id == CSSPropertyVariable) {
    exception_state.ThrowTypeError(
        "Appending to custom properties is not supported");
    return;
  }

  const CSSProperty& property = CSSProperty::Get(property_id);
  if (!property.IsRepeated()) {
    exception_state.ThrowTypeError("Property does not support multiple values");
    return;
  }

  CSSValueList* current_value = nullptr;
  if (const CSSValue* css_value = GetProperty(property_id)) {
    DCHECK(css_value->IsValueList());
    current_value = ToCSSValueList(css_value)->Copy();
  } else {
    current_value = CssValueListForPropertyID(property_id);
  }

  const CSSValue* result =
      CoerceStyleValuesOrStrings(property, values, *execution_context);
  if (!result) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  DCHECK(result->IsValueList());
  for (const auto& value : *ToCSSValueList(result)) {
    current_value->Append(*value);
  }

  SetProperty(property_id, *current_value);
}

void StylePropertyMap::remove(const String& property_name,
                              ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid property name: " + property_name);
    return;
  }

  if (property_id == CSSPropertyVariable) {
    RemoveCustomProperty(AtomicString(property_name));
  } else {
    RemoveProperty(property_id);
  }
}

void StylePropertyMap::update(const String& property_name,
                              V8UpdateFunction* update_function,
                              ExceptionState& exception_state) {
  CSSStyleValue* old_value = get(property_name, exception_state);
  if (exception_state.HadException()) {
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  const CSSPropertyID property_id = cssPropertyID(property_name);

  const auto& new_value = update_function->Invoke(this, old_value);
  if (new_value.IsNothing() || !new_value.ToChecked()) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  const CSSValue* result = StyleValueToCSSValue(CSSProperty::Get(property_id),
                                                *new_value.ToChecked());
  if (!result) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }

  if (property_id == CSSPropertyVariable)
    SetCustomProperty(AtomicString(property_name), *result);
  else
    SetProperty(property_id, *result);
}

}  // namespace blink
