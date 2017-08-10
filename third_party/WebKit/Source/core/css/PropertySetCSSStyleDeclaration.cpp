/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/PropertySetCSSStyleDeclaration.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Element.h"
#include "core/dom/MutationObserverInterestGroup.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/html/custom/CustomElement.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "core/probe/CoreProbes.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

static CustomElementDefinition* DefinitionIfStyleChangedCallback(
    Element* element) {
  CustomElementDefinition* definition =
      CustomElement::DefinitionForElement(element);
  return definition && definition->HasStyleAttributeChangedCallback()
             ? definition
             : nullptr;
}

class StyleAttributeMutationScope {
  WTF_MAKE_NONCOPYABLE(StyleAttributeMutationScope);
  STACK_ALLOCATED();

 public:
  DISABLE_CFI_PERF
  StyleAttributeMutationScope(AbstractPropertySetCSSStyleDeclaration* decl) {
    ++scope_count_;

    if (scope_count_ != 1) {
      DCHECK_EQ(current_decl_, decl);
      return;
    }

    DCHECK(!current_decl_);
    current_decl_ = decl;

    if (!current_decl_->ParentElement())
      return;

    mutation_recipients_ =
        MutationObserverInterestGroup::CreateForAttributesMutation(
            *current_decl_->ParentElement(), HTMLNames::styleAttr);
    bool should_read_old_value =
        (mutation_recipients_ && mutation_recipients_->IsOldValueRequested()) ||
        DefinitionIfStyleChangedCallback(current_decl_->ParentElement());

    if (should_read_old_value)
      old_value_ =
          current_decl_->ParentElement()->getAttribute(HTMLNames::styleAttr);

    if (mutation_recipients_) {
      AtomicString requested_old_value =
          mutation_recipients_->IsOldValueRequested() ? old_value_
                                                      : g_null_atom;
      mutation_ = MutationRecord::CreateAttributes(
          current_decl_->ParentElement(), HTMLNames::styleAttr,
          requested_old_value);
    }
  }

  DISABLE_CFI_PERF
  ~StyleAttributeMutationScope() {
    --scope_count_;
    if (scope_count_)
      return;

    if (should_deliver_) {
      if (mutation_)
        mutation_recipients_->EnqueueMutationRecord(mutation_);

      Element* element = current_decl_->ParentElement();
      if (CustomElementDefinition* definition =
              DefinitionIfStyleChangedCallback(element)) {
        definition->EnqueueAttributeChangedCallback(
            element, HTMLNames::styleAttr, old_value_,
            element->getAttribute(HTMLNames::styleAttr));
      }

      should_deliver_ = false;
    }

    // We have to clear internal state before calling Inspector's code.
    AbstractPropertySetCSSStyleDeclaration* local_copy_style_decl =
        current_decl_;
    current_decl_ = 0;

    if (!should_notify_inspector_)
      return;

    should_notify_inspector_ = false;
    if (local_copy_style_decl->ParentElement())
      probe::didInvalidateStyleAttr(local_copy_style_decl->ParentElement());
  }

  void EnqueueMutationRecord() { should_deliver_ = true; }

  void DidInvalidateStyleAttr() { should_notify_inspector_ = true; }

 private:
  static unsigned scope_count_;
  static AbstractPropertySetCSSStyleDeclaration* current_decl_;
  static bool should_notify_inspector_;
  static bool should_deliver_;

  Member<MutationObserverInterestGroup> mutation_recipients_;
  Member<MutationRecord> mutation_;
  AtomicString old_value_;
};

unsigned StyleAttributeMutationScope::scope_count_ = 0;
AbstractPropertySetCSSStyleDeclaration*
    StyleAttributeMutationScope::current_decl_ = 0;
bool StyleAttributeMutationScope::should_notify_inspector_ = false;
bool StyleAttributeMutationScope::should_deliver_ = false;

}  // namespace

DEFINE_TRACE(PropertySetCSSStyleDeclaration) {
  visitor->Trace(property_set_);
  AbstractPropertySetCSSStyleDeclaration::Trace(visitor);
}

unsigned AbstractPropertySetCSSStyleDeclaration::length() const {
  return PropertySet().PropertyCount();
}

