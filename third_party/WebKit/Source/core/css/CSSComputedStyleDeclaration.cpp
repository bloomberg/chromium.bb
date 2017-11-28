/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "core/css/CSSComputedStyleDeclaration.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSVariableData.h"
#include "core/css/ComputedStyleCSSValueMapping.h"
#include "core/css/StyleEngine.h"
#include "core/css/ZoomAdjustedPixelValue.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/PseudoElement.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

// List of all properties we know how to compute, omitting shorthands.
// NOTE: Do not use this list, use computableProperties() instead
// to respect runtime enabling of CSS properties.
const CSSPropertyID kComputedPropertyArray[] = {
    CSSPropertyAnimationDelay, CSSPropertyAnimationDirection,
    CSSPropertyAnimationDuration, CSSPropertyAnimationFillMode,
    CSSPropertyAnimationIterationCount, CSSPropertyAnimationName,
    CSSPropertyAnimationPlayState, CSSPropertyAnimationTimingFunction,
    CSSPropertyBackgroundAttachment, CSSPropertyBackgroundBlendMode,
    CSSPropertyBackgroundClip, CSSPropertyBackgroundColor,
    CSSPropertyBackgroundImage, CSSPropertyBackgroundOrigin,
    // more-specific background-position-x/y are non-standard
    CSSPropertyBackgroundPosition, CSSPropertyBackgroundRepeat,
    CSSPropertyBackgroundSize, CSSPropertyBorderBottomColor,
    CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderBottomRightRadius,
    CSSPropertyBorderBottomStyle, CSSPropertyBorderBottomWidth,
    CSSPropertyBorderCollapse, CSSPropertyBorderImageOutset,
    CSSPropertyBorderImageRepeat, CSSPropertyBorderImageSlice,
    CSSPropertyBorderImageSource, CSSPropertyBorderImageWidth,
    CSSPropertyBorderLeftColor, CSSPropertyBorderLeftStyle,
    CSSPropertyBorderLeftWidth, CSSPropertyBorderRightColor,
    CSSPropertyBorderRightStyle, CSSPropertyBorderRightWidth,
    CSSPropertyBorderTopColor, CSSPropertyBorderTopLeftRadius,
    CSSPropertyBorderTopRightRadius, CSSPropertyBorderTopStyle,
    CSSPropertyBorderTopWidth, CSSPropertyBottom, CSSPropertyBoxShadow,
    CSSPropertyBoxSizing, CSSPropertyBreakAfter, CSSPropertyBreakBefore,
    CSSPropertyBreakInside, CSSPropertyCaptionSide, CSSPropertyClear,
    CSSPropertyClip, CSSPropertyColor, CSSPropertyContent, CSSPropertyCursor,
    CSSPropertyDirection, CSSPropertyDisplay, CSSPropertyEmptyCells,
    CSSPropertyFloat, CSSPropertyFontFamily, CSSPropertyFontKerning,
    CSSPropertyFontSize, CSSPropertyFontSizeAdjust, CSSPropertyFontStretch,
    CSSPropertyFontStyle, CSSPropertyFontVariant,
    CSSPropertyFontVariantLigatures, CSSPropertyFontVariantCaps,
    CSSPropertyFontVariantNumeric, CSSPropertyFontVariantEastAsian,
    CSSPropertyFontWeight, CSSPropertyHeight, CSSPropertyImageOrientation,
    CSSPropertyImageRendering, CSSPropertyIsolation, CSSPropertyJustifyItems,
    CSSPropertyJustifySelf, CSSPropertyLeft, CSSPropertyLetterSpacing,
    CSSPropertyLineHeight, CSSPropertyLineHeightStep, CSSPropertyListStyleImage,
    CSSPropertyListStylePosition, CSSPropertyListStyleType,
    CSSPropertyMarginBottom, CSSPropertyMarginLeft, CSSPropertyMarginRight,
    CSSPropertyMarginTop, CSSPropertyMaxHeight, CSSPropertyMaxWidth,
    CSSPropertyMinHeight, CSSPropertyMinWidth, CSSPropertyMixBlendMode,
    CSSPropertyObjectFit, CSSPropertyObjectPosition, CSSPropertyOffsetAnchor,
    CSSPropertyOffsetDistance, CSSPropertyOffsetPath, CSSPropertyOffsetPosition,
    CSSPropertyOffsetRotate, CSSPropertyOpacity, CSSPropertyOrphans,
    CSSPropertyOutlineColor, CSSPropertyOutlineOffset, CSSPropertyOutlineStyle,
    CSSPropertyOutlineWidth, CSSPropertyOverflowAnchor, CSSPropertyOverflowWrap,
    CSSPropertyOverflowX, CSSPropertyOverflowY, CSSPropertyPaddingBottom,
    CSSPropertyPaddingLeft, CSSPropertyPaddingRight, CSSPropertyPaddingTop,
    CSSPropertyPointerEvents, CSSPropertyPosition, CSSPropertyResize,
    CSSPropertyRight, CSSPropertyScrollBehavior, CSSPropertySpeak,
    CSSPropertyTableLayout, CSSPropertyTabSize, CSSPropertyTextAlign,
    CSSPropertyTextAlignLast, CSSPropertyTextDecoration,
    CSSPropertyTextDecorationLine, CSSPropertyTextDecorationStyle,
    CSSPropertyTextDecorationColor, CSSPropertyTextDecorationSkipInk,
    CSSPropertyTextJustify, CSSPropertyTextUnderlinePosition,
    CSSPropertyTextIndent, CSSPropertyTextRendering, CSSPropertyTextShadow,
    CSSPropertyTextSizeAdjust, CSSPropertyTextOverflow,
    CSSPropertyTextTransform, CSSPropertyTop, CSSPropertyTouchAction,
    CSSPropertyTransitionDelay, CSSPropertyTransitionDuration,
    CSSPropertyTransitionProperty, CSSPropertyTransitionTimingFunction,
    CSSPropertyUnicodeBidi, CSSPropertyVerticalAlign, CSSPropertyVisibility,
    CSSPropertyWhiteSpace, CSSPropertyWidows, CSSPropertyWidth,
    CSSPropertyWillChange, CSSPropertyWordBreak, CSSPropertyWordSpacing,
    CSSPropertyWordWrap, CSSPropertyZIndex, CSSPropertyZoom,

    CSSPropertyWebkitAppearance, CSSPropertyBackfaceVisibility,
    CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyWebkitBorderImage,
    CSSPropertyWebkitBorderVerticalSpacing, CSSPropertyWebkitBoxAlign,
    CSSPropertyWebkitBoxDecorationBreak, CSSPropertyWebkitBoxDirection,
    CSSPropertyWebkitBoxFlex, CSSPropertyWebkitBoxFlexGroup,
    CSSPropertyWebkitBoxLines, CSSPropertyWebkitBoxOrdinalGroup,
    CSSPropertyWebkitBoxOrient, CSSPropertyWebkitBoxPack,
    CSSPropertyWebkitBoxReflect, CSSPropertyColumnCount, CSSPropertyColumnGap,
    CSSPropertyColumnRuleColor, CSSPropertyColumnRuleStyle,
    CSSPropertyColumnRuleWidth, CSSPropertyColumnSpan, CSSPropertyColumnWidth,
    CSSPropertyBackdropFilter, CSSPropertyAlignContent, CSSPropertyAlignItems,
    CSSPropertyAlignSelf, CSSPropertyFlexBasis, CSSPropertyFlexGrow,
    CSSPropertyFlexShrink, CSSPropertyFlexDirection, CSSPropertyFlexWrap,
    CSSPropertyJustifyContent, CSSPropertyWebkitFontSmoothing,
    CSSPropertyGridAutoColumns, CSSPropertyGridAutoFlow,
    CSSPropertyGridAutoRows, CSSPropertyGridColumnEnd,
    CSSPropertyGridColumnStart, CSSPropertyGridTemplateAreas,
    CSSPropertyGridTemplateColumns, CSSPropertyGridTemplateRows,
    CSSPropertyGridRowEnd, CSSPropertyGridRowStart, CSSPropertyGridColumnGap,
    CSSPropertyGridRowGap, CSSPropertyWebkitHighlight, CSSPropertyHyphens,
    CSSPropertyWebkitHyphenateCharacter, CSSPropertyWebkitLineBreak,
    CSSPropertyWebkitLineClamp, CSSPropertyWebkitLocale,
    CSSPropertyWebkitMarginBeforeCollapse, CSSPropertyWebkitMarginAfterCollapse,
    CSSPropertyWebkitMaskBoxImage, CSSPropertyWebkitMaskBoxImageOutset,
    CSSPropertyWebkitMaskBoxImageRepeat, CSSPropertyWebkitMaskBoxImageSlice,
    CSSPropertyWebkitMaskBoxImageSource, CSSPropertyWebkitMaskBoxImageWidth,
    CSSPropertyWebkitMaskClip, CSSPropertyWebkitMaskComposite,
    CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskOrigin,
    CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskRepeat,
    CSSPropertyWebkitMaskSize, CSSPropertyOrder, CSSPropertyPerspective,
    CSSPropertyPerspectiveOrigin, CSSPropertyWebkitPrintColorAdjust,
    CSSPropertyWebkitRtlOrdering, CSSPropertyShapeOutside,
    CSSPropertyShapeImageThreshold, CSSPropertyShapeMargin,
    CSSPropertyWebkitTapHighlightColor, CSSPropertyWebkitTextCombine,
    CSSPropertyWebkitTextDecorationsInEffect,
    CSSPropertyWebkitTextEmphasisColor, CSSPropertyWebkitTextEmphasisPosition,
    CSSPropertyWebkitTextEmphasisStyle, CSSPropertyWebkitTextFillColor,
    CSSPropertyWebkitTextOrientation, CSSPropertyWebkitTextSecurity,
    CSSPropertyWebkitTextStrokeColor, CSSPropertyWebkitTextStrokeWidth,
    CSSPropertyTransform, CSSPropertyTransformOrigin, CSSPropertyTransformStyle,
    CSSPropertyWebkitUserDrag, CSSPropertyWebkitUserModify,
    CSSPropertyUserSelect, CSSPropertyWebkitWritingMode,
    CSSPropertyWebkitAppRegion, CSSPropertyBufferedRendering,
    CSSPropertyClipPath, CSSPropertyClipRule, CSSPropertyMask,
    CSSPropertyFilter, CSSPropertyFloodColor, CSSPropertyFloodOpacity,
    CSSPropertyLightingColor, CSSPropertyStopColor, CSSPropertyStopOpacity,
    CSSPropertyColorInterpolation, CSSPropertyColorInterpolationFilters,
    CSSPropertyColorRendering, CSSPropertyFill, CSSPropertyFillOpacity,
    CSSPropertyFillRule, CSSPropertyMarkerEnd, CSSPropertyMarkerMid,
    CSSPropertyMarkerStart, CSSPropertyMaskType, CSSPropertyMaskSourceType,
    CSSPropertyShapeRendering, CSSPropertyStroke, CSSPropertyStrokeDasharray,
    CSSPropertyStrokeDashoffset, CSSPropertyStrokeLinecap,
    CSSPropertyStrokeLinejoin, CSSPropertyStrokeMiterlimit,
    CSSPropertyStrokeOpacity, CSSPropertyStrokeWidth,
    CSSPropertyAlignmentBaseline, CSSPropertyBaselineShift,
    CSSPropertyDominantBaseline, CSSPropertyTextAnchor, CSSPropertyWritingMode,
    CSSPropertyVectorEffect, CSSPropertyPaintOrder, CSSPropertyD, CSSPropertyCx,
    CSSPropertyCy, CSSPropertyX, CSSPropertyY, CSSPropertyR, CSSPropertyRx,
    CSSPropertyRy, CSSPropertyTranslate, CSSPropertyRotate, CSSPropertyScale,
    CSSPropertyCaretColor, CSSPropertyLineBreak};

