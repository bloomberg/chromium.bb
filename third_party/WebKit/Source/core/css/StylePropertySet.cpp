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

#include "core/css/StylePropertySet.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/StylePropertySerializer.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "core/frame/UseCounter.h"
#include "platform/wtf/text/StringBuilder.h"

#ifndef NDEBUG
#include <stdio.h>
#include "platform/wtf/text/CString.h"
#endif

namespace blink {

static size_t SizeForImmutableStylePropertySetWithPropertyCount(
    unsigned count) {
  return sizeof(ImmutableStylePropertySet) - sizeof(void*) +
         sizeof(Member<CSSValue>) * count +
         sizeof(StylePropertyMetadata) * count;
}

ImmutableStylePropertySet* ImmutableStylePropertySet::Create(
    const CSSProperty* properties,
    unsigned count,
    CSSParserMode css_parser_mode) {
  DCHECK_LE(count, static_cast<unsigned>(kMaxArraySize));
  void* slot = ThreadHeap::Allocate<StylePropertySet>(
      SizeForImmutableStylePropertySetWithPropertyCount(count));
  return new (slot)
      ImmutableStylePropertySet(properties, count, css_parser_mode);
}

ImmutableStylePropertySet* StylePropertySet::ImmutableCopyIfNeeded() const {
  if (!IsMutable())
    return ToImmutableStylePropertySet(const_cast<StylePropertySet*>(this));
  const MutableStylePropertySet* mutable_this = ToMutableStylePropertySet(this);
  return ImmutableStylePropertySet::Create(
      mutable_this->property_vector_.data(),
      mutable_this->property_vector_.size(), CssParserMode());
}

MutableStylePropertySet::MutableStylePropertySet(CSSParserMode css_parser_mode)
    : StylePropertySet(css_parser_mode) {}

MutableStylePropertySet::MutableStylePropertySet(const CSSProperty* properties,
                                                 unsigned length)
    : StylePropertySet(kHTMLStandardMode) {
  property_vector_.ReserveInitialCapacity(length);
  for (unsigned i = 0; i < length; ++i)
    property_vector_.UncheckedAppend(properties[i]);
}

ImmutableStylePropertySet::ImmutableStylePropertySet(
    const CSSProperty* properties,
    unsigned length,
    CSSParserMode css_parser_mode)
    : StylePropertySet(css_parser_mode, length) {
  StylePropertyMetadata* metadata_array =
      const_cast<StylePropertyMetadata*>(this->MetadataArray());
  Member<const CSSValue>* value_array =
      const_cast<Member<const CSSValue>*>(this->ValueArray());
  for (unsigned i = 0; i < array_size_; ++i) {
    metadata_array[i] = properties[i].Metadata();
    value_array[i] = properties[i].Value();
  }
}

ImmutableStylePropertySet::~ImmutableStylePropertySet() {}

// Convert property into an uint16_t for comparison with metadata's property_id_
// to avoid the compiler converting it to an int multiple times in a loop.
static uint16_t GetConvertedCSSPropertyID(CSSPropertyID property_id) {
  return static_cast<uint16_t>(property_id);
}

static uint16_t GetConvertedCSSPropertyID(const AtomicString&) {
  return static_cast<uint16_t>(CSSPropertyVariable);
}

static bool IsPropertyMatch(const StylePropertyMetadata& metadata,
                            const CSSValue&,
                            uint16_t id,
                            CSSPropertyID property_id) {
  DCHECK_EQ(id, property_id);
  bool result = metadata.property_id_ == id;
  // Only enabled properties should be part of the style.
#if DCHECK_IS_ON()
  DCHECK(!result ||
         CSSPropertyAPI::Get(resolveCSSPropertyID(property_id)).IsEnabled());
#endif
  return result;
}

static bool IsPropertyMatch(const StylePropertyMetadata& metadata,
                            const CSSValue& value,
                            uint16_t id,
                            const AtomicString& custom_property_name) {
  DCHECK_EQ(id, CSSPropertyVariable);
  return metadata.property_id_ == id &&
         ToCSSCustomPropertyDeclaration(value).GetName() ==
             custom_property_name;
}

template <typename T>
int ImmutableStylePropertySet::FindPropertyIndex(T property) const {
  uint16_t id = GetConvertedCSSPropertyID(property);
  for (int n = array_size_ - 1; n >= 0; --n) {
    if (IsPropertyMatch(MetadataArray()[n], *ValueArray()[n], id, property))
      return n;
  }

  return -1;
}
template CORE_EXPORT int ImmutableStylePropertySet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int ImmutableStylePropertySet::FindPropertyIndex(
    AtomicString) const;

DEFINE_TRACE_AFTER_DISPATCH(ImmutableStylePropertySet) {
  const Member<const CSSValue>* values = ValueArray();
  for (unsigned i = 0; i < array_size_; i++)
    visitor->Trace(values[i]);
  StylePropertySet::TraceAfterDispatch(visitor);
}

MutableStylePropertySet::MutableStylePropertySet(const StylePropertySet& other)
    : StylePropertySet(other.CssParserMode()) {
  if (other.IsMutable()) {
    property_vector_ = ToMutableStylePropertySet(other).property_vector_;
  } else {
    property_vector_.ReserveInitialCapacity(other.PropertyCount());
    for (unsigned i = 0; i < other.PropertyCount(); ++i)
      property_vector_.UncheckedAppend(other.PropertyAt(i).ToCSSProperty());
  }
}

static String SerializeShorthand(const StylePropertySet& property_set,
                                 CSSPropertyID property_id) {
  return StylePropertySerializer(property_set).GetPropertyValue(property_id);
}

static String SerializeShorthand(const StylePropertySet&,
                                 const AtomicString& custom_property_name) {
  // Custom properties are never shorthands.
  return "";
}

template <typename T>
String StylePropertySet::GetPropertyValue(T property) const {
  const CSSValue* value = GetPropertyCSSValue(property);
  if (value)
    return value->CssText();
  return SerializeShorthand(*this, property);
}
template CORE_EXPORT String
    StylePropertySet::GetPropertyValue<CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT String
    StylePropertySet::GetPropertyValue<AtomicString>(AtomicString) const;

template <typename T>
const CSSValue* StylePropertySet::GetPropertyCSSValue(T property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index == -1)
    return nullptr;
  return &PropertyAt(found_property_index).Value();
}
template CORE_EXPORT const CSSValue*
    StylePropertySet::GetPropertyCSSValue<CSSPropertyID>(CSSPropertyID) const;
