// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MutableCSSPropertyValueSet.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserContext.h"

namespace blink {

namespace {

inline bool ContainsId(const CSSProperty** set,
                       unsigned length,
                       CSSPropertyID id) {
  for (unsigned i = 0; i < length; ++i) {
    if (set[i]->IDEquals(id))
      return true;
  }
  return false;
}

}  // namespace

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    CSSParserMode css_parser_mode)
    : CSSPropertyValueSet(css_parser_mode) {}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    const CSSPropertyValue* properties,
    unsigned length)
    : CSSPropertyValueSet(kHTMLStandardMode) {
  property_vector_.ReserveInitialCapacity(length);
  for (unsigned i = 0; i < length; ++i)
    property_vector_.UncheckedAppend(properties[i]);
}

MutableCSSPropertyValueSet::MutableCSSPropertyValueSet(
    const CSSPropertyValueSet& other)
    : CSSPropertyValueSet(other.CssParserMode()) {
  if (other.IsMutable()) {
    property_vector_ = ToMutableCSSPropertyValueSet(other).property_vector_;
  } else {
    property_vector_.ReserveInitialCapacity(other.PropertyCount());
    for (unsigned i = 0; i < other.PropertyCount(); ++i) {
      property_vector_.UncheckedAppend(
          other.PropertyAt(i).ToCSSPropertyValue());
    }
  }
}

bool MutableCSSPropertyValueSet::RemoveShorthandProperty(
    CSSPropertyID property_id) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length())
    return false;

  return RemovePropertiesInSet(shorthand.properties(), shorthand.length());
}

bool MutableCSSPropertyValueSet::RemovePropertyAtIndex(int property_index,
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
bool MutableCSSPropertyValueSet::RemoveProperty(T property,
                                                String* return_text) {
  if (RemoveShorthandProperty(property)) {
    // FIXME: Return an equivalent shorthand when possible.
    if (return_text)
      *return_text = "";
    return true;
  }

  int found_property_index = FindPropertyIndex(property);
  return RemovePropertyAtIndex(found_property_index, return_text);
}
template CORE_EXPORT bool MutableCSSPropertyValueSet::RemoveProperty(
    CSSPropertyID,
    String*);
template CORE_EXPORT bool MutableCSSPropertyValueSet::RemoveProperty(
    AtomicString,
    String*);

MutableCSSPropertyValueSet::SetResult MutableCSSPropertyValueSet::SetProperty(
    CSSPropertyID unresolved_property,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
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
                               secure_context_mode, context_style_sheet);
}

MutableCSSPropertyValueSet::SetResult MutableCSSPropertyValueSet::SetProperty(
    const AtomicString& custom_property_name,
    const PropertyRegistry* registry,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet,
    bool is_animation_tainted) {
  if (value.IsEmpty()) {
    bool did_parse = true;
    bool did_change = RemoveProperty(custom_property_name);
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }
  return CSSParser::ParseValueForCustomProperty(
      this, custom_property_name, registry, value, important,
      secure_context_mode, context_style_sheet, is_animation_tainted);
}

void MutableCSSPropertyValueSet::SetProperty(CSSPropertyID property_id,
                                             const CSSValue& value,
                                             bool important) {
  StylePropertyShorthand shorthand = shorthandForProperty(property_id);
  if (!shorthand.length()) {
    SetProperty(
        CSSPropertyValue(CSSProperty::Get(property_id), value, important));
    return;
  }

  RemovePropertiesInSet(shorthand.properties(), shorthand.length());

  for (unsigned i = 0; i < shorthand.length(); ++i) {
    property_vector_.push_back(
        CSSPropertyValue(*shorthand.properties()[i], value, important));
  }
}

