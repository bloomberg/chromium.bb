// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/AtRuleCSSStyleDeclaration.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleAttributeMutationScope.h"

#include <iostream>

namespace blink {

String AtRuleCSSStyleDeclaration::cssText() const {
  return descriptor_value_set_->AsText();
}

void AtRuleCSSStyleDeclaration::setCSSText(
    const ExecutionContext* execution_context,
    const String& new_text,
    ExceptionState& exception_state) {
  StyleAttributeMutationScope mutation_scope(this);
  if (parent_rule_->parentStyleSheet())
    parent_rule_->parentStyleSheet()->WillMutateRules();

  // A null execution_context may be passed in by the inspector.
  const SecureContextMode mode = execution_context
                                     ? execution_context->GetSecureContextMode()
                                     : SecureContextMode::kInsecureContext;
  descriptor_value_set_->ParseDeclarationList(new_text, mode);

  // Style sheet mutation needs to be signaled even if the change failed.
  // willMutateRules/didMutateRules must pair.
  if (parent_rule_->parentStyleSheet())
    parent_rule_->parentStyleSheet()->DidMutateRules();

  mutation_scope.EnqueueMutationRecord();
}

unsigned AtRuleCSSStyleDeclaration::length() const {
  return descriptor_value_set_->Length();
}

String AtRuleCSSStyleDeclaration::item(unsigned index) const {
  return descriptor_value_set_->DescriptorAt(index);
}

String AtRuleCSSStyleDeclaration::getPropertyValue(
    const String& property_name) {
  AtRuleDescriptorID id = AsAtRuleDescriptorID(property_name);
  if (id == AtRuleDescriptorID::Invalid)
    return g_empty_string;

  const CSSValue* value = descriptor_value_set_->GetPropertyCSSValue(id);
  if (value)
    return value->CssText();

  return g_empty_string;
}

String AtRuleCSSStyleDeclaration::getPropertyPriority(
    const String& property_name) {
  return g_empty_string;
}

String AtRuleCSSStyleDeclaration::GetPropertyShorthand(
    const String& property_name) {
  return g_empty_string;
}

bool AtRuleCSSStyleDeclaration::IsPropertyImplicit(
    const String& property_name) {
  return false;
}

void AtRuleCSSStyleDeclaration::setProperty(const ExecutionContext* context,
                                            const String& property_name,
                                            const String& value,
                                            const String& priority,
                                            ExceptionState& exception_state) {
  AtRuleDescriptorID id = AsAtRuleDescriptorID(property_name);
  if (id == AtRuleDescriptorID::Invalid)
    return;

  SetPropertyInternal(id, value, context->GetSecureContextMode());
}

String AtRuleCSSStyleDeclaration::removeProperty(
    const String& property_name,
    ExceptionState& exception_state) {
  AtRuleDescriptorID id = AsAtRuleDescriptorID(property_name);
  if (id == AtRuleDescriptorID::Invalid)
    return g_empty_string;
  const CSSValue* value = descriptor_value_set_->GetPropertyCSSValue(id);
  descriptor_value_set_->RemoveProperty(id);
  return value ? value->CssText() : g_empty_string;
}

void AtRuleCSSStyleDeclaration::Reattach(AtRuleDescriptorValueSet& set) {
  descriptor_value_set_ = set;
}

const CSSValue* AtRuleCSSStyleDeclaration::GetPropertyCSSValueInternal(
    CSSPropertyID property_id) {
  AtRuleDescriptorID id = CSSPropertyIDAsAtRuleDescriptor(property_id);
  DCHECK_NE(id, AtRuleDescriptorID::Invalid);
  return descriptor_value_set_->GetPropertyCSSValue(id);
}

String AtRuleCSSStyleDeclaration::GetPropertyValueInternal(
    CSSPropertyID property_id) {
  AtRuleDescriptorID id = CSSPropertyIDAsAtRuleDescriptor(property_id);
  if (id == AtRuleDescriptorID::Invalid)
    return g_empty_string;
  const CSSValue* value = descriptor_value_set_->GetPropertyCSSValue(id);
  if (value)
    return value->CssText();
  return g_empty_string;
}

void AtRuleCSSStyleDeclaration::SetPropertyInternal(
    CSSPropertyID property_id,
    const String& custom_property_name,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
    ExceptionState& exception_state) {
  AtRuleDescriptorID id = CSSPropertyIDAsAtRuleDescriptor(property_id);
  if (id == AtRuleDescriptorID::Invalid)
    return;
  SetPropertyInternal(id, value, secure_context_mode);
}

void AtRuleCSSStyleDeclaration::SetPropertyInternal(AtRuleDescriptorID id,
                                                    const String& value,
                                                    SecureContextMode mode) {
  StyleAttributeMutationScope mutation_scope(this);
  bool did_change = false;

  if (parent_rule_ && parent_rule_->parentStyleSheet())
    parent_rule_->parentStyleSheet()->WillMutateRules();

  descriptor_value_set_->SetProperty(id, value, mode);

  if (parent_rule_ && parent_rule_->parentStyleSheet())
    parent_rule_->parentStyleSheet()->DidMutateRules();
  if (!did_change)
    return;

  mutation_scope.EnqueueMutationRecord();
}

void AtRuleCSSStyleDeclaration::Trace(blink::Visitor* visitor) {
  visitor->Trace(descriptor_value_set_);
  visitor->Trace(parent_rule_);
  CSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