String AbstractPropertySetCSSStyleDeclaration::item(unsigned i) const {
  if (i >= PropertySet().PropertyCount())
    return "";
  StylePropertySet::PropertyReference property = PropertySet().PropertyAt(i);
  if (property.Id() == CSSPropertyVariable)
    return ToCSSCustomPropertyDeclaration(property.Value()).GetName();
  if (property.Id() == CSSPropertyApplyAtRule)
    return "@apply";
  return getPropertyName(property.Id());
}

String AbstractPropertySetCSSStyleDeclaration::cssText() const {
  return PropertySet().AsText();
}

void AbstractPropertySetCSSStyleDeclaration::setCSSText(const String& text,
                                                        ExceptionState&) {
  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  PropertySet().ParseDeclarationList(text, ContextStyleSheet());

  DidMutate(kPropertyChanged);

  mutation_scope.EnqueueMutationRecord();
}

String AbstractPropertySetCSSStyleDeclaration::getPropertyValue(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!property_id)
    return String();
  if (property_id == CSSPropertyVariable)
    return PropertySet().GetPropertyValue(AtomicString(property_name));
  return PropertySet().GetPropertyValue(property_id);
}

String AbstractPropertySetCSSStyleDeclaration::getPropertyPriority(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!property_id)
    return String();

  bool important = false;
  if (property_id == CSSPropertyVariable)
    important = PropertySet().PropertyIsImportant(AtomicString(property_name));
  else
    important = PropertySet().PropertyIsImportant(property_id);
  return important ? "important" : "";
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyShorthand(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);

  // Custom properties don't have shorthands, so we can ignore them here.
  if (!property_id || property_id == CSSPropertyVariable)
    return String();
  if (isShorthandProperty(property_id))
    return String();
  CSSPropertyID shorthand_id = PropertySet().GetPropertyShorthand(property_id);
  if (!shorthand_id)
    return String();
  return getPropertyNameString(shorthand_id);
}

bool AbstractPropertySetCSSStyleDeclaration::IsPropertyImplicit(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);

  // Custom properties don't have shorthands, so we can ignore them here.
  if (!property_id || property_id == CSSPropertyVariable)
    return false;
  return PropertySet().IsPropertyImplicit(property_id);
}

void AbstractPropertySetCSSStyleDeclaration::setProperty(
    const String& property_name,
    const String& value,
    const String& priority,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = unresolvedCSSPropertyID(property_name);
  if (!property_id)
    return;

  bool important = DeprecatedEqualIgnoringCase(priority, "important");
  if (!important && !priority.IsEmpty())
    return;

  SetPropertyInternal(property_id, property_name, value, important,
                      exception_state);
}

String AbstractPropertySetCSSStyleDeclaration::removeProperty(
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!property_id)
    return String();

  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  String result;
  bool changed = false;
  if (property_id == CSSPropertyVariable)
    changed =
        PropertySet().RemoveProperty(AtomicString(property_name), &result);
  else
    changed = PropertySet().RemoveProperty(property_id, &result);

  DidMutate(changed ? kPropertyChanged : kNoChanges);

  if (changed)
    mutation_scope.EnqueueMutationRecord();
  return result;
}

const CSSValue*
AbstractPropertySetCSSStyleDeclaration::GetPropertyCSSValueInternal(
    CSSPropertyID property_id) {
  return PropertySet().GetPropertyCSSValue(property_id);
}

const CSSValue*
AbstractPropertySetCSSStyleDeclaration::GetPropertyCSSValueInternal(
    AtomicString custom_property_name) {
  return PropertySet().GetPropertyCSSValue(custom_property_name);
}

String AbstractPropertySetCSSStyleDeclaration::GetPropertyValueInternal(
    CSSPropertyID property_id) {
  return PropertySet().GetPropertyValue(property_id);
}

DISABLE_CFI_PERF
void AbstractPropertySetCSSStyleDeclaration::SetPropertyInternal(
    CSSPropertyID unresolved_property,
    const String& custom_property_name,
    const String& value,
    bool important,
    ExceptionState&) {
  StyleAttributeMutationScope mutation_scope(this);
  WillMutate();

  bool did_change = false;
  if (unresolved_property == CSSPropertyVariable) {
    AtomicString atomic_name(custom_property_name);

    bool is_animation_tainted = IsKeyframeStyle();
    did_change =
        PropertySet()
            .SetProperty(atomic_name, GetPropertyRegistry(), value, important,
                         ContextStyleSheet(), is_animation_tainted)
            .did_change;
  } else {
    did_change = PropertySet()
                     .SetProperty(unresolved_property, value, important,
                                  ContextStyleSheet())
                     .did_change;
  }

  DidMutate(did_change ? kPropertyChanged : kNoChanges);

  if (!did_change)
    return;

  Element* parent = ParentElement();
  if (parent)
    parent->GetDocument().GetStyleEngine().AttributeChangedForElement(
        HTMLNames::styleAttr, *parent);
  mutation_scope.EnqueueMutationRecord();
}

