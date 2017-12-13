// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/InlineStylePropertyMap.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSPropertyValueSet.h"
#include "core/css/MutableCSSPropertyValueSet.h"

namespace blink {

const CSSValue* InlineStylePropertyMap::GetProperty(CSSPropertyID property_id) {
  return owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
      property_id);
}

const CSSValue* InlineStylePropertyMap::GetCustomProperty(
    AtomicString property_name) {
  return owner_element_->EnsureMutableInlineStyle().GetPropertyCSSValue(
      property_name);
}

void InlineStylePropertyMap::SetProperty(CSSPropertyID property_id,
                                         const CSSValue* value) {
  owner_element_->SetInlineStyleProperty(property_id, value);
}

void InlineStylePropertyMap::RemoveProperty(CSSPropertyID property_id) {
  owner_element_->RemoveInlineStyleProperty(property_id);
}

void InlineStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  CSSPropertyValueSet& inline_style_set =
      owner_element_->EnsureMutableInlineStyle();
  for (unsigned i = 0; i < inline_style_set.PropertyCount(); i++) {
    const auto& property_reference = inline_style_set.PropertyAt(i);
    if (property_reference.Id() == CSSPropertyVariable) {
      const auto& decl =
          ToCSSCustomPropertyDeclaration(property_reference.Value());
      callback(decl.GetName(), property_reference.Value());
    } else {
      callback(getPropertyNameAtomicString(property_reference.Id()),
               property_reference.Value());
    }
  }
}

}  // namespace blink