CSSValueID CssIdentifierForFontSizeKeyword(int keyword_size) {
  DCHECK_NE(keyword_size, 0);
  DCHECK_LE(keyword_size, 8);
  return static_cast<CSSValueID>(CSSValueXxSmall + keyword_size - 1);
}

void LogUnimplementedPropertyID(CSSPropertyID property_id) {
  DEFINE_STATIC_LOCAL(HashSet<CSSPropertyID>, property_id_set, ());
  if (!property_id_set.insert(property_id).is_new_entry)
    return;

  DLOG(ERROR) << "Blink does not yet implement getComputedStyle for '"
              << getPropertyName(property_id) << "'.";
}

}  // namespace

const Vector<const CSSProperty*>&
CSSComputedStyleDeclaration::ComputableProperties() {
  DEFINE_STATIC_LOCAL(Vector<const CSSProperty*>, properties, ());
  if (properties.IsEmpty()) {
    CSSProperty::FilterEnabledCSSPropertiesIntoVector(
        kComputedPropertyArray, WTF_ARRAY_LENGTH(kComputedPropertyArray),
        properties);
  }
  return properties;
}

CSSComputedStyleDeclaration::CSSComputedStyleDeclaration(
    Node* n,
    bool allow_visited_style,
    const String& pseudo_element_name)
    : node_(n),
      pseudo_element_specifier_(
          CSSSelector::ParsePseudoId(pseudo_element_name)),
      allow_visited_style_(allow_visited_style) {}

