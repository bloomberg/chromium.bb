// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/AccessibleNode.h"

#include "core/dom/AXObjectCache.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "core/frame/Settings.h"

namespace blink {

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
    case AOMStringProperty::kRole:
      return element->getAttribute(HTMLNames::roleAttr);
    case AOMStringProperty::kLabel:
      return element->getAttribute(HTMLNames::aria_labelAttr);
    default:
      NOTREACHED();
      return nullAtom;
  }
}

AtomicString AccessibleNode::role() const {
  return getProperty(m_element, AOMStringProperty::kRole);
}

void AccessibleNode::setRole(const AtomicString& role) {
  setStringProperty(AOMStringProperty::kRole, role);

  // TODO(dmazzoni): Make a cleaner API for this rather than pretending
  // the DOM attribute changed.
  if (AXObjectCache* cache = m_element->document().axObjectCache())
    cache->handleAttributeChanged(HTMLNames::roleAttr, m_element);
}

AtomicString AccessibleNode::label() const {
  return getProperty(m_element, AOMStringProperty::kLabel);
}

void AccessibleNode::setLabel(const AtomicString& label) {
  setStringProperty(AOMStringProperty::kLabel, label);

  if (AXObjectCache* cache = m_element->document().axObjectCache())
    cache->handleAttributeChanged(HTMLNames::aria_labelAttr, m_element);
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

DEFINE_TRACE(AccessibleNode) {
  visitor->trace(m_element);
}

}  // namespace blink
