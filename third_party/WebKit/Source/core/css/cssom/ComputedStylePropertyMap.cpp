// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/ComputedStylePropertyMap.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSVariableData.h"
#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/dom/Document.h"
#include "core/dom/PseudoElement.h"
#include "core/style/ComputedStyle.h"

namespace blink {

Node* ComputedStylePropertyMap::StyledNode() const {
  DCHECK(node_);
  if (!pseudo_id_)
    return node_;
  if (node_->IsElementNode()) {
    if (PseudoElement* element =
            (ToElement(node_))->GetPseudoElement(pseudo_id_)) {
      return element;
    }
  }
  return nullptr;
}

const ComputedStyle* ComputedStylePropertyMap::UpdateStyle() {
  Node* node = StyledNode();
  if (!node || !node->InActiveDocument())
    return nullptr;

  // Update style before getting the value for the property
  // This could cause the node to be blown away. This code is copied from
  // CSSComputedStyleDeclaration::GetPropertyCSSValue.
  node->GetDocument().UpdateStyleAndLayoutTreeForNode(node);
  node = StyledNode();
  if (!node)
    return nullptr;
  // This is copied from CSSComputedStyleDeclaration::computeComputedStyle().
  // PseudoIdNone must be used if node() is a PseudoElement.
  const ComputedStyle* style = node->EnsureComputedStyle(
      node->IsPseudoElement() ? kPseudoIdNone : pseudo_id_);
  node = StyledNode();
  if (!node || !node->InActiveDocument() || !style)
    return nullptr;
  return style;
}

const CSSValue* ComputedStylePropertyMap::GetProperty(
    CSSPropertyID property_id) {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;
  return CSSProperty::Get(property_id)
      .CSSValueFromComputedStyle(*style, nullptr /* layout_object */,
                                 StyledNode(), false);
}

const CSSValue* ComputedStylePropertyMap::GetCustomProperty(
    AtomicString property_name) {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;
  return ComputedStyleCSSValueMapping::Get(
      property_name, *style, node_->GetDocument().GetPropertyRegistry());
}

void ComputedStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return;

  for (const CSSProperty* property :
       CSSComputedStyleDeclaration::ComputableProperties()) {
    DCHECK(property);
    DCHECK(!property->IDEquals(CSSPropertyVariable));
    const CSSValue* value = property->CSSValueFromComputedStyle(
        *style, nullptr /* layout_object */, StyledNode(), false);
    if (value)
      callback(property->GetPropertyName(), *value);
  }

  const auto& variables = ComputedStyleCSSValueMapping::GetVariables(*style);
  if (variables) {
    for (const auto& name_value : *variables) {
      if (name_value.value) {
        callback(name_value.key, *CSSCustomPropertyDeclaration::Create(
                                     name_value.key, name_value.value));
      }
    }
  }
}

}  // namespace blink