CSSComputedStyleDeclaration::~CSSComputedStyleDeclaration() = default;

String CSSComputedStyleDeclaration::cssText() const {
  StringBuilder result;
  static const Vector<const CSSProperty*>& properties = ComputableProperties();

  for (unsigned i = 0; i < properties.size(); i++) {
    if (i)
      result.Append(' ');
    result.Append(getPropertyName(properties[i]->PropertyID()));
    result.Append(": ");
    result.Append(GetPropertyValue(properties[i]->PropertyID()));
    result.Append(';');
  }

  return result.ToString();
}

void CSSComputedStyleDeclaration::setCSSText(const ExecutionContext*,
                                             const String&,
                                             ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      kNoModificationAllowedError,
      "These styles are computed, and therefore read-only.");
}

const CSSValue*
CSSComputedStyleDeclaration::GetFontSizeCSSValuePreferringKeyword() const {
  if (!node_)
    return nullptr;

  node_->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  const ComputedStyle* style =
      node_->EnsureComputedStyle(pseudo_element_specifier_);
  if (!style)
    return nullptr;

  if (int keyword_size = style->GetFontDescription().KeywordSize()) {
    return CSSIdentifierValue::Create(
        CssIdentifierForFontSizeKeyword(keyword_size));
  }

  return ZoomAdjustedPixelValue(style->GetFontDescription().ComputedPixelSize(),
                                *style);
}