template CORE_EXPORT const CSSValue*
    StylePropertySet::GetPropertyCSSValue<AtomicString>(AtomicString) const;

DEFINE_TRACE(StylePropertySet) {
  if (is_mutable_)
    ToMutableStylePropertySet(this)->TraceAfterDispatch(visitor);
  else
    ToImmutableStylePropertySet(this)->TraceAfterDispatch(visitor);
}

void StylePropertySet::FinalizeGarbageCollectedObject() {
  if (is_mutable_)
    ToMutableStylePropertySet(this)->~MutableStylePropertySet();
  else
    ToImmutableStylePropertySet(this)->~ImmutableStylePropertySet();
}

bool MutableStylePropertySet::RemoveShorthandProperty(
    CSSPropertyID property_id) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return false;

  return RemovePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool MutableStylePropertySet::RemovePropertyAtIndex(int property_index,
                                                    String* return_text) {
  if (property_index == -1) {
    if (return_text)
      *return_text = "";
    return false;
  }

  if (return_text)
    *return_text = PropertyAt(property_index).Value().CssText();

  // A more efficient removal strategy would involve marking entries as empty
  // and sweeping them when the vector grows too big.
  property_vector_.EraseAt(property_index);

  return true;
}

template <typename T>
bool MutableStylePropertySet::RemoveProperty(T property, String* return_text) {
  if (RemoveShorthandProperty(property)) {
    // FIXME: Return an equivalent shorthand when possible.
    if (return_text)
      *return_text = "";
    return true;
  }

  int found_property_index = FindPropertyIndex(property);
  return RemovePropertyAtIndex(found_property_index, return_text);
}
template CORE_EXPORT bool MutableStylePropertySet::RemoveProperty(CSSPropertyID,
                                                                  String*);
template CORE_EXPORT bool MutableStylePropertySet::RemoveProperty(AtomicString,
                                                                  String*);

