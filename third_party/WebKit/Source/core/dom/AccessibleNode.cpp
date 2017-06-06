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
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
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

template <typename P, typename T>
static T FindPropertyValue(P property,
                           bool& is_null,
                           Vector<std::pair<P, T>>& properties,
                           T default_value) {
  for (const auto& item : properties) {
    if (item.first == property) {
      is_null = false;
      return item.second;
    }
  }

  return default_value;
}

// static
bool AccessibleNode::GetProperty(Element* element,
                                 AOMBooleanProperty property,
                                 bool& is_null) {
  is_null = true;

  bool default_value = false;
  if (!element || !element->ExistingAccessibleNode())
    return default_value;

  return FindPropertyValue(
      property, is_null, element->ExistingAccessibleNode()->boolean_properties_,
      default_value);
}

// static
float AccessibleNode::GetProperty(Element* element,
                                  AOMFloatProperty property,
                                  bool& is_null) {
  is_null = true;

  float default_value = 0.0;
  if (!element || !element->ExistingAccessibleNode())
    return default_value;

  return FindPropertyValue(property, is_null,
                           element->ExistingAccessibleNode()->float_properties_,
                           default_value);
}

// static
int32_t AccessibleNode::GetProperty(Element* element,
                                    AOMIntProperty property,
                                    bool& is_null) {
  is_null = true;

  int32_t default_value = 0;
  if (!element || !element->ExistingAccessibleNode())
    return default_value;

  return FindPropertyValue(property, is_null,
                           element->ExistingAccessibleNode()->int_properties_,
                           default_value);
}

// static
uint32_t AccessibleNode::GetProperty(Element* element,
                                     AOMUIntProperty property,
                                     bool& is_null) {
  is_null = true;

  uint32_t default_value = 0;
  if (!element || !element->ExistingAccessibleNode())
    return default_value;

  return FindPropertyValue(property, is_null,
                           element->ExistingAccessibleNode()->uint_properties_,
                           default_value);
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
    case AOMStringProperty::kPressed:
      return element->FastGetAttribute(aria_pressedAttr);
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

// static
float AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                 AOMFloatProperty property,
                                                 bool& is_null) {
  is_null = true;
  if (!element)
    return 0.0;

  float result = GetProperty(element, property, is_null);
  if (!is_null)
    return result;

  // Fall back on the equivalent ARIA attribute.
  AtomicString attr_value;
  switch (property) {
    case AOMFloatProperty::kValueMax:
      attr_value = element->FastGetAttribute(aria_valuemaxAttr);
      break;
    case AOMFloatProperty::kValueMin:
      attr_value = element->FastGetAttribute(aria_valueminAttr);
      break;
    case AOMFloatProperty::kValueNow:
      attr_value = element->FastGetAttribute(aria_valuenowAttr);
      break;
  }

  is_null = attr_value.IsNull();
  return attr_value.ToFloat();
}

// static
uint32_t AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                    AOMUIntProperty property,
                                                    bool& is_null) {
  is_null = true;
  if (!element)
    return 0;

  int32_t result = GetProperty(element, property, is_null);
  if (!is_null)
    return result;

  // Fall back on the equivalent ARIA attribute.
  AtomicString attr_value;
  switch (property) {
    case AOMUIntProperty::kColIndex:
      attr_value = element->FastGetAttribute(aria_colindexAttr);
      break;
    case AOMUIntProperty::kColSpan:
      attr_value = element->FastGetAttribute(aria_colspanAttr);
      break;
    case AOMUIntProperty::kLevel:
      attr_value = element->FastGetAttribute(aria_levelAttr);
      break;
    case AOMUIntProperty::kPosInSet:
      attr_value = element->FastGetAttribute(aria_posinsetAttr);
      break;
    case AOMUIntProperty::kRowIndex:
      attr_value = element->FastGetAttribute(aria_rowindexAttr);
      break;
    case AOMUIntProperty::kRowSpan:
      attr_value = element->FastGetAttribute(aria_rowspanAttr);
      break;
  }

  is_null = attr_value.IsNull();
  return attr_value.GetString().ToUInt();
}

