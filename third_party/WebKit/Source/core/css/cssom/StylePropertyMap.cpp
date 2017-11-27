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

const CSSValue* StyleValueToCSSValue(CSSPropertyID property_id,
                                     const CSSStyleValue& style_value,
                                     SecureContextMode secure_context_mode) {
  if (!CSSOMTypes::PropertyCanTake(property_id, style_value))
    return nullptr;
  return style_value.ToCSSValueWithProperty(property_id, secure_context_mode);
}

const CSSValue* CoerceStyleValueOrStringToCSSValue(
    CSSPropertyID property_id,
    const CSSStyleValueOrString& value,
    const CSSParserContext* parser_context) {
  if (value.IsCSSStyleValue()) {
    if (!value.GetAsCSSStyleValue())
      return nullptr;

    return StyleValueToCSSValue(property_id, *value.GetAsCSSStyleValue(),
                                parser_context->GetSecureContextMode());
  }

  const auto values = StyleValueFactory::FromString(
      property_id, value.GetAsString(), parser_context);
  // TODO(https://github.com/w3c/css-houdini-drafts/issues/512):
  // What is the correct behaviour here?
  if (values.size() != 1)
    return nullptr;
  return StyleValueToCSSValue(property_id, *values[0],
                              parser_context->GetSecureContextMode());
}

}  // namespace

void StylePropertyMap::set(const ExecutionContext* execution_context,
                           const String& property_name,
                           const HeapVector<CSSStyleValueOrString>& values,
                           ExceptionState& exception_state) {
  if (values.IsEmpty())
    return;

  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid || property_id == CSSPropertyVariable) {
    // TODO(meade): Handle custom properties here.
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  if (CSSProperty::Get(property_id).IsRepeated()) {
    CSSValueList* result = CssValueListForPropertyID(property_id);
    for (const auto& value : values) {
      const CSSValue* css_value = CoerceStyleValueOrStringToCSSValue(
          property_id, value, CSSParserContext::Create(*execution_context));
      if (!css_value || (css_value->IsCSSWideKeyword() && values.size() > 1)) {
        exception_state.ThrowTypeError("Invalid type for property");
        return;
      }
      result->Append(*css_value);
    }

    if (result->length() == 1 && result->Item(0).IsCSSWideKeyword())
      SetProperty(property_id, &result->Item(0));
    else
      SetProperty(property_id, result);
  } else {
    if (values.size() != 1) {
      // FIXME: Is this actually the correct behaviour?
      exception_state.ThrowTypeError("Not supported");
      return;
    }

    const CSSValue* result = CoerceStyleValueOrStringToCSSValue(
        property_id, values[0], CSSParserContext::Create(*execution_context));
    if (!result) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }

    SetProperty(property_id, result);
  }
}

void StylePropertyMap::append(const ExecutionContext* execution_context,
                              const String& property_name,
                              const HeapVector<CSSStyleValueOrString>& values,
                              ExceptionState& exception_state) {
  if (values.IsEmpty())
    return;

  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid || property_id == CSSPropertyVariable) {
    // TODO(meade): Handle custom properties here.
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
    return;
  }

  if (!CSSProperty::Get(property_id).IsRepeated()) {
    exception_state.ThrowTypeError("Property does not support multiple values");
    return;
  }

  const CSSValue* css_value = GetProperty(property_id);

  CSSValueList* css_value_list = nullptr;
  if (!css_value) {
    css_value_list = CssValueListForPropertyID(property_id);
  } else if (css_value->IsValueList()) {
    css_value_list = ToCSSValueList(css_value)->Copy();
  } else {
    // TODO(meade): Figure out what the correct behaviour here is.
    NOTREACHED();
    exception_state.ThrowTypeError("Property is not already list valued");
    return;
  }

  for (auto& value : values) {
    const CSSValue* css_value = CoerceStyleValueOrStringToCSSValue(
        property_id, value, CSSParserContext::Create(*execution_context));
    if (!css_value) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }
    css_value_list->Append(*css_value);
  }

  SetProperty(property_id, css_value_list);
}

void StylePropertyMap::remove(const String& property_name,
                              ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid || property_id == CSSPropertyVariable) {
    // TODO(meade): Handle custom properties here.
    exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
  }

  RemoveProperty(property_id);
}

}  // namespace blink
