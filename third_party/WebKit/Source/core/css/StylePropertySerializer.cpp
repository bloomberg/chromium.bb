/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "core/css/StylePropertySerializer.h"

#include "CSSValueKeywords.h"
#include "core/css/StylePropertyShorthand.h"
#include "core/page/RuntimeCSSEnabled.h"
#include <wtf/BitArray.h>
#include <wtf/text/StringBuilder.h>

using namespace std;

namespace WebCore {

static bool isInitialOrInherit(const String& value)
{
    DEFINE_STATIC_LOCAL(String, initial, ("initial"));
    DEFINE_STATIC_LOCAL(String, inherit, ("inherit"));
    return value.length() == 7 && (value == initial || value == inherit);
}

StylePropertySerializer::StylePropertySerializer(const StylePropertySet& properties)
    : m_propertySet(properties)
{
}

String StylePropertySerializer::asText() const
{
    StringBuilder result;

    int positionXPropertyIndex = -1;
    int positionYPropertyIndex = -1;
    int repeatXPropertyIndex = -1;
    int repeatYPropertyIndex = -1;

    BitArray<numCSSProperties> shorthandPropertyUsed;
    BitArray<numCSSProperties> shorthandPropertyAppeared;

    unsigned size = m_propertySet.propertyCount();
    unsigned numDecls = 0;
    for (unsigned n = 0; n < size; ++n) {
        StylePropertySet::PropertyReference property = m_propertySet.propertyAt(n);
        CSSPropertyID propertyID = property.id();
        // Only enabled properties should be part of the style.
        ASSERT(RuntimeCSSEnabled::isCSSPropertyEnabled(propertyID));
        CSSPropertyID shorthandPropertyID = CSSPropertyInvalid;
        CSSPropertyID borderFallbackShorthandProperty = CSSPropertyInvalid;
        String value;

        switch (propertyID) {
        case CSSPropertyVariable:
            if (numDecls++)
                result.append(' ');
            result.append(property.cssText());
            continue;
        case CSSPropertyBackgroundPositionX:
            positionXPropertyIndex = n;
            continue;
        case CSSPropertyBackgroundPositionY:
            positionYPropertyIndex = n;
            continue;
        case CSSPropertyBackgroundRepeatX:
            repeatXPropertyIndex = n;
            continue;
        case CSSPropertyBackgroundRepeatY:
            repeatYPropertyIndex = n;
            continue;
        case CSSPropertyBorderTopWidth:
        case CSSPropertyBorderRightWidth:
        case CSSPropertyBorderBottomWidth:
        case CSSPropertyBorderLeftWidth:
            if (!borderFallbackShorthandProperty)
                borderFallbackShorthandProperty = CSSPropertyBorderWidth;
        case CSSPropertyBorderTopStyle:
        case CSSPropertyBorderRightStyle:
        case CSSPropertyBorderBottomStyle:
        case CSSPropertyBorderLeftStyle:
            if (!borderFallbackShorthandProperty)
                borderFallbackShorthandProperty = CSSPropertyBorderStyle;
        case CSSPropertyBorderTopColor:
        case CSSPropertyBorderRightColor:
        case CSSPropertyBorderBottomColor:
        case CSSPropertyBorderLeftColor:
            if (!borderFallbackShorthandProperty)
                borderFallbackShorthandProperty = CSSPropertyBorderColor;

            // FIXME: Deal with cases where only some of border-(top|right|bottom|left) are specified.
            if (!shorthandPropertyAppeared.get(CSSPropertyBorder - firstCSSProperty)) {
                value = borderPropertyValue(ReturnNullOnUncommonValues);
                if (value.isNull())
                    shorthandPropertyAppeared.set(CSSPropertyBorder - firstCSSProperty);
                else
                    shorthandPropertyID = CSSPropertyBorder;
            } else if (shorthandPropertyUsed.get(CSSPropertyBorder - firstCSSProperty))
                shorthandPropertyID = CSSPropertyBorder;
            if (!shorthandPropertyID)
                shorthandPropertyID = borderFallbackShorthandProperty;
            break;
        case CSSPropertyWebkitBorderHorizontalSpacing:
        case CSSPropertyWebkitBorderVerticalSpacing:
            shorthandPropertyID = CSSPropertyBorderSpacing;
            break;
        case CSSPropertyFontFamily:
        case CSSPropertyLineHeight:
        case CSSPropertyFontSize:
        case CSSPropertyFontStyle:
        case CSSPropertyFontVariant:
        case CSSPropertyFontWeight:
            // Don't use CSSPropertyFont because old UAs can't recognize them but are important for editing.
            break;
        case CSSPropertyListStyleType:
        case CSSPropertyListStylePosition:
        case CSSPropertyListStyleImage:
            shorthandPropertyID = CSSPropertyListStyle;
            break;
        case CSSPropertyMarginTop:
        case CSSPropertyMarginRight:
        case CSSPropertyMarginBottom:
        case CSSPropertyMarginLeft:
            shorthandPropertyID = CSSPropertyMargin;
            break;
        case CSSPropertyOutlineWidth:
        case CSSPropertyOutlineStyle:
        case CSSPropertyOutlineColor:
            shorthandPropertyID = CSSPropertyOutline;
            break;
        case CSSPropertyOverflowX:
        case CSSPropertyOverflowY:
            shorthandPropertyID = CSSPropertyOverflow;
            break;
        case CSSPropertyPaddingTop:
        case CSSPropertyPaddingRight:
        case CSSPropertyPaddingBottom:
        case CSSPropertyPaddingLeft:
            shorthandPropertyID = CSSPropertyPadding;
            break;
        case CSSPropertyTransitionProperty:
        case CSSPropertyTransitionDuration:
        case CSSPropertyTransitionTimingFunction:
        case CSSPropertyTransitionDelay:
            shorthandPropertyID = CSSPropertyTransition;
            break;
        case CSSPropertyWebkitAnimationName:
        case CSSPropertyWebkitAnimationDuration:
        case CSSPropertyWebkitAnimationTimingFunction:
        case CSSPropertyWebkitAnimationDelay:
        case CSSPropertyWebkitAnimationIterationCount:
        case CSSPropertyWebkitAnimationDirection:
        case CSSPropertyWebkitAnimationFillMode:
            shorthandPropertyID = CSSPropertyWebkitAnimation;
            break;
        case CSSPropertyWebkitFlexDirection:
        case CSSPropertyWebkitFlexWrap:
            shorthandPropertyID = CSSPropertyWebkitFlexFlow;
            break;
        case CSSPropertyWebkitFlexBasis:
        case CSSPropertyWebkitFlexGrow:
        case CSSPropertyWebkitFlexShrink:
            shorthandPropertyID = CSSPropertyWebkitFlex;
            break;
        case CSSPropertyWebkitMaskPositionX:
        case CSSPropertyWebkitMaskPositionY:
        case CSSPropertyWebkitMaskRepeatX:
        case CSSPropertyWebkitMaskRepeatY:
        case CSSPropertyWebkitMaskImage:
        case CSSPropertyWebkitMaskRepeat:
        case CSSPropertyWebkitMaskPosition:
        case CSSPropertyWebkitMaskClip:
        case CSSPropertyWebkitMaskOrigin:
            shorthandPropertyID = CSSPropertyWebkitMask;
            break;
        case CSSPropertyWebkitTransformOriginX:
        case CSSPropertyWebkitTransformOriginY:
        case CSSPropertyWebkitTransformOriginZ:
            shorthandPropertyID = CSSPropertyWebkitTransformOrigin;
            break;
        case CSSPropertyWebkitTransitionProperty:
        case CSSPropertyWebkitTransitionDuration:
        case CSSPropertyWebkitTransitionTimingFunction:
        case CSSPropertyWebkitTransitionDelay:
            shorthandPropertyID = CSSPropertyWebkitTransition;
            break;
        case CSSPropertyWebkitWrapFlow:
        case CSSPropertyWebkitShapeMargin:
        case CSSPropertyWebkitShapePadding:
            shorthandPropertyID = CSSPropertyWebkitWrap;
            break;
        default:
            break;
        }

        unsigned shortPropertyIndex = shorthandPropertyID - firstCSSProperty;
        if (shorthandPropertyID) {
            if (shorthandPropertyUsed.get(shortPropertyIndex))
                continue;
            if (!shorthandPropertyAppeared.get(shortPropertyIndex) && value.isNull())
                value = m_propertySet.getPropertyValue(shorthandPropertyID);
            shorthandPropertyAppeared.set(shortPropertyIndex);
        }

        if (!value.isNull()) {
            propertyID = shorthandPropertyID;
            shorthandPropertyUsed.set(shortPropertyIndex);
        } else
            value = property.value()->cssText();

        if (value == "initial" && !CSSProperty::isInheritedProperty(propertyID))
            continue;

        if (numDecls++)
            result.append(' ');
        result.append(getPropertyName(propertyID));
        result.appendLiteral(": ");
        result.append(value);
        if (property.isImportant())
            result.appendLiteral(" !important");
        result.append(';');
    }

    // FIXME: This is a not-so-nice way to turn x/y positions into single background-position in output.
    // It is required because background-position-x/y are non-standard properties and WebKit generated output
    // would not work in Firefox (<rdar://problem/5143183>)
    // It would be a better solution if background-position was CSS_PAIR.
    if (positionXPropertyIndex != -1 && positionYPropertyIndex != -1 && m_propertySet.propertyAt(positionXPropertyIndex).isImportant() == m_propertySet.propertyAt(positionYPropertyIndex).isImportant()) {
        StylePropertySet::PropertyReference positionXProperty = m_propertySet.propertyAt(positionXPropertyIndex);
        StylePropertySet::PropertyReference positionYProperty = m_propertySet.propertyAt(positionYPropertyIndex);

        if (numDecls++)
            result.append(' ');
        result.appendLiteral("background-position: ");
        if (positionXProperty.value()->isValueList() || positionYProperty.value()->isValueList())
            result.append(getLayeredShorthandValue(backgroundPositionShorthand()));
        else {
            result.append(positionXProperty.value()->cssText());
            result.append(' ');
            result.append(positionYProperty.value()->cssText());
        }
        if (positionXProperty.isImportant())
            result.appendLiteral(" !important");
        result.append(';');
    } else {
        if (positionXPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(m_propertySet.propertyAt(positionXPropertyIndex).cssText());
        }
        if (positionYPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(m_propertySet.propertyAt(positionYPropertyIndex).cssText());
        }
    }

    // FIXME: We need to do the same for background-repeat.
    if (repeatXPropertyIndex != -1 && repeatYPropertyIndex != -1 && m_propertySet.propertyAt(repeatXPropertyIndex).isImportant() == m_propertySet.propertyAt(repeatYPropertyIndex).isImportant()) {
        StylePropertySet::PropertyReference repeatXProperty = m_propertySet.propertyAt(repeatXPropertyIndex);
        StylePropertySet::PropertyReference repeatYProperty = m_propertySet.propertyAt(repeatYPropertyIndex);

        if (numDecls++)
            result.append(' ');
        result.appendLiteral("background-repeat: ");
        if (repeatXProperty.value()->isValueList() || repeatYProperty.value()->isValueList())
            result.append(getLayeredShorthandValue(backgroundRepeatShorthand()));
        else {
            result.append(repeatXProperty.value()->cssText());
            result.append(' ');
            result.append(repeatYProperty.value()->cssText());
        }
        if (repeatXProperty.isImportant())
            result.appendLiteral(" !important");
        result.append(';');
    } else {
        if (repeatXPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(m_propertySet.propertyAt(repeatXPropertyIndex).cssText());
        }
        if (repeatYPropertyIndex != -1) {
            if (numDecls++)
                result.append(' ');
            result.append(m_propertySet.propertyAt(repeatYPropertyIndex).cssText());
        }
    }

    ASSERT(!numDecls ^ !result.isEmpty());
    return result.toString();
}

String StylePropertySerializer::getPropertyValue(CSSPropertyID propertyID) const
{
    // Shorthand and 4-values properties
    switch (propertyID) {
    case CSSPropertyBorderSpacing:
        return borderSpacingValue(borderSpacingShorthand());
    case CSSPropertyBackgroundPosition:
        return getLayeredShorthandValue(backgroundPositionShorthand());
    case CSSPropertyBackgroundRepeat:
        return getLayeredShorthandValue(backgroundRepeatShorthand());
    case CSSPropertyBackground:
        return getLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyBorder:
        return borderPropertyValue(OmitUncommonValues);
    case CSSPropertyBorderTop:
        return getShorthandValue(borderTopShorthand());
    case CSSPropertyBorderRight:
        return getShorthandValue(borderRightShorthand());
    case CSSPropertyBorderBottom:
        return getShorthandValue(borderBottomShorthand());
    case CSSPropertyBorderLeft:
        return getShorthandValue(borderLeftShorthand());
    case CSSPropertyOutline:
        return getShorthandValue(outlineShorthand());
    case CSSPropertyBorderColor:
        return get4Values(borderColorShorthand());
    case CSSPropertyBorderWidth:
        return get4Values(borderWidthShorthand());
    case CSSPropertyBorderStyle:
        return get4Values(borderStyleShorthand());
    case CSSPropertyWebkitColumnRule:
        return getShorthandValue(webkitColumnRuleShorthand());
    case CSSPropertyWebkitColumns:
        return getShorthandValue(webkitColumnsShorthand());
    case CSSPropertyWebkitFlex:
        return getShorthandValue(webkitFlexShorthand());
    case CSSPropertyWebkitFlexFlow:
        return getShorthandValue(webkitFlexFlowShorthand());
    case CSSPropertyWebkitGridColumn:
        return getShorthandValue(webkitGridColumnShorthand());
    case CSSPropertyWebkitGridRow:
        return getShorthandValue(webkitGridRowShorthand());
    case CSSPropertyFont:
        return fontValue();
    case CSSPropertyMargin:
        return get4Values(marginShorthand());
    case CSSPropertyWebkitMarginCollapse:
        return getShorthandValue(webkitMarginCollapseShorthand());
    case CSSPropertyOverflow:
        return getCommonValue(overflowShorthand());
    case CSSPropertyPadding:
        return get4Values(paddingShorthand());
    case CSSPropertyTransition:
        return getLayeredShorthandValue(transitionShorthand());
    case CSSPropertyListStyle:
        return getShorthandValue(listStyleShorthand());
    case CSSPropertyWebkitMarquee:
        return getShorthandValue(webkitMarqueeShorthand());
    case CSSPropertyWebkitMaskPosition:
        return getLayeredShorthandValue(webkitMaskPositionShorthand());
    case CSSPropertyWebkitMaskRepeat:
        return getLayeredShorthandValue(webkitMaskRepeatShorthand());
    case CSSPropertyWebkitMask:
        return getLayeredShorthandValue(webkitMaskShorthand());
    case CSSPropertyWebkitTextEmphasis:
        return getShorthandValue(webkitTextEmphasisShorthand());
    case CSSPropertyWebkitTextStroke:
        return getShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyWebkitTransformOrigin:
        return getShorthandValue(webkitTransformOriginShorthand());
    case CSSPropertyWebkitTransition:
        return getLayeredShorthandValue(webkitTransitionShorthand());
    case CSSPropertyWebkitAnimation:
        return getLayeredShorthandValue(webkitAnimationShorthand());
    case CSSPropertyWebkitWrap:
        return getShorthandValue(webkitWrapShorthand());
#if ENABLE(SVG)
    case CSSPropertyMarker: {
        RefPtr<CSSValue> value = m_propertySet.getPropertyCSSValue(CSSPropertyMarkerStart);
        if (value)
            return value->cssText();
        return String();
    }
#endif
    case CSSPropertyBorderRadius:
        return get4Values(borderRadiusShorthand());
    default:
        return String();
    }
}

String StylePropertySerializer::borderSpacingValue(const StylePropertyShorthand& shorthand) const
{
    RefPtr<CSSValue> horizontalValue = m_propertySet.getPropertyCSSValue(shorthand.properties()[0]);
    RefPtr<CSSValue> verticalValue = m_propertySet.getPropertyCSSValue(shorthand.properties()[1]);

    // While standard border-spacing property does not allow specifying border-spacing-vertical without
    // specifying border-spacing-horizontal <http://www.w3.org/TR/CSS21/tables.html#separated-borders>,
    // -webkit-border-spacing-vertical can be set without -webkit-border-spacing-horizontal.
    if (!horizontalValue || !verticalValue)
        return String();

    String horizontalValueCSSText = horizontalValue->cssText();
    String verticalValueCSSText = verticalValue->cssText();
    if (horizontalValueCSSText == verticalValueCSSText)
        return horizontalValueCSSText;
    return horizontalValueCSSText + ' ' + verticalValueCSSText;
}

void StylePropertySerializer::appendFontLonghandValueIfExplicit(CSSPropertyID propertyID, StringBuilder& result, String& commonValue) const
{
    int foundPropertyIndex = m_propertySet.findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return; // All longhands must have at least implicit values if "font" is specified.

    if (m_propertySet.propertyAt(foundPropertyIndex).isImplicit()) {
        commonValue = String();
        return;
    }

    char prefix = '\0';
    switch (propertyID) {
    case CSSPropertyFontStyle:
        break; // No prefix.
    case CSSPropertyFontFamily:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
        prefix = ' ';
        break;
    case CSSPropertyLineHeight:
        prefix = '/';
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (prefix && !result.isEmpty())
        result.append(prefix);
    String value = m_propertySet.propertyAt(foundPropertyIndex).value()->cssText();
    result.append(value);
    if (!commonValue.isNull() && commonValue != value)
        commonValue = String();
}

String StylePropertySerializer::fontValue() const
{
    int fontSizePropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontSize);
    int fontFamilyPropertyIndex = m_propertySet.findPropertyIndex(CSSPropertyFontFamily);
    if (fontSizePropertyIndex == -1 || fontFamilyPropertyIndex == -1)
        return emptyString();

    StylePropertySet::PropertyReference fontSizeProperty = m_propertySet.propertyAt(fontSizePropertyIndex);
    StylePropertySet::PropertyReference fontFamilyProperty = m_propertySet.propertyAt(fontFamilyPropertyIndex);
    if (fontSizeProperty.isImplicit() || fontFamilyProperty.isImplicit())
        return emptyString();

    String commonValue = fontSizeProperty.value()->cssText();
    StringBuilder result;
    appendFontLonghandValueIfExplicit(CSSPropertyFontStyle, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontVariant, result, commonValue);
    appendFontLonghandValueIfExplicit(CSSPropertyFontWeight, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontSizeProperty.value()->cssText());
    appendFontLonghandValueIfExplicit(CSSPropertyLineHeight, result, commonValue);
    if (!result.isEmpty())
        result.append(' ');
    result.append(fontFamilyProperty.value()->cssText());
    if (isInitialOrInherit(commonValue))
        return commonValue;
    return result.toString();
}

String StylePropertySerializer::get4Values(const StylePropertyShorthand& shorthand) const
{
    // Assume the properties are in the usual order top, right, bottom, left.
    int topValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[0]);
    int rightValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[1]);
    int bottomValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[2]);
    int leftValueIndex = m_propertySet.findPropertyIndex(shorthand.properties()[3]);

    if (topValueIndex == -1 || rightValueIndex == -1 || bottomValueIndex == -1 || leftValueIndex == -1)
        return String();

    StylePropertySet::PropertyReference top = m_propertySet.propertyAt(topValueIndex);
    StylePropertySet::PropertyReference right = m_propertySet.propertyAt(rightValueIndex);
    StylePropertySet::PropertyReference bottom = m_propertySet.propertyAt(bottomValueIndex);
    StylePropertySet::PropertyReference left = m_propertySet.propertyAt(leftValueIndex);

        // All 4 properties must be specified.
    if (!top.value() || !right.value() || !bottom.value() || !left.value())
        return String();

    if (top.isInherited() && right.isInherited() && bottom.isInherited() && left.isInherited())
        return getValueName(CSSValueInherit);

    if (top.value()->isInitialValue() || right.value()->isInitialValue() || bottom.value()->isInitialValue() || left.value()->isInitialValue()) {
        if (top.value()->isInitialValue() && right.value()->isInitialValue() && bottom.value()->isInitialValue() && left.value()->isInitialValue() && !top.isImplicit()) {
            // All components are "initial" and "top" is not implicit.
            return getValueName(CSSValueInitial);
        }
        return String();
    }
    if (top.isImportant() != right.isImportant() || right.isImportant() != bottom.isImportant() || bottom.isImportant() != left.isImportant())
        return String();

    bool showLeft = !right.value()->equals(*left.value());
    bool showBottom = !top.value()->equals(*bottom.value()) || showLeft;
    bool showRight = !top.value()->equals(*right.value()) || showBottom;

    StringBuilder result;
    result.append(top.value()->cssText());
    if (showRight) {
        result.append(' ');
        result.append(right.value()->cssText());
    }
    if (showBottom) {
        result.append(' ');
        result.append(bottom.value()->cssText());
    }
    if (showLeft) {
        result.append(' ');
        result.append(left.value()->cssText());
    }
    return result.toString();
}

