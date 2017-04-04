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

AccessibleNode::AccessibleNode(Element* element) : m_element(element) {
  DCHECK(RuntimeEnabledFeatures::accessibilityObjectModelEnabled());
}

AccessibleNode::~AccessibleNode() {}

// static
const AtomicString& AccessibleNode::getProperty(Element* element,
                                                AOMStringProperty property) {
  if (!element)
    return nullAtom;

  if (AccessibleNode* accessibleNode = element->existingAccessibleNode()) {
    for (const auto& item : accessibleNode->m_stringProperties) {
      if (item.first == property)
        return item.second;
    }
  }

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
  return nullAtom;
}

AtomicString AccessibleNode::autocomplete() const {
  return getProperty(m_element, AOMStringProperty::kAutocomplete);
}

void AccessibleNode::setAutocomplete(const AtomicString& autocomplete) {
  setStringProperty(AOMStringProperty::kAutocomplete, autocomplete);
  notifyAttributeChanged(aria_autocompleteAttr);
}

AtomicString AccessibleNode::checked() const {
  return getProperty(m_element, AOMStringProperty::kChecked);
}

void AccessibleNode::setChecked(const AtomicString& checked) {
  setStringProperty(AOMStringProperty::kChecked, checked);
  notifyAttributeChanged(aria_checkedAttr);
}

AtomicString AccessibleNode::current() const {
  return getProperty(m_element, AOMStringProperty::kCurrent);
}

void AccessibleNode::setCurrent(const AtomicString& current) {
  setStringProperty(AOMStringProperty::kCurrent, current);

  if (AXObjectCache* cache = m_element->document().existingAXObjectCache())
    cache->handleAttributeChanged(aria_currentAttr, m_element);
}

AtomicString AccessibleNode::invalid() const {
  return getProperty(m_element, AOMStringProperty::kInvalid);
}

void AccessibleNode::setInvalid(const AtomicString& invalid) {
  setStringProperty(AOMStringProperty::kInvalid, invalid);
  notifyAttributeChanged(aria_invalidAttr);
}

AtomicString AccessibleNode::keyShortcuts() const {
  return getProperty(m_element, AOMStringProperty::kKeyShortcuts);
}

void AccessibleNode::setKeyShortcuts(const AtomicString& keyShortcuts) {
  setStringProperty(AOMStringProperty::kKeyShortcuts, keyShortcuts);
  notifyAttributeChanged(aria_keyshortcutsAttr);
}

AtomicString AccessibleNode::label() const {
  return getProperty(m_element, AOMStringProperty::kLabel);
}

void AccessibleNode::setLabel(const AtomicString& label) {
  setStringProperty(AOMStringProperty::kLabel, label);
  notifyAttributeChanged(aria_labelAttr);
}

AtomicString AccessibleNode::live() const {
  return getProperty(m_element, AOMStringProperty::kLive);
}

void AccessibleNode::setLive(const AtomicString& live) {
  setStringProperty(AOMStringProperty::kLive, live);
  notifyAttributeChanged(aria_liveAttr);
}

AtomicString AccessibleNode::orientation() const {
  return getProperty(m_element, AOMStringProperty::kOrientation);
}

void AccessibleNode::setOrientation(const AtomicString& orientation) {
  setStringProperty(AOMStringProperty::kOrientation, orientation);
  notifyAttributeChanged(aria_orientationAttr);
}

AtomicString AccessibleNode::placeholder() const {
  return getProperty(m_element, AOMStringProperty::kPlaceholder);
}

void AccessibleNode::setPlaceholder(const AtomicString& placeholder) {
  setStringProperty(AOMStringProperty::kPlaceholder, placeholder);
  notifyAttributeChanged(aria_placeholderAttr);
}

AtomicString AccessibleNode::relevant() const {
  return getProperty(m_element, AOMStringProperty::kRelevant);
}

void AccessibleNode::setRelevant(const AtomicString& relevant) {
  setStringProperty(AOMStringProperty::kRelevant, relevant);
  notifyAttributeChanged(aria_relevantAttr);
}

AtomicString AccessibleNode::role() const {
  return getProperty(m_element, AOMStringProperty::kRole);
}

void AccessibleNode::setRole(const AtomicString& role) {
  setStringProperty(AOMStringProperty::kRole, role);
  notifyAttributeChanged(roleAttr);
}

AtomicString AccessibleNode::roleDescription() const {
  return getProperty(m_element, AOMStringProperty::kRoleDescription);
}

void AccessibleNode::setRoleDescription(const AtomicString& roleDescription) {
  setStringProperty(AOMStringProperty::kRoleDescription, roleDescription);
  notifyAttributeChanged(aria_roledescriptionAttr);
}

AtomicString AccessibleNode::sort() const {
  return getProperty(m_element, AOMStringProperty::kSort);
}

void AccessibleNode::setSort(const AtomicString& sort) {
  setStringProperty(AOMStringProperty::kSort, sort);
  notifyAttributeChanged(aria_sortAttr);
}

AtomicString AccessibleNode::valueText() const {
  return getProperty(m_element, AOMStringProperty::kValueText);
}

void AccessibleNode::setValueText(const AtomicString& valueText) {
  setStringProperty(AOMStringProperty::kValueText, valueText);
  notifyAttributeChanged(aria_valuetextAttr);
}

void AccessibleNode::setStringProperty(AOMStringProperty property,
                                       const AtomicString& value) {
  for (auto& item : m_stringProperties) {
    if (item.first == property) {
      item.second = value;
      return;
    }
  }

  m_stringProperties.push_back(std::make_pair(property, value));
}

void AccessibleNode::notifyAttributeChanged(
    const blink::QualifiedName& attribute) {
  // TODO(dmazzoni): Make a cleaner API for this rather than pretending
  // the DOM attribute changed.
  if (AXObjectCache* cache = getAXObjectCache())
    cache->handleAttributeChanged(attribute, m_element);
}

AXObjectCache* AccessibleNode::getAXObjectCache() {
  return m_element->document().existingAXObjectCache();
}

DEFINE_TRACE(AccessibleNode) {
  visitor->trace(m_element);
}

}  // namespace blink
