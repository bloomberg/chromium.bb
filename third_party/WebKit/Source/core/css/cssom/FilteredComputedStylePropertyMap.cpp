// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/cssom/CSSUnparsedValue.h"

namespace blink {

FilteredComputedStylePropertyMap::FilteredComputedStylePropertyMap(
    Node* node,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties)
    : ComputedStylePropertyMap(node) {
  for (const auto& native_property : native_properties) {
    // Silently drop shorthand properties.
    DCHECK_NE(native_property, CSSPropertyInvalid);
    if (CSSProperty::Get(native_property).IsShorthand())
      continue;

    native_properties_.insert(native_property);
  }

  for (const auto& custom_property : custom_properties) {
    custom_properties_.insert(custom_property);
  }
}

int FilteredComputedStylePropertyMap::size() {
  return native_properties_.size() + custom_properties_.size();
}

const CSSValue* FilteredComputedStylePropertyMap::GetProperty(
    CSSPropertyID property_id) {
  if (!native_properties_.Contains(property_id))
    return nullptr;

  return ComputedStylePropertyMap::GetProperty(property_id);
}

const CSSValue* FilteredComputedStylePropertyMap::GetCustomProperty(
    AtomicString property_name) {
  if (!custom_properties_.Contains(AtomicString(property_name)))
    return nullptr;

  const CSSValue* value =
      ComputedStylePropertyMap::GetCustomProperty(property_name);
  if (!value)
    return CSSUnparsedValue::Create()->ToCSSValue();
  return value;
}

void FilteredComputedStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  for (const auto property_id : native_properties_) {
    if (const CSSValue* value = GetProperty(property_id)) {
      callback(CSSProperty::Get(property_id).GetPropertyNameAtomicString(),
               *value);
    }
  }

  for (const auto& name : custom_properties_) {
    const CSSValue* value = GetCustomProperty(name);
    DCHECK(value);
    callback(name, *value);
  }
}

}  // namespace blink
