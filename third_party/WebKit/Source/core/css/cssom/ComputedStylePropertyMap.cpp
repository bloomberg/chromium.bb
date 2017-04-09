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

CSSStyleValue* StyleValueForLength(const Length& length) {
  if (length.IsAuto()) {
    return CSSKeywordValue::Create("auto");
  }
  if (length.IsFixed()) {
    return CSSSimpleLength::Create(length.Pixels(),
                                   CSSPrimitiveValue::UnitType::kPixels);
  }
  if (length.IsPercent()) {
    return CSSSimpleLength::Create(length.Percent(),
                                   CSSPrimitiveValue::UnitType::kPercentage);
  }
  if (length.IsCalculated()) {
    return CSSCalcLength::FromLength(length);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

Node* ComputedStylePropertyMap::GetNode() const {
  if (!node_) {
    return nullptr;
  }
  if (!pseudo_id_) {
    return node_;
  }
  if (node_->IsElementNode()) {
    // Seems to only support before, after, backdrop, first-letter. See
    // PseudoElementData::pseudoElement.
    if (PseudoElement* element =
            (ToElement(node_))->GetPseudoElement(pseudo_id_)) {
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
CSSStyleValueVector ComputedStylePropertyMap::GetAllInternal(
    CSSPropertyID property_id) {
  CSSStyleValueVector style_value_vector;

  Node* node = this->GetNode();
  if (!node || !node->InActiveDocument()) {
    return style_value_vector;
  }

  // Update style before getting the value for the property
  node->GetDocument().UpdateStyleAndLayoutTreeForNode(node);
  node = this->GetNode();
  if (!node) {
    return style_value_vector;
  }
  // I have copied this from
  // CSSComputedStyleDeclaration::computeComputedStyle(). I don't know if there
  // is any use in passing m_pseudoId if node is not already a PseudoElement,
  // but passing pseudo_Id when it IS already a PseudoElement leads to disaster.
  const ComputedStyle* style = node->EnsureComputedStyle(
      node->IsPseudoElement() ? kPseudoIdNone : pseudo_id_);
  node = this->GetNode();
  if (!node || !node->InActiveDocument() || !style) {
    return style_value_vector;
  }

  CSSStyleValue* style_value = nullptr;

  switch (property_id) {
    // TODO(rjwright): Generate this code.
    case CSSPropertyLeft:
      style_value = StyleValueForLength(style->Left());
      break;
    case CSSPropertyRight:
      style_value = StyleValueForLength(style->Right());
      break;
    case CSSPropertyTop:
      style_value = StyleValueForLength(style->Top());
      break;
    case CSSPropertyBottom:
      style_value = StyleValueForLength(style->Bottom());
      break;
    case CSSPropertyHeight:
      style_value = StyleValueForLength(style->Height());
      break;
    case CSSPropertyWidth:
      style_value = StyleValueForLength(style->Width());
      break;
    case CSSPropertyLineHeight: {
      // LineHeight is represented as a Length in ComputedStyle, even though it
      // can be a number or the "normal" keyword. "normal" is encoded as a
      // negative percent, and numbers (which must be positive) are encoded as
      // percents.
      Length line_height = style->LineHeight();
      if (line_height.IsNegative()) {
        style_value = CSSKeywordValue::Create("normal");
        break;
      }
      if (line_height.IsPercent()) {
        style_value = CSSNumberValue::Create(line_height.Percent());
        break;
      }
      if (line_height.IsFixed()) {
        style_value = CSSSimpleLength::Create(
            line_height.Pixels(), CSSPrimitiveValue::UnitType::kPixels);
        break;
      }
      NOTREACHED();
      break;
    }
    default:
      // For properties not yet handled above, fall back to using resolved
      // style.
      const CSSValue* value = ComputedStyleCSSValueMapping::Get(
          property_id, *style, nullptr, node, false);
      if (value) {
        return StyleValueFactory::CssValueToStyleValueVector(property_id,
                                                             *value);
      }
      break;
  }

  if (style_value) {
    style_value_vector.push_back(style_value);
  }
  return style_value_vector;
}

CSSStyleValueVector ComputedStylePropertyMap::GetAllInternal(
    AtomicString custom_property_name) {
  const CSSValue* css_value =
      computed_style_declaration_->GetPropertyCSSValue(custom_property_name);
  if (!css_value)
    return CSSStyleValueVector();
  return StyleValueFactory::CssValueToStyleValueVector(CSSPropertyInvalid,
                                                       *css_value);
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
