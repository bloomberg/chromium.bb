// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/DeclaredStylePropertyMap.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/StylePropertySerializer.h"
#include "core/css/StyleRule.h"

namespace blink {

DeclaredStylePropertyMap::DeclaredStylePropertyMap(CSSStyleRule* owner_rule)
    : StylePropertyMap(), owner_rule_(owner_rule) {}

unsigned int DeclaredStylePropertyMap::size() {
  if (!GetStyleRule())
    return 0;
  return GetStyleRule()->Properties().PropertyCount();
}

const CSSValue* DeclaredStylePropertyMap::GetProperty(
    CSSPropertyID property_id) {
  if (!GetStyleRule())
    return nullptr;
  return GetStyleRule()->Properties().GetPropertyCSSValue(property_id);
}

const CSSValue* DeclaredStylePropertyMap::GetCustomProperty(
    AtomicString property_name) {
  if (!GetStyleRule())
    return nullptr;
  return GetStyleRule()->Properties().GetPropertyCSSValue(property_name);
}

void DeclaredStylePropertyMap::SetProperty(CSSPropertyID property_id,
                                           const CSSValue& value) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().SetProperty(property_id, value);
}

bool DeclaredStylePropertyMap::SetShorthandProperty(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode) {
  DCHECK(CSSProperty::Get(property_id).IsShorthand());
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  const auto result = GetStyleRule()->MutableProperties().SetProperty(
      property_id, value, false /* important */, secure_context_mode);
  return result.did_parse;
}

void DeclaredStylePropertyMap::SetCustomProperty(
    const AtomicString& property_name,
    const CSSValue& value) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);

  DCHECK(value.IsVariableReferenceValue());
  CSSVariableData* variable_data =
      ToCSSVariableReferenceValue(value).VariableDataValue();
  GetStyleRule()->MutableProperties().SetProperty(
      CSSPropertyVariable,
      *CSSCustomPropertyDeclaration::Create(property_name, variable_data));
}

void DeclaredStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().RemoveProperty(property_id);
}

void DeclaredStylePropertyMap::RemoveCustomProperty(
    const AtomicString& property_name) {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().RemoveProperty(property_name);
}

void DeclaredStylePropertyMap::RemoveAllProperties() {
  if (!GetStyleRule())
    return;
  CSSStyleSheet::RuleMutationScope mutation_scope(owner_rule_);
  GetStyleRule()->MutableProperties().Clear();
}

void DeclaredStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  if (!GetStyleRule())
    return;
  const CSSPropertyValueSet& declared_style_set = GetStyleRule()->Properties();
  for (unsigned i = 0; i < declared_style_set.PropertyCount(); i++) {
    const auto& property_reference = declared_style_set.PropertyAt(i);
    if (property_reference.Id() == CSSPropertyVariable) {
      const auto& decl =
          ToCSSCustomPropertyDeclaration(property_reference.Value());
      callback(decl.GetName(), property_reference.Value());
    } else {
      const CSSProperty& property = CSSProperty::Get(property_reference.Id());
      callback(property.GetPropertyNameAtomicString(),
               property_reference.Value());
    }
  }
}

StyleRule* DeclaredStylePropertyMap::GetStyleRule() const {
  if (!owner_rule_)
    return nullptr;
  return owner_rule_->GetStyleRule();
}

String DeclaredStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) {
  DCHECK(property.IsShorthand());
  if (StyleRule* style_rule = GetStyleRule()) {
    return StylePropertySerializer(style_rule->Properties())
        .GetPropertyValue(property.PropertyID());
  }

  return "";
}

}  // namespace blink
