// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/FilteredComputedStylePropertyMap.h"

namespace blink {

FilteredComputedStylePropertyMap::FilteredComputedStylePropertyMap(
    CSSComputedStyleDeclaration* computed_style_declaration,
    const Vector<CSSPropertyID>& native_properties,
    const Vector<AtomicString>& custom_properties,
    Node* node)
    : ComputedStylePropertyMap(node) {
  for (const auto& native_property : native_properties) {
    native_properties_.insert(native_property);
  }

  for (const auto& custom_property : custom_properties) {
    custom_properties_.insert(custom_property);
  }
}

CSSStyleValue* FilteredComputedStylePropertyMap::get(
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id >= firstCSSProperty &&
      native_properties_.Contains(property_id)) {
    CSSStyleValueVector style_vector = GetAllInternal(property_id);
    if (style_vector.IsEmpty())
      return nullptr;

    return style_vector[0];
  }

  if (property_id == CSSPropertyVariable &&
      custom_properties_.Contains(AtomicString(property_name))) {
    CSSStyleValueVector style_vector =
        GetAllInternal(AtomicString(property_name));
    if (style_vector.IsEmpty())
      return nullptr;

    return style_vector[0];
  }

  exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
  return nullptr;
}

CSSStyleValueVector FilteredComputedStylePropertyMap::getAll(
    const String& property_name,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id >= firstCSSProperty &&
      native_properties_.Contains(property_id))
    return GetAllInternal(property_id);

  if (property_id == CSSPropertyVariable &&
      custom_properties_.Contains(AtomicString(property_name)))
    return GetAllInternal(AtomicString(property_name));

  exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
  return CSSStyleValueVector();
}

bool FilteredComputedStylePropertyMap::has(const String& property_name,
                                           ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id >= firstCSSProperty &&
      native_properties_.Contains(property_id))
    return !GetAllInternal(property_id).IsEmpty();

  if (property_id == CSSPropertyVariable &&
      custom_properties_.Contains(AtomicString(property_name)))
    return !GetAllInternal(AtomicString(property_name)).IsEmpty();

  exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
  return false;
}

Vector<String> FilteredComputedStylePropertyMap::getProperties() {
  Vector<String> result;
  for (const auto& native_property : native_properties_) {
    result.push_back(getPropertyNameString(native_property));
  }

  for (const auto& custom_property : custom_properties_) {
    result.push_back(custom_property);
  }

  return result;
}

}  // namespace blink
