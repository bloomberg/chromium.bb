// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/AtRuleDescriptorValueSet.h"

#include "core/css/AtRuleDescriptorSerializer.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/parser/AtRuleDescriptorParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserMode.h"

namespace blink {

AtRuleDescriptorValueSet* AtRuleDescriptorValueSet::Create(
    const HeapVector<blink::CSSPropertyValue, 256>& properties,
    CSSParserMode mode,
    AtRuleType type) {
  AtRuleDescriptorValueSet* set = new AtRuleDescriptorValueSet(mode, type);
  for (const auto& property : properties) {
    set->SetProperty(CSSPropertyIDAsAtRuleDescriptor(property.Id()),
                     *property.Value());
  }
  return set;
}

String AtRuleDescriptorValueSet::AsText() const {
  return AtRuleDescriptorSerializer::SerializeAtRuleDescriptors(*this);
}

bool AtRuleDescriptorValueSet::HasFailedOrCanceledSubresources() const {
  for (const auto& entry : values_) {
    if (entry.second->HasFailedOrCanceledSubresources()) {
      return true;
    }
  }
  return false;
}

const CSSValue* AtRuleDescriptorValueSet::GetPropertyCSSValue(
    AtRuleDescriptorID id) const {
  int index = IndexOf(id);
  if (index < 0)
    return nullptr;

  return values_[index].second;
}

void AtRuleDescriptorValueSet::SetProperty(AtRuleDescriptorID id,
                                           const String& value,
                                           SecureContextMode mode) {
  if (value.IsEmpty()) {
    if (Contains(id))
      RemoveProperty(id);
    return;
  }

  CSSParserContext* context = CSSParserContext::Create(kHTMLStandardMode, mode);
  const CSSValue* parsed_value =
      AtRuleDescriptorParser::ParseAtRule(id, value, *context);
  if (parsed_value) {
    SetProperty(id, *parsed_value);
  }
}

void AtRuleDescriptorValueSet::SetProperty(AtRuleDescriptorID id,
                                           const CSSValue& value) {
  int index = IndexOf(id);
  if (index < 0) {
    values_.push_back(std::make_pair(id, &value));
    return;
  }

  values_[index].second = &value;
}

void AtRuleDescriptorValueSet::RemoveProperty(AtRuleDescriptorID id) {
  int index = IndexOf(id);
  if (index >= 0)
    values_.EraseAt(index);
}

String AtRuleDescriptorValueSet::DescriptorAt(unsigned index) const {
  if (index >= values_.size())
    return g_empty_string;

  return GetDescriptorName(values_[index].first);
}

AtRuleDescriptorValueSet* AtRuleDescriptorValueSet::MutableCopy() const {
  AtRuleDescriptorValueSet* set = new AtRuleDescriptorValueSet(mode_, type_);
  for (const auto& entry : values_) {
    set->values_.push_back(std::make_pair(entry.first, entry.second));
  }
  return set;
}

void AtRuleDescriptorValueSet::ParseDeclarationList(const String& declaration,
                                                    SecureContextMode mode) {
  values_.clear();
  // TODO(crbug.com/752745): Refactor CSSParserImpl to avoid
  // using CSSPropertyValueSet here.
  MutableCSSPropertyValueSet* property_value_set =
      MutableCSSPropertyValueSet::Create(mode_);
  property_value_set->ParseDeclarationList(declaration, mode,
                                           nullptr /* context_style_sheet */);
  for (unsigned i = 0; i < property_value_set->PropertyCount(); i++) {
    CSSPropertyValueSet::PropertyReference reference =
        property_value_set->PropertyAt(i);
    SetProperty(CSSPropertyIDAsAtRuleDescriptor(reference.Id()),
                reference.Value());
  }
}

void AtRuleDescriptorValueSet::Trace(blink::Visitor* visitor) {
  for (const auto& entry : values_) {
    visitor->Trace(entry.second);
  }
}

bool AtRuleDescriptorValueSet::Contains(AtRuleDescriptorID id) const {
  return IndexOf(id) != -1;
}

int AtRuleDescriptorValueSet::IndexOf(AtRuleDescriptorID id) const {
  for (unsigned i = 0; i < values_.size(); i++) {
    if (values_[i].first == id)
      return i;
  }
  return -1;
}

}  // namespace blink