String StylePropertySerializer::getLayeredShorthandValue(const StylePropertyShorthand& shorthand) const
{
    StringBuilder result;

    const unsigned size = shorthand.length();
    // Begin by collecting the properties into an array.
    Vector< RefPtr<CSSValue> > values(size);
    size_t numLayers = 0;

    for (unsigned i = 0; i < size; ++i) {
        values[i] = m_propertySet.getPropertyCSSValue(shorthand.properties()[i]);
        if (values[i]) {
            if (values[i]->isBaseValueList()) {
                CSSValueList* valueList = static_cast<CSSValueList*>(values[i].get());
                numLayers = max(valueList->length(), numLayers);
            } else
                numLayers = max<size_t>(1U, numLayers);
        }
    }

    String commonValue;
    bool commonValueInitialized = false;

    // Now stitch the properties together. Implicit initial values are flagged as such and
    // can safely be omitted.
    for (size_t i = 0; i < numLayers; i++) {
        StringBuilder layerResult;
        bool useRepeatXShorthand = false;
        bool useRepeatYShorthand = false;
        bool useSingleWordShorthand = false;
        bool foundPositionYCSSProperty = false;
        for (unsigned j = 0; j < size; j++) {
            RefPtr<CSSValue> value;
            if (values[j]) {
                if (values[j]->isBaseValueList())
                    value = static_cast<CSSValueList*>(values[j].get())->item(i);
                else {
                    value = values[j];

                    // Color only belongs in the last layer.
                    if (shorthand.properties()[j] == CSSPropertyBackgroundColor) {
                        if (i != numLayers - 1)
                            value = 0;
                    } else if (i) // Other singletons only belong in the first layer.
                        value = 0;
                }
            }

            // We need to report background-repeat as it was written in the CSS. If the property is implicit,
            // then it was written with only one value. Here we figure out which value that was so we can
            // report back correctly.
            if ((shorthand.properties()[j] == CSSPropertyBackgroundRepeatX && m_propertySet.isPropertyImplicit(shorthand.properties()[j]))
                || (shorthand.properties()[j] == CSSPropertyWebkitMaskRepeatX && m_propertySet.isPropertyImplicit(shorthand.properties()[j]))) {

                // BUG 49055: make sure the value was not reset in the layer check just above.
                if ((j < size - 1 && shorthand.properties()[j + 1] == CSSPropertyBackgroundRepeatY && value)
                    || (j < size - 1 && shorthand.properties()[j + 1] == CSSPropertyWebkitMaskRepeatY && value)) {
                    RefPtr<CSSValue> yValue;
                    RefPtr<CSSValue> nextValue = values[j + 1];
                    if (nextValue->isValueList())
                        yValue = static_cast<CSSValueList*>(nextValue.get())->itemWithoutBoundsCheck(i);
                    else
                        yValue = nextValue;

                    int xId = static_cast<CSSPrimitiveValue*>(value.get())->getIdent();
                    int yId = static_cast<CSSPrimitiveValue*>(yValue.get())->getIdent();
                    if (xId != yId) {
                        if (xId == CSSValueRepeat && yId == CSSValueNoRepeat) {
                            useRepeatXShorthand = true;
                            ++j;
                        } else if (xId == CSSValueNoRepeat && yId == CSSValueRepeat) {
                            useRepeatYShorthand = true;
                            continue;
                        }
                    } else {
                        useSingleWordShorthand = true;
                        ++j;
                    }
                }
            }

            String valueText;
            if (value && !value->isImplicitInitialValue()) {
                if (!layerResult.isEmpty())
                    layerResult.append(' ');
                if (foundPositionYCSSProperty
                    && (shorthand.properties()[j] == CSSPropertyBackgroundSize || shorthand.properties()[j] == CSSPropertyWebkitMaskSize))
                    layerResult.appendLiteral("/ ");
                if (!foundPositionYCSSProperty
                    && (shorthand.properties()[j] == CSSPropertyBackgroundSize || shorthand.properties()[j] == CSSPropertyWebkitMaskSize))
                    continue;

                if (useRepeatXShorthand) {
                    useRepeatXShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatX));
                } else if (useRepeatYShorthand) {
                    useRepeatYShorthand = false;
                    layerResult.append(getValueName(CSSValueRepeatY));
                } else {
                    if (useSingleWordShorthand)
                        useSingleWordShorthand = false;
                    valueText = value->cssText();
                    layerResult.append(valueText);
                }

                if (shorthand.properties()[j] == CSSPropertyBackgroundPositionY
                    || shorthand.properties()[j] == CSSPropertyWebkitMaskPositionY) {
                    foundPositionYCSSProperty = true;

                    // background-position is a special case: if only the first offset is specified,
                    // the second one defaults to "center", not the same value.
                    if (commonValueInitialized && commonValue != "initial" && commonValue != "inherit")
                        commonValue = String();
                }
            }

            if (!commonValueInitialized) {
                commonValue = valueText;
                commonValueInitialized = true;
            } else if (!commonValue.isNull() && commonValue != valueText)
                commonValue = String();
        }

        if (!layerResult.isEmpty()) {
            if (!result.isEmpty())
                result.appendLiteral(", ");
            result.append(layerResult);
        }
    }

    if (isInitialOrInherit(commonValue))
        return commonValue;

    if (result.isEmpty())
        return String();
    return result.toString();
}

