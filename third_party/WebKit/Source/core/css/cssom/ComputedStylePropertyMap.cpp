// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/ComputedStylePropertyMap.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "core/css/cssom/CSSKeywordValue.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/StyleEngine.h"

namespace blink {

namespace {

CSSStyleValue* styleValueForLength(const Length& length) {
  if (length.isAuto()) {
    return CSSKeywordValue::create("auto");
  }
  if (length.isFixed()) {
    return CSSSimpleLength::create(length.pixels(),
                                   CSSPrimitiveValue::UnitType::Pixels);
  }
  if (length.isPercent()) {
    return CSSSimpleLength::create(length.percent(),
                                   CSSPrimitiveValue::UnitType::Percentage);
  }
  if (length.isCalculated()) {
    return CSSCalcLength::fromLength(length);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

Node* ComputedStylePropertyMap::node() const {
  if (!m_node) {
    return nullptr;
  }
  if (!m_pseudoId) {
    return m_node;
  }
  if (m_node->isElementNode()) {
    // Seems to only support before, after, backdrop, first-letter. See
    // PseudoElementData::pseudoElement.
    if (PseudoElement* element =
            (toElement(m_node))->pseudoElement(m_pseudoId)) {
      return element;
    }
  }
  return nullptr;
}

CSSStyleValueVector ComputedStylePropertyMap::getAllInternal(
    CSSPropertyID propertyID) {
  CSSStyleValueVector styleValueVector;

  Node* node = this->node();
  if (!node || !node->inActiveDocument()) {
    return styleValueVector;
  }
  node->document().updateStyleAndLayoutTreeForNode(node);
  node = this->node();
  if (!node) {
    return styleValueVector;
  }
  // I have copied this from
  // CSSComputedStyleDeclaration::computeComputedStyle(). I don't know if
  // there is any use in passing m_pseudoId if node is not already a
  // PseudoElement, but passing
  // pseudo_Id when it IS already a PseudoElement leads to disaster.
  const ComputedStyle* style = node->ensureComputedStyle(
      node->isPseudoElement() ? PseudoIdNone : m_pseudoId);
  node = this->node();
  if (!node || !node->inActiveDocument() || !style) {
    return styleValueVector;
  }

  CSSStyleValue* styleValue = nullptr;

  switch (propertyID) {
    // TODO(rjwright): Generate this code.
    case CSSPropertyLeft:
      styleValue = styleValueForLength(style->left());
      break;
    case CSSPropertyRight:
      styleValue = styleValueForLength(style->right());
      break;
    case CSSPropertyTop:
      styleValue = styleValueForLength(style->top());
      break;
    case CSSPropertyBottom:
      styleValue = styleValueForLength(style->bottom());
      break;
    case CSSPropertyHeight:
      styleValue = styleValueForLength(style->height());
      break;
    case CSSPropertyWidth:
      styleValue = styleValueForLength(style->width());
      break;
    default:
      // TODO(rjwright): Add a flag argument to
      // ComputedStyleCSSValyeMapping::get that makes it return
      // just the raw value off the ComputedStyle, and not zoom adjusted or
      // anything like that.
      const CSSValue* value = ComputedStyleCSSValueMapping::get(
          propertyID, *style, nullptr, node, false);
      if (value) {
        return StyleValueFactory::cssValueToStyleValueVector(propertyID,
                                                             *value);
      }
      break;
  }

  if (styleValue) {
    styleValueVector.push_back(styleValue);
  }
  return styleValueVector;
}

CSSStyleValueVector ComputedStylePropertyMap::getAllInternal(
    AtomicString customPropertyName) {
  const CSSValue* cssValue =
      m_computedStyleDeclaration->getPropertyCSSValue(customPropertyName);
  if (!cssValue)
    return CSSStyleValueVector();
  return StyleValueFactory::cssValueToStyleValueVector(CSSPropertyInvalid,
                                                       *cssValue);
}

Vector<String> ComputedStylePropertyMap::getProperties() {
  Vector<String> result;
  for (unsigned i = 0; i < m_computedStyleDeclaration->length(); i++) {
    result.push_back(m_computedStyleDeclaration->item(i));
  }
  return result;
}

}  // namespace blink
