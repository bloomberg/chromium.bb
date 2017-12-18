// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

#include "core/css/CSSCustomPropertyDeclaration.h"

namespace blink {

FilteredComputedStylePropertyMap::FilteredComputedStylePropertyMap(
    Node* node,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties)
    : ComputedStylePropertyMap(node) {
  for (const auto& native_property : native_properties) {
    native_properties_.insert(native_property);
  }

  for (const auto& custom_property : custom_properties) {
    custom_properties_.insert(custom_property);
  }
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

  return ComputedStylePropertyMap::GetCustomProperty(property_name);
}

void FilteredComputedStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  // FIXME: We should be filtering out properties from ComputedStylePropertyMap,
  // but native_properties_ may contain invalid properties (e.g. shorthands).
  // The correct behaviour would be to ignore invalid properties, but there
  // are a few tests that rely on this.
  for (const auto property_id : native_properties_) {
    const CSSValue* value = GetProperty(property_id);
    if (value) {
      callback(CSSProperty::Get(property_id).GetPropertyNameAtomicString(),
               *value);
    }
  }

  for (const auto& name : custom_properties_) {
    const CSSValue* value = GetCustomProperty(name);
    // FIXME: If custom_properties_ contains an invalid custom property, the
    // current behaviour is to treat it as valid. The best we can do here is
    // returning a dummy 'initial' value until we fix this.
    if (value) {
      callback(name, *value);
    } else {
      callback(name,
               *CSSCustomPropertyDeclaration::Create(name, CSSValueInitial));
    }
  }
}

}  // namespace blink
