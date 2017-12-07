/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "core/css/CSSPropertyValueSet.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/ImmutableCSSPropertyValueSet.h"
#include "core/css/MutableCSSPropertyValueSet.h"
#include "core/css/StylePropertySerializer.h"
#include "core/css/properties/CSSProperty.h"

#ifndef NDEBUG
#include <stdio.h>
#include "platform/wtf/text/CString.h"
#endif

namespace blink {

namespace {

String SerializeShorthand(const CSSPropertyValueSet& property_set,
                          CSSPropertyID property_id) {
  return StylePropertySerializer(property_set).GetPropertyValue(property_id);
}

String SerializeShorthand(const CSSPropertyValueSet&,
                          const AtomicString& custom_property_name) {
  // Custom properties are never shorthands.
  return "";
}

String SerializeShorthand(const CSSPropertyValueSet& property_set,
                          AtRuleDescriptorID atrule_id) {
  return StylePropertySerializer(property_set)
      .GetPropertyValue(AtRuleDescriptorIDAsCSSPropertyID(atrule_id));
}

}  // namespace

template <typename T>
String CSSPropertyValueSet::GetPropertyValue(T property) const {
  const CSSValue* value = GetPropertyCSSValue(property);
  if (value)
    return value->CssText();
  return SerializeShorthand(*this, property);
}
template CORE_EXPORT String
    CSSPropertyValueSet::GetPropertyValue<CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT String
    CSSPropertyValueSet::GetPropertyValue<AtRuleDescriptorID>(
        AtRuleDescriptorID) const;
template CORE_EXPORT String
    CSSPropertyValueSet::GetPropertyValue<AtomicString>(AtomicString) const;

template <typename T>
const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue(T property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index == -1)
    return nullptr;
  return &PropertyAt(found_property_index).Value();
}
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT const CSSValue* CSSPropertyValueSet::GetPropertyCSSValue<
    AtRuleDescriptorID>(AtRuleDescriptorID) const;
template CORE_EXPORT const CSSValue*
    CSSPropertyValueSet::GetPropertyCSSValue<AtomicString>(AtomicString) const;

void CSSPropertyValueSet::Trace(blink::Visitor* visitor) {
  if (is_mutable_)
    ToMutableCSSPropertyValueSet(this)->TraceAfterDispatch(visitor);
  else
    ToImmutableCSSPropertyValueSet(this)->TraceAfterDispatch(visitor);
}

void CSSPropertyValueSet::FinalizeGarbageCollectedObject() {
  if (is_mutable_)
    ToMutableCSSPropertyValueSet(this)->~MutableCSSPropertyValueSet();
  else
    ToImmutableCSSPropertyValueSet(this)->~ImmutableCSSPropertyValueSet();
}

template <typename T>
bool CSSPropertyValueSet::PropertyIsImportant(T property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index != -1)
    return PropertyAt(found_property_index).IsImportant();
  return ShorthandIsImportant(property);
}
template bool CSSPropertyValueSet::PropertyIsImportant<CSSPropertyID>(
    CSSPropertyID) const;
template bool CSSPropertyValueSet::PropertyIsImportant<AtomicString>(
    AtomicString) const;

bool CSSPropertyValueSet::ShorthandIsImportant(
    CSSPropertyID property_id) const {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return false;

  for (unsigned i = 0; i < shorthand.length(); ++i) {
    if (!PropertyIsImportant(shorthand.properties()[i]->PropertyID()))
      return false;
  }
  return true;
}

bool CSSPropertyValueSet::ShorthandIsImportant(
    AtomicString custom_property_name) const {
  // Custom properties are never shorthands.
  return false;
}

CSSPropertyID CSSPropertyValueSet::GetPropertyShorthand(
    CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return CSSPropertyInvalid;
  return PropertyAt(found_property_index).ShorthandID();
}

bool CSSPropertyValueSet::IsPropertyImplicit(CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return false;
  return PropertyAt(found_property_index).IsImplicit();
}

String CSSPropertyValueSet::AsText() const {
  return StylePropertySerializer(*this).AsText();
}

MutableCSSPropertyValueSet* CSSPropertyValueSet::MutableCopy() const {
  return new MutableCSSPropertyValueSet(*this);
}

MutableCSSPropertyValueSet* CSSPropertyValueSet::CopyPropertiesInSet(
    const Vector<const CSSProperty*>& properties) const {
  HeapVector<CSSPropertyValue, 256> list;
  list.ReserveInitialCapacity(properties.size());
  for (unsigned i = 0; i < properties.size(); ++i) {
    const CSSValue* value = GetPropertyCSSValue(properties[i]->PropertyID());
    if (value) {
      list.push_back(CSSPropertyValue(*properties[i], *value, false));
    }
  }
  return MutableCSSPropertyValueSet::Create(list.data(), list.size());
}