DISABLE_CFI_PERF
StyleSheetContents* AbstractPropertySetCSSStyleDeclaration::ContextStyleSheet()
    const {
  CSSStyleSheet* css_style_sheet = ParentStyleSheet();
  return css_style_sheet ? css_style_sheet->Contents() : nullptr;
}

bool AbstractPropertySetCSSStyleDeclaration::CssPropertyMatches(
    CSSPropertyID property_id,
    const CSSValue* property_value) const {
  return PropertySet().PropertyMatches(property_id, *property_value);
}

DEFINE_TRACE(AbstractPropertySetCSSStyleDeclaration) {
  CSSStyleDeclaration::Trace(visitor);
}

StyleRuleCSSStyleDeclaration::StyleRuleCSSStyleDeclaration(
    MutableStylePropertySet& property_set_arg,
    CSSRule* parent_rule)
    : PropertySetCSSStyleDeclaration(property_set_arg),
      parent_rule_(parent_rule) {}

StyleRuleCSSStyleDeclaration::~StyleRuleCSSStyleDeclaration() {}

void StyleRuleCSSStyleDeclaration::WillMutate() {
  if (parent_rule_ && parent_rule_->parentStyleSheet())
    parent_rule_->parentStyleSheet()->WillMutateRules();
}

void StyleRuleCSSStyleDeclaration::DidMutate(MutationType type) {
  // Style sheet mutation needs to be signaled even if the change failed.
  // willMutateRules/didMutateRules must pair.
  if (parent_rule_ && parent_rule_->parentStyleSheet())
    parent_rule_->parentStyleSheet()->DidMutateRules();
}

CSSStyleSheet* StyleRuleCSSStyleDeclaration::ParentStyleSheet() const {
  return parent_rule_ ? parent_rule_->parentStyleSheet() : nullptr;
}

void StyleRuleCSSStyleDeclaration::Reattach(
    MutableStylePropertySet& property_set) {
  property_set_ = &property_set;
}

PropertyRegistry* StyleRuleCSSStyleDeclaration::GetPropertyRegistry() const {
  CSSStyleSheet* sheet = parent_rule_->parentStyleSheet();
  if (!sheet)
    return nullptr;
  Node* node = sheet->ownerNode();
  if (!node)
    return nullptr;
  return node->GetDocument().GetPropertyRegistry();
}

DEFINE_TRACE(StyleRuleCSSStyleDeclaration) {
  visitor->Trace(parent_rule_);
  PropertySetCSSStyleDeclaration::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(StyleRuleCSSStyleDeclaration) {
  visitor->TraceWrappers(parent_rule_);
  PropertySetCSSStyleDeclaration::TraceWrappers(visitor);
}

MutableStylePropertySet& InlineCSSStyleDeclaration::PropertySet() const {
  return parent_element_->EnsureMutableInlineStyle();
}

void InlineCSSStyleDeclaration::DidMutate(MutationType type) {
  if (type == kNoChanges)
    return;

  if (!parent_element_)
    return;

  parent_element_->ClearMutableInlineStyleIfEmpty();
  parent_element_->SetNeedsStyleRecalc(
      kLocalStyleChange, StyleChangeReasonForTracing::Create(
                             StyleChangeReason::kInlineCSSStyleMutated));
  parent_element_->InvalidateStyleAttribute();
  StyleAttributeMutationScope(this).DidInvalidateStyleAttr();
}

CSSStyleSheet* InlineCSSStyleDeclaration::ParentStyleSheet() const {
  return parent_element_ ? &parent_element_->GetDocument().ElementSheet()
                         : nullptr;
}

PropertyRegistry* InlineCSSStyleDeclaration::GetPropertyRegistry() const {
  return parent_element_ ? parent_element_->GetDocument().GetPropertyRegistry()
                         : nullptr;
}

DEFINE_TRACE(InlineCSSStyleDeclaration) {
  visitor->Trace(parent_element_);
  AbstractPropertySetCSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
