// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/InlineStylePropertyMap.h"

#include "bindings/core/v8/Iterable.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/properties/CSSPropertyAPI.h"

namespace blink {

namespace {

CSSValueList* CssValueListForPropertyID(CSSPropertyID property_id) {
  char separator = CSSPropertyAPI::Get(property_id).RepetitionSeparator();
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
                                     const CSSStyleValue& style_value) {
  if (!CSSOMTypes::PropertyCanTake(property_id, style_value))
    return nullptr;
  return style_value.ToCSSValueWithProperty(property_id);
}

const CSSValue* SingleStyleValueAsCSSValue(CSSPropertyID property_id,
                                           const CSSStyleValue& style_value) {
  const CSSValue* css_value = StyleValueToCSSValue(property_id, style_value);
  if (!css_value)
    return nullptr;

  if (!CSSPropertyAPI::Get(property_id).IsRepeated() ||
      css_value->IsCSSWideKeyword())
    return css_value;

  CSSValueList* value_list = CssValueListForPropertyID(property_id);
  value_list->Append(*css_value);
  return value_list;
}

const CSSValueList* AsCSSValueList(
    CSSPropertyID property_id,
    const CSSStyleValueVector& style_value_vector) {
  CSSValueList* value_list = CssValueListForPropertyID(property_id);
  for (const CSSStyleValue* value : style_value_vector) {
    const CSSValue* css_value = StyleValueToCSSValue(property_id, *value);
    if (!css_value) {
      return nullptr;
    }
    value_list->Append(*css_value);
  }
  return value_list;
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
  DEFINE_STATIC_LOCAL(const String, kAtApply, ("@apply"));
  Vector<String> result;
  bool contains_at_apply = false;
  StylePropertySet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (unsigned i = 0; i < inline_style_set.PropertyCount(); i++) {
    CSSPropertyID property_id = inline_style_set.PropertyAt(i).Id();
    if (property_id == CSSPropertyVariable) {
      StylePropertySet::PropertyReference property_reference =
          inline_style_set.PropertyAt(i);
      const CSSCustomPropertyDeclaration& custom_declaration =
          ToCSSCustomPropertyDeclaration(property_reference.Value());
      result.push_back(custom_declaration.GetName());
    } else if (property_id == CSSPropertyApplyAtRule) {
      if (!contains_at_apply) {
        result.push_back(kAtApply);
        contains_at_apply = true;
      }
    } else {
      result.push_back(getPropertyNameString(property_id));
    }
  }
  return result;
}

void InlineStylePropertyMap::set(
    CSSPropertyID property_id,
    CSSStyleValueOrCSSStyleValueSequenceOrString& item,
    ExceptionState& exception_state) {
  const CSSValue* css_value = nullptr;
  if (item.isCSSStyleValue()) {
    css_value =
        SingleStyleValueAsCSSValue(property_id, *item.getAsCSSStyleValue());
  } else if (item.isCSSStyleValueSequence()) {
    if (!CSSPropertyAPI::Get(property_id).IsRepeated()) {
      exception_state.ThrowTypeError(
          "Property does not support multiple values");
      return;
    }
    css_value = AsCSSValueList(property_id, item.getAsCSSStyleValueSequence());
  } else {
    // Parse it.
    DCHECK(item.isString());
    // TODO(meade): Implement this.
    exception_state.ThrowTypeError("Not implemented yet");
    return;
  }
  if (!css_value) {
    exception_state.ThrowTypeError("Invalid type for property");
    return;
  }
  owner_element_->SetInlineStyleProperty(property_id, css_value);
}

void InlineStylePropertyMap::append(
    CSSPropertyID property_id,
    CSSStyleValueOrCSSStyleValueSequenceOrString& item,
    ExceptionState& exception_state) {
  if (!CSSPropertyAPI::Get(property_id).IsRepeated()) {
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
    exception_state.ThrowTypeError("Property is not already list valued");
    return;
  }

  if (item.isCSSStyleValue()) {
    const CSSValue* css_value =
        StyleValueToCSSValue(property_id, *item.getAsCSSStyleValue());
    if (!css_value) {
      exception_state.ThrowTypeError("Invalid type for property");
      return;
    }
    css_value_list->Append(*css_value);
  } else if (item.isCSSStyleValueSequence()) {
    for (CSSStyleValue* style_value : item.getAsCSSStyleValueSequence()) {
      const CSSValue* css_value =
          StyleValueToCSSValue(property_id, *style_value);
      if (!css_value) {
        exception_state.ThrowTypeError("Invalid type for property");
        return;
      }
      css_value_list->Append(*css_value);
    }
  } else {
    // Parse it.
    DCHECK(item.isString());
    // TODO(meade): Implement this.
    exception_state.ThrowTypeError("Not implemented yet");
    return;
  }

  owner_element_->SetInlineStyleProperty(property_id, css_value_list);
}

void InlineStylePropertyMap::remove(CSSPropertyID property_id,
                                    ExceptionState& exception_state) {
  owner_element_->RemoveInlineStyleProperty(property_id);
}

HeapVector<StylePropertyMap::StylePropertyMapEntry>
InlineStylePropertyMap::GetIterationEntries() {
  DEFINE_STATIC_LOCAL(const String, kAtApply, ("@apply"));
  HeapVector<StylePropertyMap::StylePropertyMapEntry> result;
  StylePropertySet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (unsigned i = 0; i < inline_style_set.PropertyCount(); i++) {
    StylePropertySet::PropertyReference property_reference =
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
      value.setCSSStyleValue(
          CSSUnsupportedStyleValue::Create(custom_declaration.CustomCSSText()));
    } else if (property_id == CSSPropertyApplyAtRule) {
      name = kAtApply;
      value.setCSSStyleValue(CSSUnsupportedStyleValue::Create(
          ToCSSCustomIdentValue(property_reference.Value()).Value()));
    } else {
      name = getPropertyNameString(property_id);
      CSSStyleValueVector style_value_vector =
          StyleValueFactory::CssValueToStyleValueVector(
              property_id, property_reference.Value());
      if (style_value_vector.size() == 1)
        value.setCSSStyleValue(style_value_vector[0]);
      else
        value.setCSSStyleValueSequence(style_value_vector);
    }
    result.push_back(std::make_pair(name, value));
  }
  return result;
}

}  // namespace blink