unsigned CSSPropertyValueSet::AverageSizeInBytes() {
  // Please update this if the storage scheme changes so that this longer
  // reflects the actual size.
  return ImmutableCSSPropertyValueSet::SizeWithPropertyCount(4);
}

// See the function above if you need to update this.
struct SameSizeAsCSSPropertyValueSet
    : public GarbageCollectedFinalized<SameSizeAsCSSPropertyValueSet> {
  unsigned bitfield;
};
static_assert(sizeof(CSSPropertyValueSet) ==
                  sizeof(SameSizeAsCSSPropertyValueSet),
              "CSSPropertyValueSet should stay small");

#ifndef NDEBUG
void CSSPropertyValueSet::ShowStyle() {
  fprintf(stderr, "%s\n", AsText().Ascii().data());
}
#endif

const CSSPropertyValueMetadata&
CSSPropertyValueSet::PropertyReference::PropertyMetadata() const {
  if (property_set_->IsMutable()) {
    return ToMutableCSSPropertyValueSet(*property_set_)
        .property_vector_.at(index_)
        .Metadata();
  }
  return ToImmutableCSSPropertyValueSet(*property_set_).MetadataArray()[index_];
}

const CSSValue& CSSPropertyValueSet::PropertyReference::PropertyValue() const {
  if (property_set_->IsMutable()) {
    return *ToMutableCSSPropertyValueSet(*property_set_)
                .property_vector_.at(index_)
                .Value();
  }
  return *ToImmutableCSSPropertyValueSet(*property_set_).ValueArray()[index_];
}

unsigned CSSPropertyValueSet::PropertyCount() const {
  if (is_mutable_)
    return ToMutableCSSPropertyValueSet(this)->property_vector_.size();
  return array_size_;
}

bool CSSPropertyValueSet::IsEmpty() const {
  return !PropertyCount();
}

template <typename T>
int CSSPropertyValueSet::FindPropertyIndex(T property) const {
  if (is_mutable_)
    return ToMutableCSSPropertyValueSet(this)->FindPropertyIndex(property);
  return ToImmutableCSSPropertyValueSet(this)->FindPropertyIndex(property);
}
template CORE_EXPORT int CSSPropertyValueSet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int CSSPropertyValueSet::FindPropertyIndex(
    AtomicString) const;
template CORE_EXPORT int CSSPropertyValueSet::FindPropertyIndex(
    AtRuleDescriptorID) const;

// Convert property into an uint16_t for comparison with metadata's property id
// to avoid the compiler converting it to an int multiple times in a loop.
uint16_t CSSPropertyValueSet::GetConvertedCSSPropertyID(
    CSSPropertyID property_id) {
  return static_cast<uint16_t>(property_id);
}

uint16_t CSSPropertyValueSet::GetConvertedCSSPropertyID(const AtomicString&) {
  return static_cast<uint16_t>(CSSPropertyVariable);
}

uint16_t CSSPropertyValueSet::GetConvertedCSSPropertyID(
    AtRuleDescriptorID descriptor_id) {
  return static_cast<uint16_t>(
      AtRuleDescriptorIDAsCSSPropertyID(descriptor_id));
}

bool CSSPropertyValueSet::IsPropertyMatch(
    const CSSPropertyValueMetadata& metadata,
    const CSSValue&,
    uint16_t id,
    CSSPropertyID property_id) {
  DCHECK_EQ(id, property_id);
  bool result = metadata.Property().PropertyID() == id;
// Only enabled properties should be part of the style.
#if DCHECK_IS_ON()
  DCHECK(!result ||
         CSSProperty::Get(resolveCSSPropertyID(property_id)).IsEnabled());
#endif
  return result;
}

bool CSSPropertyValueSet::IsPropertyMatch(
    const CSSPropertyValueMetadata& metadata,
    const CSSValue& value,
    uint16_t id,
    const AtomicString& custom_property_name) {
  DCHECK_EQ(id, CSSPropertyVariable);
  return metadata.Property().PropertyID() == id &&
         ToCSSCustomPropertyDeclaration(value).GetName() ==
             custom_property_name;
}

bool CSSPropertyValueSet::IsPropertyMatch(
    const CSSPropertyValueMetadata& metadata,
    const CSSValue& css_value,
    uint16_t id,
    AtRuleDescriptorID descriptor_id) {
  return IsPropertyMatch(metadata, css_value, id,
                         AtRuleDescriptorIDAsCSSPropertyID(descriptor_id));
}

bool CSSPropertyValueSet::HasFailedOrCanceledSubresources() const {
  unsigned size = PropertyCount();
  for (unsigned i = 0; i < size; ++i) {
    if (PropertyAt(i).Value().HasFailedOrCanceledSubresources())
      return true;
  }
  return false;
}

}  // namespace blink
