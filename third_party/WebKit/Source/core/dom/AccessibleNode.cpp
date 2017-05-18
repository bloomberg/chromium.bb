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
      if (item.first == property && !item.second.IsNull())
        return item.second;
    }
  }

  return g_null_atom;
}

// static
bool AccessibleNode::GetProperty(Element* element,
                                 AOMBooleanProperty property,
                                 bool& is_null) {
  is_null = true;
  if (!element)
    return false;

  if (AccessibleNode* accessible_node = element->ExistingAccessibleNode()) {
    for (const auto& item : accessible_node->boolean_properties_) {
      if (item.first == property) {
        is_null = false;
        return item.second;
      }
    }
  }

  return false;
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
      return element->FastGetAttribute(aria_autocompleteAttr);
    case AOMStringProperty::kChecked:
      return element->FastGetAttribute(aria_checkedAttr);
    case AOMStringProperty::kCurrent:
      return element->FastGetAttribute(aria_currentAttr);
    case AOMStringProperty::kInvalid:
      return element->FastGetAttribute(aria_invalidAttr);
    case AOMStringProperty::kKeyShortcuts:
      return element->FastGetAttribute(aria_keyshortcutsAttr);
    case AOMStringProperty::kLabel:
      return element->FastGetAttribute(aria_labelAttr);
    case AOMStringProperty::kLive:
      return element->FastGetAttribute(aria_liveAttr);
    case AOMStringProperty::kOrientation:
      return element->FastGetAttribute(aria_orientationAttr);
    case AOMStringProperty::kPlaceholder:
      return element->FastGetAttribute(aria_placeholderAttr);
    case AOMStringProperty::kRelevant:
      return element->FastGetAttribute(aria_relevantAttr);
    case AOMStringProperty::kRole:
      return element->FastGetAttribute(roleAttr);
    case AOMStringProperty::kRoleDescription:
      return element->FastGetAttribute(aria_roledescriptionAttr);
    case AOMStringProperty::kSort:
      return element->FastGetAttribute(aria_sortAttr);
    case AOMStringProperty::kValueText:
      return element->FastGetAttribute(aria_valuetextAttr);
  }

  NOTREACHED();
  return g_null_atom;
}

// static
bool AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                AOMBooleanProperty property,
                                                bool& is_null) {
  is_null = true;
  if (!element)
    return false;

  bool result = GetProperty(element, property, is_null);
  if (!is_null)
    return result;

  // Fall back on the equivalent ARIA attribute.
  AtomicString attr_value;
  switch (property) {
    case AOMBooleanProperty::kAtomic:
      attr_value = element->FastGetAttribute(aria_atomicAttr);
      break;
    case AOMBooleanProperty::kBusy:
      attr_value = element->FastGetAttribute(aria_busyAttr);
      break;
    case AOMBooleanProperty::kDisabled:
      attr_value = element->FastGetAttribute(aria_disabledAttr);
      break;
    case AOMBooleanProperty::kExpanded:
      attr_value = element->FastGetAttribute(aria_expandedAttr);
      break;
    case AOMBooleanProperty::kHidden:
      attr_value = element->FastGetAttribute(aria_hiddenAttr);
      break;
    case AOMBooleanProperty::kModal:
      attr_value = element->FastGetAttribute(aria_modalAttr);
      break;
    case AOMBooleanProperty::kMultiline:
      attr_value = element->FastGetAttribute(aria_multilineAttr);
      break;
    case AOMBooleanProperty::kMultiselectable:
      attr_value = element->FastGetAttribute(aria_multiselectableAttr);
      break;
    case AOMBooleanProperty::kReadOnly:
      attr_value = element->FastGetAttribute(aria_readonlyAttr);
      break;
    case AOMBooleanProperty::kRequired:
      attr_value = element->FastGetAttribute(aria_requiredAttr);
      break;
    case AOMBooleanProperty::kSelected:
      attr_value = element->FastGetAttribute(aria_selectedAttr);
      break;
  }

  is_null = attr_value.IsNull();
  return EqualIgnoringASCIICase(attr_value, "true");
}

bool AccessibleNode::atomic(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kAtomic, is_null);
}

void AccessibleNode::setAtomic(bool atomic, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kAtomic, atomic, is_null);
  NotifyAttributeChanged(aria_atomicAttr);
}

AtomicString AccessibleNode::autocomplete() const {
  return GetProperty(element_, AOMStringProperty::kAutocomplete);
}

void AccessibleNode::setAutocomplete(const AtomicString& autocomplete) {
  SetStringProperty(AOMStringProperty::kAutocomplete, autocomplete);
  NotifyAttributeChanged(aria_autocompleteAttr);
}

bool AccessibleNode::busy(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kBusy, is_null);
}

void AccessibleNode::setBusy(bool busy, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kBusy, busy, is_null);
  NotifyAttributeChanged(aria_busyAttr);
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

bool AccessibleNode::disabled(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kDisabled, is_null);
}

void AccessibleNode::setDisabled(bool disabled, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kDisabled, disabled, is_null);
  NotifyAttributeChanged(aria_disabledAttr);
}

bool AccessibleNode::expanded(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kExpanded, is_null);
}

void AccessibleNode::setExpanded(bool expanded, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kExpanded, expanded, is_null);
  NotifyAttributeChanged(aria_expandedAttr);
}

bool AccessibleNode::hidden(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kHidden, is_null);
}

void AccessibleNode::setHidden(bool hidden, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kHidden, hidden, is_null);
  NotifyAttributeChanged(aria_hiddenAttr);
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

bool AccessibleNode::modal(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kModal, is_null);
}

void AccessibleNode::setModal(bool modal, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kModal, modal, is_null);
  NotifyAttributeChanged(aria_modalAttr);
}

bool AccessibleNode::multiline(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kMultiline, is_null);
}

void AccessibleNode::setMultiline(bool multiline, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kMultiline, multiline, is_null);
  NotifyAttributeChanged(aria_multilineAttr);
}

bool AccessibleNode::multiselectable(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kMultiselectable, is_null);
}

void AccessibleNode::setMultiselectable(bool multiselectable, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kMultiselectable, multiselectable,
                     is_null);
  NotifyAttributeChanged(aria_multiselectableAttr);
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

bool AccessibleNode::readOnly(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kReadOnly, is_null);
}

void AccessibleNode::setReadOnly(bool read_only, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kReadOnly, read_only, is_null);
  NotifyAttributeChanged(aria_readonlyAttr);
}

AtomicString AccessibleNode::relevant() const {
  return GetProperty(element_, AOMStringProperty::kRelevant);
}

void AccessibleNode::setRelevant(const AtomicString& relevant) {
  SetStringProperty(AOMStringProperty::kRelevant, relevant);
  NotifyAttributeChanged(aria_relevantAttr);
}

bool AccessibleNode::required(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kRequired, is_null);
}

void AccessibleNode::setRequired(bool required, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kRequired, required, is_null);
  NotifyAttributeChanged(aria_requiredAttr);
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

bool AccessibleNode::selected(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kSelected, is_null);
}

void AccessibleNode::setSelected(bool selected, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kSelected, selected, is_null);
  NotifyAttributeChanged(aria_selectedAttr);
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

void AccessibleNode::SetBooleanProperty(AOMBooleanProperty property,
                                        bool value,
                                        bool is_null) {
  for (size_t i = 0; i < boolean_properties_.size(); i++) {
    auto& item = boolean_properties_[i];
    if (item.first == property) {
      if (is_null)
        boolean_properties_.erase(i);
      else
        item.second = value;
      return;
    }
  }

  boolean_properties_.push_back(std::make_pair(property, value));
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
