// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/ComputedStylePropertyMap.h"

#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/dom/PseudoElement.h"

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
  return ComputedStyleCSSValueMapping::Get(property_id, *style,
                                           nullptr /* layout_object */);
}

const CSSValue* ComputedStylePropertyMap::GetCustomProperty(
    AtomicString property_name) {
  const ComputedStyle* style = UpdateStyle();
  if (!style)
    return nullptr;
  return ComputedStyleCSSValueMapping::Get(
      property_name, *style, node_->GetDocument().GetPropertyRegistry());
}

Vector<String> ComputedStylePropertyMap::getProperties() {
  Vector<String> result;
  for (CSSPropertyID property_id :
       CSSComputedStyleDeclaration::ComputableProperties()) {
    result.push_back(getPropertyNameString(property_id));
  }
  return result;
}

}  // namespace blink
