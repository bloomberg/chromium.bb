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
#include "core/css/MutableCSSPropertyValueSet.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/properties/CSSProperty.h"

namespace blink {

const CSSValue* InlineStylePropertyMap::GetProperty(CSSPropertyID property_id) {
  return owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
      property_id);
}

const CSSValue* InlineStylePropertyMap::GetCustomProperty(
    AtomicString property_name) {
  return owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
      property_name);
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

void InlineStylePropertyMap::SetProperty(CSSPropertyID property_id,
                                         const CSSValue* value) {
  owner_element_->SetInlineStyleProperty(property_id, value);
}

void InlineStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
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