bool CSSComputedStyleDeclaration::IsMonospaceFont() const {
  if (!node_)
    return false;

  const ComputedStyle* style =
      node_->EnsureComputedStyle(pseudo_element_specifier_);
  if (!style)
    return false;

  return style->GetFontDescription().IsMonospace();
}
const ComputedStyle* CSSComputedStyleDeclaration::ComputeComputedStyle() const {
  Node* styled_node = this->StyledNode();
  DCHECK(styled_node);
  return styled_node->EnsureComputedStyle(styled_node->IsPseudoElement()
                                              ? kPseudoIdNone
                                              : pseudo_element_specifier_);
}

Node* CSSComputedStyleDeclaration::StyledNode() const {
  if (!node_)
    return nullptr;
  if (node_->IsElementNode()) {
    if (PseudoElement* element =
            ToElement(node_)->GetPseudoElement(pseudo_element_specifier_))
      return element;
  }
  return node_.Get();
}

LayoutObject* CSSComputedStyleDeclaration::StyledLayoutObject() const {
  auto* node = StyledNode();
  if (!node)
    return nullptr;

  if (pseudo_element_specifier_ != kPseudoIdNone && node == node_.Get())
    return nullptr;

  return node->GetLayoutObject();
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValue(
    AtomicString custom_property_name) const {
  Node* styled_node = StyledNode();
  if (!styled_node)
    return nullptr;

  styled_node->GetDocument().UpdateStyleAndLayoutTreeForNode(styled_node);

  const ComputedStyle* style = ComputeComputedStyle();
  if (!style)
    return nullptr;
  // Don't use styled_node in case it was discarded or replaced in
  // UpdateStyleAndLayoutTreeForNode.
  return ComputedStyleCSSValueMapping::Get(
      custom_property_name, *style,
      StyledNode()->GetDocument().GetPropertyRegistry());
}

std::unique_ptr<HashMap<AtomicString, scoped_refptr<CSSVariableData>>>
CSSComputedStyleDeclaration::GetVariables() const {
  const ComputedStyle* style = ComputeComputedStyle();
  if (!style)
    return nullptr;
  return ComputedStyleCSSValueMapping::GetVariables(*style);
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValue(
    const CSSProperty& property_class) const {
  Node* styled_node = StyledNode();
  if (!styled_node)
    return nullptr;

  Document& document = styled_node->GetDocument();
  document.UpdateStyleAndLayoutTreeForNode(styled_node);

  // The style recalc could have caused the styled node to be discarded or
  // replaced if it was a PseudoElement so we need to update it.
  styled_node = StyledNode();
  LayoutObject* layout_object = StyledLayoutObject();

  const ComputedStyle* style = ComputeComputedStyle();

  bool force_full_layout =
      property_class.IsLayoutDependent(style, layout_object) ||
      styled_node->IsInShadowTree() ||
      (document.LocalOwner() &&
       document.GetStyleEngine().HasViewportDependentMediaQueries());

  if (force_full_layout) {
    document.UpdateStyleAndLayoutIgnorePendingStylesheetsForNode(styled_node);
    styled_node = StyledNode();
    style = ComputeComputedStyle();
    layout_object = StyledLayoutObject();
  }

  if (!style)
    return nullptr;

  const CSSValue* value = ComputedStyleCSSValueMapping::Get(
      property_class, *style, layout_object, styled_node, allow_visited_style_);
  if (value)
    return value;

  LogUnimplementedPropertyID(property_class.PropertyID());
  return nullptr;
}

String CSSComputedStyleDeclaration::GetPropertyValue(
    CSSPropertyID property_id) const {
  const CSSValue* value = GetPropertyCSSValue(CSSProperty::Get(property_id));
  if (value)
    return value->CssText();
  return "";
}

unsigned CSSComputedStyleDeclaration::length() const {
  if (!node_ || !node_->InActiveDocument())
    return 0;
  return ComputableProperties().size();
}

String CSSComputedStyleDeclaration::item(unsigned i) const {
  if (i >= length())
    return "";

  return getPropertyNameString(ComputableProperties()[i]->PropertyID());
}

bool CSSComputedStyleDeclaration::CssPropertyMatches(
    CSSPropertyID property_id,
    const CSSValue* property_value) const {
  if (property_id == CSSPropertyFontSize &&
      (property_value->IsPrimitiveValue() ||
       property_value->IsIdentifierValue()) &&
      node_) {
    node_->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
    const ComputedStyle* style =
        node_->EnsureComputedStyle(pseudo_element_specifier_);
    if (style && style->GetFontDescription().KeywordSize()) {
      CSSValueID size_value = CssIdentifierForFontSizeKeyword(
          style->GetFontDescription().KeywordSize());
      if (property_value->IsIdentifierValue() &&
          ToCSSIdentifierValue(property_value)->GetValueID() == size_value)
        return true;
    }
  }
  const CSSValue* value = GetPropertyCSSValue(CSSProperty::Get(property_id));
  return DataEquivalent(value, property_value);
}

MutableCSSPropertyValueSet* CSSComputedStyleDeclaration::CopyProperties()
    const {
  return CopyPropertiesInSet(ComputableProperties());
}

MutableCSSPropertyValueSet* CSSComputedStyleDeclaration::CopyPropertiesInSet(
    const Vector<const CSSProperty*>& properties) const {
  HeapVector<CSSPropertyValue, 256> list;
  list.ReserveInitialCapacity(properties.size());
  for (unsigned i = 0; i < properties.size(); ++i) {
    const CSSProperty& property = *properties[i];
    const CSSValue* value = GetPropertyCSSValue(property);
    if (value)
      list.push_back(CSSPropertyValue(property, *value, false));
  }
  return MutableCSSPropertyValueSet::Create(list.data(), list.size());
}

CSSRule* CSSComputedStyleDeclaration::parentRule() const {
  return nullptr;
}

String CSSComputedStyleDeclaration::getPropertyValue(
    const String& property_name) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (!property_id)
    return String();
  if (property_id == CSSPropertyVariable) {
    const CSSValue* value = GetPropertyCSSValue(AtomicString(property_name));
    if (value)
      return value->CssText();
    return String();
  }
#if DCHECK_IS_ON
  DCHECK(CSSProperty::Get(property_id).IsEnabled());
#endif
  return GetPropertyValue(property_id);
}

String CSSComputedStyleDeclaration::getPropertyPriority(const String&) {
  // All computed styles have a priority of not "important".
  return "";
}

String CSSComputedStyleDeclaration::GetPropertyShorthand(const String&) {
  return "";
}

bool CSSComputedStyleDeclaration::IsPropertyImplicit(const String&) {
  return false;
}

void CSSComputedStyleDeclaration::setProperty(const ExecutionContext*,
                                              const String& name,
                                              const String&,
                                              const String&,
                                              ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      kNoModificationAllowedError,
      "These styles are computed, and therefore the '" + name +
          "' property is read-only.");
}