template <typename T>
bool StylePropertySet::PropertyIsImportant(T property) const {
  int found_property_index = FindPropertyIndex(property);
  if (found_property_index != -1)
    return PropertyAt(found_property_index).IsImportant();
  return ShorthandIsImportant(property);
}
template bool StylePropertySet::PropertyIsImportant<CSSPropertyID>(
    CSSPropertyID) const;
template bool StylePropertySet::PropertyIsImportant<AtomicString>(
    AtomicString) const;

bool StylePropertySet::ShorthandIsImportant(CSSPropertyID property_id) const {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return false;

  for (unsigned i = 0; i < shorthand.length(); ++i) {
    if (!PropertyIsImportant(shorthand.properties()[i]))
      return false;
  }
  return true;
}

bool StylePropertySet::ShorthandIsImportant(
    AtomicString custom_property_name) const {
  // Custom properties are never shorthands.
  return false;
}

CSSPropertyID StylePropertySet::GetPropertyShorthand(
    CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return CSSPropertyInvalid;
  return PropertyAt(found_property_index).ShorthandID();
}

bool StylePropertySet::IsPropertyImplicit(CSSPropertyID property_id) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return false;
  return PropertyAt(found_property_index).IsImplicit();
}

MutableStylePropertySet::SetResult MutableStylePropertySet::SetProperty(
    CSSPropertyID unresolved_property,
    const String& value,
    bool important,
    StyleSheetContents* context_style_sheet) {
  DCHECK_GE(unresolved_property, firstCSSProperty);

  // Setting the value to an empty string just removes the property in both IE
  // and Gecko. Setting it to null seems to produce less consistent results, but
  // we treat it just the same.
  if (value.IsEmpty()) {
    bool did_parse = true;
    bool did_change = RemoveProperty(resolveCSSPropertyID(unresolved_property));
    return SetResult{did_parse, did_change};
  }

  // When replacing an existing property value, this moves the property to the
  // end of the list. Firefox preserves the position, and MSIE moves the
  // property to the beginning.
  return CSSParser::ParseValue(this, unresolved_property, value, important,
                               context_style_sheet);
}

MutableStylePropertySet::SetResult MutableStylePropertySet::SetProperty(
    const AtomicString& custom_property_name,
    const PropertyRegistry* registry,
    const String& value,
    bool important,
    StyleSheetContents* context_style_sheet,
    bool is_animation_tainted) {
  if (value.IsEmpty()) {
    bool did_parse = true;
    bool did_change = RemoveProperty(custom_property_name);
    return MutableStylePropertySet::SetResult{did_parse, did_change};
  }
  return CSSParser::ParseValueForCustomProperty(
      this, custom_property_name, registry, value, important,
      context_style_sheet, is_animation_tainted);
}

void MutableStylePropertySet::SetProperty(CSSPropertyID property_id,
                                          const CSSValue& value,
                                          bool important) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length()) {
    SetProperty(CSSProperty(property_id, value, important));
    return;
  }

  RemovePropertiesInSet(shorthand.properties(), shorthand.length());

  for (unsigned i = 0; i < shorthand.length(); ++i)
    property_vector_.push_back(
        CSSProperty(shorthand.properties()[i], value, important));
}

bool MutableStylePropertySet::SetProperty(const CSSProperty& property,
                                          CSSProperty* slot) {
  const AtomicString& name =
      (property.Id() == CSSPropertyVariable)
          ? ToCSSCustomPropertyDeclaration(property.Value())->GetName()
          : g_null_atom;
  CSSProperty* to_replace =
      slot ? slot : FindCSSPropertyWithID(property.Id(), name);
  if (to_replace && *to_replace == property)
    return false;
  if (to_replace) {
    *to_replace = property;
    return true;
  }
  property_vector_.push_back(property);
  return true;
}

bool MutableStylePropertySet::SetProperty(CSSPropertyID property_id,
                                          CSSValueID identifier,
                                          bool important) {
  SetProperty(CSSProperty(property_id, *CSSIdentifierValue::Create(identifier),
                          important));
  return true;
}

