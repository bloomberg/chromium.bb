// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/DeclaredStylePropertyMap.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/StyleRule.h"

namespace blink {

DeclaredStylePropertyMap::DeclaredStylePropertyMap(CSSStyleRule* owner_rule)
    : StylePropertyMap(), owner_rule_(owner_rule) {}

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

}  // namespace blink
