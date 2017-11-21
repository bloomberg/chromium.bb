// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/InlineStylePropertyMap.h"

#include "bindings/core/v8/Iterable.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/properties/CSSProperty.h"

#include "core/css/cssom/CSSKeywordValue.h"
namespace blink {

namespace {

CSSValueList* CssValueListForPropertyID(CSSPropertyID property_id) {
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
    SecureContextMode secure_context_mode) {
  if (value.IsCSSStyleValue()) {
    if (!value.GetAsCSSStyleValue())
      return nullptr;

    return StyleValueToCSSValue(property_id, *value.GetAsCSSStyleValue(),
                                secure_context_mode);
  }

  DCHECK(value.IsString());
  const auto values = StyleValueFactory::FromString(
      property_id, value.GetAsString(), secure_context_mode);
  // TODO(785132): What should we do here?
  if (values.size() != 1)
    return nullptr;
  return StyleValueToCSSValue(property_id, *values[0], secure_context_mode);
}

}  // namespace

CSSStyleValueVector InlineStylePropertyMap::GetAllInternal(
    CSSPropertyID property_id) {
  const CSSValue* css_value =
      owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
          property_id);
  if (!css_value)
    return CSSStyleValueVector();

  return StyleValueFactory::CssValueToStyleValueVector(property_id, *css_value);
}

CSSStyleValueVector InlineStylePropertyMap::GetAllInternal(
    AtomicString custom_property_name) {
  const CSSValue* css_value =
      owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
          custom_property_name);
  if (!css_value)
    return CSSStyleValueVector();

  return StyleValueFactory::CssValueToStyleValueVector(CSSPropertyInvalid,
                                                       *css_value);
}

Vector<String> InlineStylePropertyMap::getProperties() {
  Vector<String> result;
  CSSPropertyValueSet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (unsigned i = 0; i < inline_style_set.PropertyCount(); i++) {
    CSSPropertyID property_id = inline_style_set.PropertyAt(i).Id();
    if (property_id == CSSPropertyVariable) {
      CSSPropertyValueSet::PropertyReference property_reference =
          inline_style_set.PropertyAt(i);
      const CSSCustomPropertyDeclaration& custom_declaration =
          ToCSSCustomPropertyDeclaration(property_reference.Value());
      result.push_back(custom_declaration.GetName());
    } else {
      result.push_back(getPropertyNameString(property_id));
    }
  }
  return result;
}

void InlineStylePropertyMap::set(const ExecutionContext* execution_context,
                                 CSSPropertyID property_id,
                                 HeapVector<CSSStyleValueOrString>& values,
                                 ExceptionState& exception_state) {
  if (values.IsEmpty())
    return;

  if (CSSProperty::Get(property_id).IsRepeated()) {
    CSSValueList* result = CssValueListForPropertyID(property_id);
    for (const auto& value : values) {
      const CSSValue* css_value = CoerceStyleValueOrStringToCSSValue(
          property_id, value, execution_context->SecureContextMode());
      if (!css_value || (css_value->IsCSSWideKeyword() && values.size() > 1)) {
        exception_state.ThrowTypeError("Invalid type for property");
        return;
      }
      result->Append(*css_value);
    }

    if (result->length() == 1 && result->Item(0).IsCSSWideKeyword())
      owner_element_->SetInlineStyleProperty(property_id, &result->Item(0));
    else
      owner_element_->SetInlineStyleProperty(property_id, result);
  } else {
    if (values.size() != 1) {
      // FIXME: Is this actually the correct behaviour?
      exception_state.ThrowTypeError("Not supported");
      return;
    }

    const CSSValue* result = CoerceStyleValueOrStringToCSSValue(
        property_id, values[0], execution_context->SecureContextMode());
    if (!result) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }

    owner_element_->SetInlineStyleProperty(property_id, result);
  }
}

void InlineStylePropertyMap::append(const ExecutionContext* execution_context,
                                    CSSPropertyID property_id,
                                    HeapVector<CSSStyleValueOrString>& values,
                                    ExceptionState& exception_state) {
  if (!CSSProperty::Get(property_id).IsRepeated()) {
    exception_state.ThrowTypeError("Property does not support multiple values");
    return;
  }

  const CSSValue* css_value =
      owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
          property_id);
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
        property_id, value, execution_context->SecureContextMode());
    if (!css_value) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }
    css_value_list->Append(*css_value);
  }

  owner_element_->SetInlineStyleProperty(property_id, css_value_list);
}

void InlineStylePropertyMap::remove(CSSPropertyID property_id,
                                    ExceptionState& exception_state) {
  owner_element_->RemoveInlineStyleProperty(property_id);
}

HeapVector<StylePropertyMap::StylePropertyMapEntry>
InlineStylePropertyMap::GetIterationEntries() {
  // TODO(779841): Needs to be sorted.
  HeapVector<StylePropertyMap::StylePropertyMapEntry> result;
  CSSPropertyValueSet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (unsigned i = 0; i < inline_style_set.PropertyCount(); i++) {
    CSSPropertyValueSet::PropertyReference property_reference =
        inline_style_set.PropertyAt(i);
    CSSPropertyID property_id = property_reference.Id();
    String name;
    CSSStyleValueOrCSSStyleValueSequence value;
    if (property_id == CSSPropertyVariable) {
      const CSSCustomPropertyDeclaration& custom_declaration =
          ToCSSCustomPropertyDeclaration(property_reference.Value());
      name = custom_declaration.GetName();
      // TODO(meade): Eventually custom properties will support other types, so
      // actually return them instead of always returning a
      // CSSUnsupportedStyleValue.
      // TODO(779477): Should these return CSSUnparsedValues?
      value.SetCSSStyleValue(
          CSSUnsupportedStyleValue::Create(custom_declaration.CustomCSSText()));
    } else {
      name = getPropertyNameString(property_id);
      CSSStyleValueVector style_value_vector =
          StyleValueFactory::CssValueToStyleValueVector(
              property_id, property_reference.Value());
      if (style_value_vector.size() == 1)
        value.SetCSSStyleValue(style_value_vector[0]);
      else
        value.SetCSSStyleValueSequence(style_value_vector);
    }
    result.push_back(std::make_pair(name, value));
  }
  return result;
}

}  // namespace blink