String StylePropertySerializer::getShorthandValue(const StylePropertyShorthand& shorthand) const
{
    String commonValue;
    StringBuilder result;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!m_propertySet.isPropertyImplicit(shorthand.properties()[i])) {
            RefPtr<CSSValue> value = m_propertySet.getPropertyCSSValue(shorthand.properties()[i]);
            if (!value)
                return String();
            String valueText = value->cssText();
            if (!i)
                commonValue = valueText;
            else if (!commonValue.isNull() && commonValue != valueText)
                commonValue = String();
            if (value->isInitialValue())
                continue;
            if (!result.isEmpty())
                result.append(' ');
            result.append(valueText);
        } else
            commonValue = String();
    }
    if (isInitialOrInherit(commonValue))
        return commonValue;
    if (result.isEmpty())
        return String();
    return result.toString();
}

// only returns a non-null value if all properties have the same, non-null value
String StylePropertySerializer::getCommonValue(const StylePropertyShorthand& shorthand) const
{
    String res;
    bool lastPropertyWasImportant = false;
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        RefPtr<CSSValue> value = m_propertySet.getPropertyCSSValue(shorthand.properties()[i]);
        // FIXME: CSSInitialValue::cssText should generate the right value.
        if (!value)
            return String();
        String text = value->cssText();
        if (text.isNull())
            return String();
        if (res.isNull())
            res = text;
        else if (res != text)
            return String();

        bool currentPropertyIsImportant = m_propertySet.propertyIsImportant(shorthand.properties()[i]);
        if (i && lastPropertyWasImportant != currentPropertyIsImportant)
            return String();
        lastPropertyWasImportant = currentPropertyIsImportant;
    }
    return res;
}

String StylePropertySerializer::borderPropertyValue(CommonValueMode valueMode) const
{
    const StylePropertyShorthand properties[3] = { borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand() };
    String commonValue;
    StringBuilder result;
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
        String value = getCommonValue(properties[i]);
        if (value.isNull()) {
            if (valueMode == ReturnNullOnUncommonValues)
                return String();
            ASSERT(valueMode == OmitUncommonValues);
            continue;
        }
        if (!i)
            commonValue = value;
        else if (!commonValue.isNull() && commonValue != value)
            commonValue = String();
        if (value == "initial")
            continue;
        if (!result.isEmpty())
            result.append(' ');
        result.append(value);
    }
    if (isInitialOrInherit(commonValue))
        return commonValue;
    return result.isEmpty() ? String() : result.toString();
}

}
