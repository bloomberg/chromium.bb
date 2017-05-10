// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AccessibleNode.h"

#include "core/dom/AXObjectCache.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "core/frame/Settings.h"

namespace blink {

using namespace HTMLNames;

AccessibleNode::AccessibleNode(Element* element) : element_(element) {
  DCHECK(RuntimeEnabledFeatures::accessibilityObjectModelEnabled());
}

AccessibleNode::~AccessibleNode() {}

// static
const AtomicString& AccessibleNode::GetProperty(Element* element,
                                                AOMStringProperty property) {
  if (!element)
    return g_null_atom;

  if (AccessibleNode* accessible_node = element->ExistingAccessibleNode()) {
    for (const auto& item : accessible_node->string_properties_) {
      if (item.first == property)
        return item.second;
    }
  }

  return g_null_atom;
}

// static
const AtomicString& AccessibleNode::GetPropertyOrARIAAttribute(
    Element* element,
    AOMStringProperty property) {
  if (!element)
    return g_null_atom;

  const AtomicString& result = GetProperty(element, property);
  if (!result.IsNull())
    return result;

  // Fall back on the equivalent ARIA attribute.
  switch (property) {
    case AOMStringProperty::kAutocomplete:
      return element->getAttribute(aria_autocompleteAttr);
    case AOMStringProperty::kChecked:
      return element->getAttribute(aria_checkedAttr);
    case AOMStringProperty::kCurrent:
      return element->getAttribute(aria_currentAttr);
    case AOMStringProperty::kInvalid:
      return element->getAttribute(aria_invalidAttr);
    case AOMStringProperty::kKeyShortcuts:
      return element->getAttribute(aria_keyshortcutsAttr);
    case AOMStringProperty::kLabel:
      return element->getAttribute(aria_labelAttr);
    case AOMStringProperty::kLive:
      return element->getAttribute(aria_liveAttr);
    case AOMStringProperty::kOrientation:
      return element->getAttribute(aria_orientationAttr);
    case AOMStringProperty::kPlaceholder:
      return element->getAttribute(aria_placeholderAttr);
    case AOMStringProperty::kRelevant:
      return element->getAttribute(aria_relevantAttr);
    case AOMStringProperty::kRole:
      return element->getAttribute(roleAttr);
    case AOMStringProperty::kRoleDescription:
      return element->getAttribute(aria_roledescriptionAttr);
    case AOMStringProperty::kSort:
      return element->getAttribute(aria_sortAttr);
    case AOMStringProperty::kValueText:
      return element->getAttribute(aria_valuetextAttr);
  }

  NOTREACHED();
  return g_null_atom;
}

AtomicString AccessibleNode::autocomplete() const {
  return GetProperty(element_, AOMStringProperty::kAutocomplete);
}

void AccessibleNode::setAutocomplete(const AtomicString& autocomplete) {
  SetStringProperty(AOMStringProperty::kAutocomplete, autocomplete);
  NotifyAttributeChanged(aria_autocompleteAttr);
}

AtomicString AccessibleNode::checked() const {
  return GetProperty(element_, AOMStringProperty::kChecked);
}

void AccessibleNode::setChecked(const AtomicString& checked) {
  SetStringProperty(AOMStringProperty::kChecked, checked);
  NotifyAttributeChanged(aria_checkedAttr);
}

AtomicString AccessibleNode::current() const {
  return GetProperty(element_, AOMStringProperty::kCurrent);
}

void AccessibleNode::setCurrent(const AtomicString& current) {
  SetStringProperty(AOMStringProperty::kCurrent, current);

  if (AXObjectCache* cache = element_->GetDocument().ExistingAXObjectCache())
    cache->HandleAttributeChanged(aria_currentAttr, element_);
}

AtomicString AccessibleNode::invalid() const {
  return GetProperty(element_, AOMStringProperty::kInvalid);
}

void AccessibleNode::setInvalid(const AtomicString& invalid) {
  SetStringProperty(AOMStringProperty::kInvalid, invalid);
  NotifyAttributeChanged(aria_invalidAttr);
}

AtomicString AccessibleNode::keyShortcuts() const {
  return GetProperty(element_, AOMStringProperty::kKeyShortcuts);
}

void AccessibleNode::setKeyShortcuts(const AtomicString& key_shortcuts) {
  SetStringProperty(AOMStringProperty::kKeyShortcuts, key_shortcuts);
  NotifyAttributeChanged(aria_keyshortcutsAttr);
}

AtomicString AccessibleNode::label() const {
  return GetProperty(element_, AOMStringProperty::kLabel);
}

void AccessibleNode::setLabel(const AtomicString& label) {
  SetStringProperty(AOMStringProperty::kLabel, label);
  NotifyAttributeChanged(aria_labelAttr);
}

AtomicString AccessibleNode::live() const {
  return GetProperty(element_, AOMStringProperty::kLive);
}

void AccessibleNode::setLive(const AtomicString& live) {
  SetStringProperty(AOMStringProperty::kLive, live);
  NotifyAttributeChanged(aria_liveAttr);
}

AtomicString AccessibleNode::orientation() const {
  return GetProperty(element_, AOMStringProperty::kOrientation);
}

void AccessibleNode::setOrientation(const AtomicString& orientation) {
  SetStringProperty(AOMStringProperty::kOrientation, orientation);
  NotifyAttributeChanged(aria_orientationAttr);
}

AtomicString AccessibleNode::placeholder() const {
  return GetProperty(element_, AOMStringProperty::kPlaceholder);
}

void AccessibleNode::setPlaceholder(const AtomicString& placeholder) {
  SetStringProperty(AOMStringProperty::kPlaceholder, placeholder);
  NotifyAttributeChanged(aria_placeholderAttr);
}

AtomicString AccessibleNode::relevant() const {
  return GetProperty(element_, AOMStringProperty::kRelevant);
}

void AccessibleNode::setRelevant(const AtomicString& relevant) {
  SetStringProperty(AOMStringProperty::kRelevant, relevant);
  NotifyAttributeChanged(aria_relevantAttr);
}

AtomicString AccessibleNode::role() const {
  return GetProperty(element_, AOMStringProperty::kRole);
}

void AccessibleNode::setRole(const AtomicString& role) {
  SetStringProperty(AOMStringProperty::kRole, role);
  NotifyAttributeChanged(roleAttr);
}

AtomicString AccessibleNode::roleDescription() const {
  return GetProperty(element_, AOMStringProperty::kRoleDescription);
}

void AccessibleNode::setRoleDescription(const AtomicString& role_description) {
  SetStringProperty(AOMStringProperty::kRoleDescription, role_description);
  NotifyAttributeChanged(aria_roledescriptionAttr);
}

AtomicString AccessibleNode::sort() const {
  return GetProperty(element_, AOMStringProperty::kSort);
}

void AccessibleNode::setSort(const AtomicString& sort) {
  SetStringProperty(AOMStringProperty::kSort, sort);
  NotifyAttributeChanged(aria_sortAttr);
}

AtomicString AccessibleNode::valueText() const {
  return GetProperty(element_, AOMStringProperty::kValueText);
}

void AccessibleNode::setValueText(const AtomicString& value_text) {
  SetStringProperty(AOMStringProperty::kValueText, value_text);
  NotifyAttributeChanged(aria_valuetextAttr);
}

void AccessibleNode::SetStringProperty(AOMStringProperty property,
                                       const AtomicString& value) {
  for (auto& item : string_properties_) {
    if (item.first == property) {
      item.second = value;
      return;
    }
  }

  string_properties_.push_back(std::make_pair(property, value));
}

void AccessibleNode::NotifyAttributeChanged(
    const blink::QualifiedName& attribute) {
  // TODO(dmazzoni): Make a cleaner API for this rather than pretending
  // the DOM attribute changed.
  if (AXObjectCache* cache = GetAXObjectCache())
    cache->HandleAttributeChanged(attribute, element_);
}

AXObjectCache* AccessibleNode::GetAXObjectCache() {
  return element_->GetDocument().ExistingAXObjectCache();
}

DEFINE_TRACE(AccessibleNode) {
  visitor->Trace(element_);
}

}  // namespace blink
