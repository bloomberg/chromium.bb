// Copyright 2016 the Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/ComputedStylePropertyMap.h"

#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "core/css/cssom/CSSKeywordValue.h"
#include "core/css/cssom/CSSNumberValue.h"
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

// ComputedStylePropertyMap::getAllInternal/get should return computed styles
// (as opposed to resolved styles a la getComputedStyle()).
//
// Property values are read from an up-to-date ComputedStyle and converted into
// CSSStyleValues. This has not been implemented for all properties yet.
// Unsupported properties fall back to using resolved styles & converting them
// to CSSStyleValues via StyleValueFactory. For some types of values, such as
// images, the difference between the two is minor.
CSSStyleValueVector ComputedStylePropertyMap::getAllInternal(
    CSSPropertyID propertyID) {
  CSSStyleValueVector styleValueVector;

  Node* node = this->node();
  if (!node || !node->inActiveDocument()) {
    return styleValueVector;
  }

  // Update style before getting the value for the property
  node->document().updateStyleAndLayoutTreeForNode(node);
  node = this->node();
  if (!node) {
    return styleValueVector;
  }
  // I have copied this from
  // CSSComputedStyleDeclaration::computeComputedStyle(). I don't know if there
  // is any use in passing m_pseudoId if node is not already a PseudoElement,
  // but passing pseudo_Id when it IS already a PseudoElement leads to disaster.
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
    case CSSPropertyLineHeight: {
      // LineHeight is represented as a Length in ComputedStyle, even though it
      // can be a number or the "normal" keyword. "normal" is encoded as a
      // negative percent, and numbers (which must be positive) are encoded as
      // percents.
      Length lineHeight = style->lineHeight();
      if (lineHeight.isNegative()) {
        styleValue = CSSKeywordValue::create("normal");
        break;
      }
      if (lineHeight.isPercent()) {
        styleValue = CSSNumberValue::create(lineHeight.percent());
        break;
      }
      if (lineHeight.isFixed()) {
        styleValue = CSSSimpleLength::create(
            lineHeight.pixels(), CSSPrimitiveValue::UnitType::Pixels);
        break;
      }
      NOTREACHED();
      break;
    }
    default:
      // For properties not yet handled above, fall back to using resolved
      // style.
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