// static
int32_t AccessibleNode::GetPropertyOrARIAAttribute(Element* element,
                                                   AOMIntProperty property,
                                                   bool& is_null) {
  is_null = true;
  if (!element)
    return 0;

  int32_t result = GetProperty(element, property, is_null);
  if (!is_null)
    return result;

  // Fall back on the equivalent ARIA attribute.
  AtomicString attr_value;
  switch (property) {
    case AOMIntProperty::kColCount:
      attr_value = element->FastGetAttribute(aria_colcountAttr);
      break;
    case AOMIntProperty::kRowCount:
      attr_value = element->FastGetAttribute(aria_rowcountAttr);
      break;
    case AOMIntProperty::kSetSize:
      attr_value = element->FastGetAttribute(aria_setsizeAttr);
      break;
  }

  is_null = attr_value.IsNull();
  return attr_value.ToInt();
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

int32_t AccessibleNode::colCount(bool& is_null) const {
  return GetProperty(element_, AOMIntProperty::kColCount, is_null);
}

void AccessibleNode::setColCount(int32_t col_count, bool is_null) {
  SetIntProperty(AOMIntProperty::kColCount, col_count, is_null);
  NotifyAttributeChanged(aria_colcountAttr);
}

uint32_t AccessibleNode::colIndex(bool& is_null) const {
  return GetProperty(element_, AOMUIntProperty::kColIndex, is_null);
}

void AccessibleNode::setColIndex(uint32_t col_index, bool is_null) {
  SetUIntProperty(AOMUIntProperty::kColIndex, col_index, is_null);
  NotifyAttributeChanged(aria_colindexAttr);
}

uint32_t AccessibleNode::colSpan(bool& is_null) const {
  return GetProperty(element_, AOMUIntProperty::kColSpan, is_null);
}

void AccessibleNode::setColSpan(uint32_t col_span, bool is_null) {
  SetUIntProperty(AOMUIntProperty::kColSpan, col_span, is_null);
  NotifyAttributeChanged(aria_colspanAttr);
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

uint32_t AccessibleNode::level(bool& is_null) const {
  return GetProperty(element_, AOMUIntProperty::kLevel, is_null);
}

void AccessibleNode::setLevel(uint32_t level, bool is_null) {
  SetUIntProperty(AOMUIntProperty::kLevel, level, is_null);
  NotifyAttributeChanged(aria_levelAttr);
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

uint32_t AccessibleNode::posInSet(bool& is_null) const {
  return GetProperty(element_, AOMUIntProperty::kPosInSet, is_null);
}

void AccessibleNode::setPosInSet(uint32_t pos_in_set, bool is_null) {
  SetUIntProperty(AOMUIntProperty::kPosInSet, pos_in_set, is_null);
  NotifyAttributeChanged(aria_posinsetAttr);
}

AtomicString AccessibleNode::pressed() const {
  return GetProperty(element_, AOMStringProperty::kPressed);
}

void AccessibleNode::setPressed(const AtomicString& pressed) {
  SetStringProperty(AOMStringProperty::kPressed, pressed);
  NotifyAttributeChanged(aria_pressedAttr);
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

int32_t AccessibleNode::rowCount(bool& is_null) const {
  return GetProperty(element_, AOMIntProperty::kRowCount, is_null);
}

void AccessibleNode::setRowCount(int32_t row_count, bool is_null) {
  SetIntProperty(AOMIntProperty::kRowCount, row_count, is_null);
  NotifyAttributeChanged(aria_rowcountAttr);
}

uint32_t AccessibleNode::rowIndex(bool& is_null) const {
  return GetProperty(element_, AOMUIntProperty::kRowIndex, is_null);
}

void AccessibleNode::setRowIndex(uint32_t row_index, bool is_null) {
  SetUIntProperty(AOMUIntProperty::kRowIndex, row_index, is_null);
  NotifyAttributeChanged(aria_rowindexAttr);
}

uint32_t AccessibleNode::rowSpan(bool& is_null) const {
  return GetProperty(element_, AOMUIntProperty::kRowSpan, is_null);
}

void AccessibleNode::setRowSpan(uint32_t row_span, bool is_null) {
  SetUIntProperty(AOMUIntProperty::kRowSpan, row_span, is_null);
  NotifyAttributeChanged(aria_rowspanAttr);
}

bool AccessibleNode::selected(bool& is_null) const {
  return GetProperty(element_, AOMBooleanProperty::kSelected, is_null);
}

void AccessibleNode::setSelected(bool selected, bool is_null) {
  SetBooleanProperty(AOMBooleanProperty::kSelected, selected, is_null);
  NotifyAttributeChanged(aria_selectedAttr);
}

int32_t AccessibleNode::setSize(bool& is_null) const {
  return GetProperty(element_, AOMIntProperty::kSetSize, is_null);
}

void AccessibleNode::setSetSize(int32_t set_size, bool is_null) {
  SetIntProperty(AOMIntProperty::kSetSize, set_size, is_null);
  NotifyAttributeChanged(aria_setsizeAttr);
}

AtomicString AccessibleNode::sort() const {
  return GetProperty(element_, AOMStringProperty::kSort);
}

void AccessibleNode::setSort(const AtomicString& sort) {
  SetStringProperty(AOMStringProperty::kSort, sort);
  NotifyAttributeChanged(aria_sortAttr);
}

float AccessibleNode::valueMax(bool& is_null) const {
  return GetProperty(element_, AOMFloatProperty::kValueMax, is_null);
}

void AccessibleNode::setValueMax(float value_max, bool is_null) {
  SetFloatProperty(AOMFloatProperty::kValueMax, value_max, is_null);
  NotifyAttributeChanged(aria_valuemaxAttr);
}

float AccessibleNode::valueMin(bool& is_null) const {
  return GetProperty(element_, AOMFloatProperty::kValueMin, is_null);
}

void AccessibleNode::setValueMin(float value_min, bool is_null) {
  SetFloatProperty(AOMFloatProperty::kValueMin, value_min, is_null);
  NotifyAttributeChanged(aria_valueminAttr);
}

float AccessibleNode::valueNow(bool& is_null) const {
  return GetProperty(element_, AOMFloatProperty::kValueNow, is_null);
}

void AccessibleNode::setValueNow(float value_now, bool is_null) {
  SetFloatProperty(AOMFloatProperty::kValueNow, value_now, is_null);
  NotifyAttributeChanged(aria_valuenowAttr);
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

template <typename P, typename T>
static void SetProperty(P property,
                        T value,
                        bool is_null,
                        Vector<std::pair<P, T>>& properties) {
  for (size_t i = 0; i < properties.size(); i++) {
    auto& item = properties[i];
    if (item.first == property) {
      if (is_null)
        properties.erase(i);
      else
        item.second = value;
      return;
    }
  }

  properties.push_back(std::make_pair(property, value));
}

void AccessibleNode::SetBooleanProperty(AOMBooleanProperty property,
                                        bool value,
                                        bool is_null) {
  SetProperty(property, value, is_null, boolean_properties_);
}

void AccessibleNode::SetIntProperty(AOMIntProperty property,
                                    int32_t value,
                                    bool is_null) {
  SetProperty(property, value, is_null, int_properties_);
}

void AccessibleNode::SetUIntProperty(AOMUIntProperty property,
                                     uint32_t value,
                                     bool is_null) {
  SetProperty(property, value, is_null, uint_properties_);
}

void AccessibleNode::SetFloatProperty(AOMFloatProperty property,
                                      float value,
                                      bool is_null) {
  SetProperty(property, value, is_null, float_properties_);
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