bool MutableCSSPropertyValueSet::SetProperty(const CSSPropertyValue& property,
                                             CSSPropertyValue* slot) {
  const AtomicString& name =
      (property.Id() == CSSPropertyVariable)
          ? ToCSSCustomPropertyDeclaration(property.Value())->GetName()
          : g_null_atom;
  CSSPropertyValue* to_replace =
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

bool MutableCSSPropertyValueSet::SetProperty(CSSPropertyID property_id,
                                             CSSValueID identifier,
                                             bool important) {
  SetProperty(CSSPropertyValue(CSSProperty::Get(property_id),
                               *CSSIdentifierValue::Create(identifier),
                               important));
  return true;
}

void MutableCSSPropertyValueSet::ParseDeclarationList(
    const String& style_declaration,
    SecureContextMode secure_context_mode,
    StyleSheetContents* context_style_sheet) {
  property_vector_.clear();

  CSSParserContext* context;
  if (context_style_sheet) {
    context = CSSParserContext::CreateWithStyleSheetContents(
        context_style_sheet->ParserContext(), context_style_sheet);
    context->SetMode(CssParserMode());
  } else {
    context = CSSParserContext::Create(CssParserMode(), secure_context_mode);
  }

  CSSParser::ParseDeclarationList(context, this, style_declaration);
}

bool MutableCSSPropertyValueSet::AddParsedProperties(
    const HeapVector<CSSPropertyValue, 256>& properties) {
  bool changed = false;
  property_vector_.ReserveCapacity(property_vector_.size() + properties.size());
  for (unsigned i = 0; i < properties.size(); ++i)
    changed |= SetProperty(properties[i]);
  return changed;
}

bool MutableCSSPropertyValueSet::AddRespectingCascade(
    const CSSPropertyValue& property) {
  // Only add properties that have no !important counterpart present
  if (!PropertyIsImportant(property.Id()) || property.IsImportant())
    return SetProperty(property);
  return false;
}

void MutableCSSPropertyValueSet::MergeAndOverrideOnConflict(
    const CSSPropertyValueSet* other) {
  unsigned size = other->PropertyCount();
  for (unsigned n = 0; n < size; ++n) {
    PropertyReference to_merge = other->PropertyAt(n);
    // TODO(leviw): This probably doesn't work correctly with Custom Properties
    CSSPropertyValue* old = FindCSSPropertyWithID(to_merge.Id());
    if (old)
      SetProperty(to_merge.ToCSSPropertyValue(), old);
    else
      property_vector_.push_back(to_merge.ToCSSPropertyValue());
  }
}

void MutableCSSPropertyValueSet::Clear() {
  property_vector_.clear();
}

bool MutableCSSPropertyValueSet::RemovePropertiesInSet(const CSSProperty** set,
                                                       unsigned length) {
  if (property_vector_.IsEmpty())
    return false;

  CSSPropertyValue* properties = property_vector_.data();
  unsigned old_size = property_vector_.size();
  unsigned new_index = 0;
  for (unsigned old_index = 0; old_index < old_size; ++old_index) {
    const CSSPropertyValue& property = properties[old_index];
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

CSSPropertyValue* MutableCSSPropertyValueSet::FindCSSPropertyWithID(
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

bool CSSPropertyValueSet::PropertyMatches(
    CSSPropertyID property_id,
    const CSSValue& property_value) const {
  int found_property_index = FindPropertyIndex(property_id);
  if (found_property_index == -1)
    return false;
  return PropertyAt(found_property_index).Value() == property_value;
}

void MutableCSSPropertyValueSet::RemoveEquivalentProperties(
    const CSSPropertyValueSet* style) {
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

void MutableCSSPropertyValueSet::RemoveEquivalentProperties(
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

CSSStyleDeclaration* MutableCSSPropertyValueSet::EnsureCSSStyleDeclaration() {
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
int MutableCSSPropertyValueSet::FindPropertyIndex(T property) const {
  const CSSPropertyValue* begin = property_vector_.data();
  const CSSPropertyValue* end = begin + property_vector_.size();

  uint16_t id = CSSPropertyValueSet::GetConvertedCSSPropertyID(property);

  const CSSPropertyValue* it = std::find_if(
      begin, end, [property, id](const CSSPropertyValue& css_property) -> bool {
        return IsPropertyMatch(css_property.Metadata(), *css_property.Value(),
                               id, property);
      });

  return (it == end) ? -1 : it - begin;
}
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    CSSPropertyID) const;
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    AtomicString) const;
template CORE_EXPORT int MutableCSSPropertyValueSet::FindPropertyIndex(
    AtRuleDescriptorID) const;

void MutableCSSPropertyValueSet::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(cssom_wrapper_);
  visitor->Trace(property_vector_);
  CSSPropertyValueSet::TraceAfterDispatch(visitor);
}

MutableCSSPropertyValueSet* MutableCSSPropertyValueSet::Create(
    CSSParserMode css_parser_mode) {
  return new MutableCSSPropertyValueSet(css_parser_mode);
}

MutableCSSPropertyValueSet* MutableCSSPropertyValueSet::Create(
    const CSSPropertyValue* properties,
    unsigned count) {
  return new MutableCSSPropertyValueSet(properties, count);
}

}  // namespace blink