void MutableStylePropertySet::ParseDeclarationList(
    const String& style_declaration,
    StyleSheetContents* context_style_sheet) {
  property_vector_.clear();

  CSSParserContext* context;
  if (context_style_sheet) {
    context = CSSParserContext::CreateWithStyleSheetContents(
        context_style_sheet->ParserContext(), context_style_sheet);
    context->SetMode(CssParserMode());
  } else {
    context = CSSParserContext::Create(CssParserMode());
  }

  CSSParser::ParseDeclarationList(context, this, style_declaration);
}

bool MutableStylePropertySet::AddParsedProperties(
    const HeapVector<CSSProperty, 256>& properties) {
  bool changed = false;
  property_vector_.ReserveCapacity(property_vector_.size() + properties.size());
  for (unsigned i = 0; i < properties.size(); ++i)
    changed |= SetProperty(properties[i]);
  return changed;
}

bool MutableStylePropertySet::AddRespectingCascade(
    const CSSProperty& property) {
  // Only add properties that have no !important counterpart present
  if (!PropertyIsImportant(property.Id()) || property.IsImportant())
    return SetProperty(property);
  return false;
}

String StylePropertySet::AsText() const {
  return StylePropertySerializer(*this).AsText();
}

void MutableStylePropertySet::MergeAndOverrideOnConflict(
    const StylePropertySet* other) {
  unsigned size = other->PropertyCount();
  for (unsigned n = 0; n < size; ++n) {
    PropertyReference to_merge = other->PropertyAt(n);
    // TODO(leviw): This probably doesn't work correctly with Custom Properties
    CSSProperty* old = FindCSSPropertyWithID(to_merge.Id());
    if (old)
      SetProperty(to_merge.ToCSSProperty(), old);
    else
      property_vector_.push_back(to_merge.ToCSSProperty());
  }
}

bool StylePropertySet::HasFailedOrCanceledSubresources() const {
  unsigned size = PropertyCount();
  for (unsigned i = 0; i < size; ++i) {
    if (PropertyAt(i).Value().HasFailedOrCanceledSubresources())
      return true;
  }
  return false;
}

void MutableStylePropertySet::Clear() {
  property_vector_.clear();
}

inline bool ContainsId(const CSSPropertyID* set,
                       unsigned length,
                       CSSPropertyID id) {
  for (unsigned i = 0; i < length; ++i) {
    if (set[i] == id)
      return true;
  }
  return false;
}

bool MutableStylePropertySet::RemovePropertiesInSet(const CSSPropertyID* set,
                                                    unsigned length) {
  if (property_vector_.IsEmpty())
    return false;

  CSSProperty* properties = property_vector_.data();
  unsigned old_size = property_vector_.size();
  unsigned new_index = 0;
  for (unsigned old_index = 0; old_index < old_size; ++old_index) {
    const CSSProperty& property = properties[old_index];
    if (ContainsId(set, length, property.Id()))
      continue;
    // Modify property_vector_ in-place since this method is
    // performance-sensitive.
    properties[new_index++] = properties[old_index];
  }
  if (new_index != old_size) {
    property_vector_.Shrink(new_index);
    return true;
  }
  return false;
}

CSSProperty* MutableStylePropertySet::FindCSSPropertyWithID(
    CSSPropertyID property_id,
    const AtomicString& custom_property_name) {
  int found_property_index = -1;
  if (property_id == CSSPropertyVariable && !custom_property_name.IsNull()) {
    // TODO(shanestephens): fix call sites so we always have a
    // customPropertyName here.
    found_property_index = FindPropertyIndex(custom_property_name);
  } else {
    DCHECK(custom_property_name.IsNull());
    found_property_index = FindPropertyIndex(property_id);
  }
  if (found_property_index == -1)
    return nullptr;
  return &property_vector_.at(found_property_index);
}

bool StylePropertySet::PropertyMatches(CSSPropertyID property_id,
                                       const CSSValue& property_value) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return false;
  return PropertyAt(found_property_index).Value() == property_value;
}

void MutableStylePropertySet::RemoveEquivalentProperties(
    const StylePropertySet* style) {
  Vector<CSSPropertyID> properties_to_remove;
  unsigned size = property_vector_.size();
  for (unsigned i = 0; i < size; ++i) {
    PropertyReference property = PropertyAt(i);
    if (style->PropertyMatches(property.Id(), property.Value()))
      properties_to_remove.push_back(property.Id());
  }
  // FIXME: This should use mass removal.
  for (unsigned i = 0; i < properties_to_remove.size(); ++i)
    RemoveProperty(properties_to_remove[i]);
}