String CSSComputedStyleDeclaration::removeProperty(
    const String& name,
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(
      kNoModificationAllowedError,
      "These styles are computed, and therefore the '" + name +
          "' property is read-only.");
  return String();
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValueInternal(
    CSSPropertyID property_id) {
  return GetPropertyCSSValue(CSSProperty::Get(property_id));
}

const CSSValue* CSSComputedStyleDeclaration::GetPropertyCSSValueInternal(
    AtomicString custom_property_name) {
  return GetPropertyCSSValue(custom_property_name);
}

String CSSComputedStyleDeclaration::GetPropertyValueInternal(
    CSSPropertyID property_id) {
  return GetPropertyValue(property_id);
}

void CSSComputedStyleDeclaration::SetPropertyInternal(
    CSSPropertyID id,
    const String&,
    const String&,
    bool,
    SecureContextMode,
    ExceptionState& exception_state) {
  // TODO(leviw): This code is currently unreachable, but shouldn't be.
  exception_state.ThrowDOMException(
      kNoModificationAllowedError,
      "These styles are computed, and therefore the '" +
          getPropertyNameString(id) + "' property is read-only.");
}

void CSSComputedStyleDeclaration::Trace(blink::Visitor* visitor) {
  visitor->Trace(node_);
  CSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