void MutableStylePropertySet::RemoveEquivalentProperties(
    const CSSStyleDeclaration* style) {
  Vector<CSSPropertyID> properties_to_remove;
  unsigned size = property_vector_.size();
  for (unsigned i = 0; i < size; ++i) {
    PropertyReference property = PropertyAt(i);
    if (style->CssPropertyMatches(property.Id(), &property.Value()))
      properties_to_remove.push_back(property.Id());
  }
  // FIXME: This should use mass removal.
  for (unsigned i = 0; i < properties_to_remove.size(); ++i)
    RemoveProperty(properties_to_remove[i]);
}

MutableStylePropertySet* StylePropertySet::MutableCopy() const {
  return new MutableStylePropertySet(*this);
}

MutableStylePropertySet* StylePropertySet::CopyPropertiesInSet(
    const Vector<CSSPropertyID>& properties) const {
  HeapVector<CSSProperty, 256> list;
  list.ReserveInitialCapacity(properties.size());
  for (unsigned i = 0; i < properties.size(); ++i) {
    const CSSValue* value = GetPropertyCSSValue(properties[i]);
    if (value)
      list.push_back(CSSProperty(properties[i], *value, false));
  }
  return MutableStylePropertySet::Create(list.data(), list.size());
}

CSSStyleDeclaration* MutableStylePropertySet::EnsureCSSStyleDeclaration() {
  // FIXME: get rid of this weirdness of a CSSStyleDeclaration inside of a
  // style property set.
  if (cssom_wrapper_) {
    DCHECK(
        !static_cast<CSSStyleDeclaration*>(cssom_wrapper_.Get())->parentRule());
    DCHECK(!cssom_wrapper_->ParentElement());
    return cssom_wrapper_.Get();
  }
  cssom_wrapper_ = new PropertySetCSSStyleDeclaration(*this);
  return cssom_wrapper_.Get();
}

template <typename T>
int MutableStylePropertySet::FindPropertyIndex(T property) const {
  const CSSProperty* begin = property_vector_.data();
  const CSSProperty* end = begin + property_vector_.size();

  uint16_t id = GetConvertedCSSPropertyID(property);

  const CSSProperty* it = std::find_if(
      begin, end, [property, id](const CSSProperty& css_property) -> bool {
        return IsPropertyMatch(css_property.Metadata(), *css_property.Value(),
                               id, property);
      });

  return (it == end) ? -1 : it - begin;
}
template CORE_EXPORT int MutableStylePropertySet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int MutableStylePropertySet::FindPropertyIndex(
    AtomicString) const;

DEFINE_TRACE_AFTER_DISPATCH(MutableStylePropertySet) {
  visitor->Trace(cssom_wrapper_);
  visitor->Trace(property_vector_);
  StylePropertySet::TraceAfterDispatch(visitor);
}

unsigned StylePropertySet::AverageSizeInBytes() {
  // Please update this if the storage scheme changes so that this longer
  // reflects the actual size.
  return SizeForImmutableStylePropertySetWithPropertyCount(4);
}

// See the function above if you need to update this.
struct SameSizeAsStylePropertySet
    : public GarbageCollectedFinalized<SameSizeAsStylePropertySet> {
  unsigned bitfield;
};
static_assert(sizeof(StylePropertySet) == sizeof(SameSizeAsStylePropertySet),
              "StylePropertySet should stay small");

#ifndef NDEBUG
void StylePropertySet::ShowStyle() {
  fprintf(stderr, "%s\n", AsText().Ascii().data());
}
#endif

MutableStylePropertySet* MutableStylePropertySet::Create(
    CSSParserMode css_parser_mode) {
  return new MutableStylePropertySet(css_parser_mode);
}

MutableStylePropertySet* MutableStylePropertySet::Create(
    const CSSProperty* properties,
    unsigned count) {
  return new MutableStylePropertySet(properties, count);
}

DEFINE_TRACE(CSSLazyPropertyParser) {}

}  // namespace blink
