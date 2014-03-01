/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "core/css/parser/CSSPropertyParser.h"
#include "RuntimeEnabledFeatures.h"
#include "core/rendering/RenderTheme.h"
#include "core/svg/SVGPaint.h"

using namespace std;

namespace WebCore {

CSSPropertyParser::CSSPropertyParser(OwnPtr<CSSParserValueList>& valueList,
    const CSSParserContext& context, bool inViewport, bool savedImportant,
    ParsedPropertyVector& parsedProperties, bool& hasFontFaceOnlyValues)
    : m_valueList(valueList)
    , m_context(context)
    , m_inViewport(inViewport)
    , m_important(savedImportant) // See comment in header, should be removed.
    , m_parsedProperties(parsedProperties)
    , m_hasFontFaceOnlyValues(hasFontFaceOnlyValues)
    , m_inParseShorthand(0)
    , m_currentShorthand(CSSPropertyInvalid)
    , m_implicitShorthand(false)
{
}

CSSPropertyParser::~CSSPropertyParser()
{
}

bool CSSPropertyParser::isSystemColor(int id)
{
    return (id >= CSSValueActiveborder && id <= CSSValueWindowtext) || id == CSSValueMenu;
}

bool CSSPropertyParser::parseSVGValue(CSSPropertyID propId, bool important)
{
    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    CSSValueID id = value->id;

    bool validPrimitive = false;
    RefPtrWillBeRawPtr<CSSValue> parsedValue;

    switch (propId) {
    /* The comment to the right defines all valid value of these
     * properties as defined in SVG 1.1, Appendix N. Property index */
    case CSSPropertyAlignmentBaseline:
    // auto | baseline | before-edge | text-before-edge | middle |
    // central | after-edge | text-after-edge | ideographic | alphabetic |
    // hanging | mathematical | inherit
        if (id == CSSValueAuto || id == CSSValueBaseline || id == CSSValueMiddle
            || (id >= CSSValueBeforeEdge && id <= CSSValueMathematical))
            validPrimitive = true;
        break;

    case CSSPropertyBaselineShift:
    // baseline | super | sub | <percentage> | <length> | inherit
        if (id == CSSValueBaseline || id == CSSValueSub
            || id >= CSSValueSuper)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength | FPercent, SVGAttributeMode);
        break;

    case CSSPropertyDominantBaseline:
    // auto | use-script | no-change | reset-size | ideographic |
    // alphabetic | hanging | mathematical | central | middle |
    // text-after-edge | text-before-edge | inherit
        if (id == CSSValueAuto || id == CSSValueMiddle
            || (id >= CSSValueUseScript && id <= CSSValueResetSize)
            || (id >= CSSValueCentral && id <= CSSValueMathematical))
            validPrimitive = true;
        break;

    case CSSPropertyEnableBackground:
    // accumulate | new [x] [y] [width] [height] | inherit
        if (id == CSSValueAccumulate) // TODO : new
            validPrimitive = true;
        break;

    case CSSPropertyMarkerStart:
    case CSSPropertyMarkerMid:
    case CSSPropertyMarkerEnd:
    case CSSPropertyMask:
        if (id == CSSValueNone) {
            validPrimitive = true;
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSPrimitiveValue::create(value->string, CSSPrimitiveValue::CSS_URI);
            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyClipRule: // nonzero | evenodd | inherit
    case CSSPropertyFillRule:
        if (id == CSSValueNonzero || id == CSSValueEvenodd)
            validPrimitive = true;
        break;

    case CSSPropertyStrokeMiterlimit: // <miterlimit> | inherit
        validPrimitive = validUnit(value, FNumber | FNonNeg, SVGAttributeMode);
        break;

    case CSSPropertyStrokeLinejoin: // miter | round | bevel | inherit
        if (id == CSSValueMiter || id == CSSValueRound || id == CSSValueBevel)
            validPrimitive = true;
        break;

    case CSSPropertyStrokeLinecap: // butt | round | square | inherit
        if (id == CSSValueButt || id == CSSValueRound || id == CSSValueSquare)
            validPrimitive = true;
        break;

    case CSSPropertyStrokeOpacity: // <opacity-value> | inherit
    case CSSPropertyFillOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyFloodOpacity:
        validPrimitive = (!id && validUnit(value, FNumber | FPercent, SVGAttributeMode));
        break;

    case CSSPropertyShapeRendering:
    // auto | optimizeSpeed | crispEdges | geometricPrecision | inherit
        if (id == CSSValueAuto || id == CSSValueOptimizespeed
            || id == CSSValueCrispedges || id == CSSValueGeometricprecision)
            validPrimitive = true;
        break;

    case CSSPropertyImageRendering: // auto | optimizeSpeed |
    case CSSPropertyColorRendering: // optimizeQuality | inherit
        if (id == CSSValueAuto || id == CSSValueOptimizespeed
            || id == CSSValueOptimizequality)
            validPrimitive = true;
        break;

    case CSSPropertyBufferedRendering: // auto | dynamic | static
        if (id == CSSValueAuto || id == CSSValueDynamic || id == CSSValueStatic)
            validPrimitive = true;
        break;

    case CSSPropertyColorProfile: // auto | sRGB | <name> | <uri> inherit
        if (id == CSSValueAuto || id == CSSValueSrgb)
            validPrimitive = true;
        break;

    case CSSPropertyColorInterpolation: // auto | sRGB | linearRGB | inherit
    case CSSPropertyColorInterpolationFilters:
        if (id == CSSValueAuto || id == CSSValueSrgb || id == CSSValueLinearrgb)
            validPrimitive = true;
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in applyRule(..)
     */

    case CSSPropertyTextAnchor: // start | middle | end | inherit
        if (id == CSSValueStart || id == CSSValueMiddle || id == CSSValueEnd)
            validPrimitive = true;
        break;

    case CSSPropertyGlyphOrientationVertical: // auto | <angle> | inherit
        if (id == CSSValueAuto) {
            validPrimitive = true;
            break;
        }
    /* fallthrough intentional */
    case CSSPropertyGlyphOrientationHorizontal: // <angle> (restricted to _deg_ per SVG 1.1 spec) | inherit
        if (value->unit == CSSPrimitiveValue::CSS_DEG || value->unit == CSSPrimitiveValue::CSS_NUMBER) {
            parsedValue = CSSPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_DEG);

            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyFill: // <paint> | inherit
    case CSSPropertyStroke: // <paint> | inherit
        {
            if (id == CSSValueNone) {
                parsedValue = SVGPaint::createNone();
            } else if (id == CSSValueCurrentcolor) {
                parsedValue = SVGPaint::createCurrentColor();
            } else if (isSystemColor(id)) {
                parsedValue = SVGPaint::createColor(RenderTheme::theme().systemColor(id));
            } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
                RGBA32 c = Color::transparent;
                if (m_valueList->next()) {
                    if (parseColorFromValue(m_valueList->current(), c))
                        parsedValue = SVGPaint::createURIAndColor(value->string, c);
                    else if (m_valueList->current()->id == CSSValueNone)
                        parsedValue = SVGPaint::createURIAndNone(value->string);
                    else if (m_valueList->current()->id == CSSValueCurrentcolor)
                        parsedValue = SVGPaint::createURIAndCurrentColor(value->string);
                }
                if (!parsedValue)
                    parsedValue = SVGPaint::createURI(value->string);
            } else {
                parsedValue = parseSVGPaint();
            }

            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyStopColor: // TODO : icccolor
    case CSSPropertyFloodColor:
    case CSSPropertyLightingColor:
        if (isSystemColor(id))
            parsedValue = SVGColor::createFromColor(RenderTheme::theme().systemColor(id));
        else if ((id >= CSSValueAqua && id <= CSSValueTransparent)
            || (id >= CSSValueAliceblue && id <= CSSValueYellowgreen) || id == CSSValueGrey)
            parsedValue = SVGColor::createFromString(value->string);
        else if (id == CSSValueCurrentcolor)
            parsedValue = SVGColor::createCurrentColor();
        else // TODO : svgcolor (iccColor)
            parsedValue = parseSVGColor();

        if (parsedValue)
            m_valueList->next();

        break;

    case CSSPropertyPaintOrder:
        if (!RuntimeEnabledFeatures::svgPaintOrderEnabled())
            return false;

        if (m_valueList->size() == 1 && id == CSSValueNormal)
            validPrimitive = true;
        else if ((parsedValue = parsePaintOrder()))
            m_valueList->next();
        break;

    case CSSPropertyVectorEffect: // none | non-scaling-stroke | inherit
        if (id == CSSValueNone || id == CSSValueNonScalingStroke)
            validPrimitive = true;
        break;

    case CSSPropertyWritingMode:
    // lr-tb | rl_tb | tb-rl | lr | rl | tb | inherit
        if (id == CSSValueLrTb || id == CSSValueRlTb || id == CSSValueTbRl || id == CSSValueLr || id == CSSValueRl || id == CSSValueTb)
            validPrimitive = true;
        break;

    case CSSPropertyStrokeWidth: // <length> | inherit
    case CSSPropertyStrokeDashoffset:
        validPrimitive = validUnit(value, FLength | FPercent, SVGAttributeMode);
        break;
    case CSSPropertyStrokeDasharray: // none | <dasharray> | inherit
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            parsedValue = parseSVGStrokeDasharray();

        break;

    case CSSPropertyKerning: // auto | normal | <length> | inherit
        if (id == CSSValueAuto || id == CSSValueNormal)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength, SVGAttributeMode);
        break;

    case CSSPropertyClipPath: // <uri> | none | inherit
    case CSSPropertyFilter:
        if (id == CSSValueNone) {
            validPrimitive = true;
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSPrimitiveValue::create(value->string, (CSSPrimitiveValue::UnitTypes) value->unit);
            if (parsedValue)
                m_valueList->next();
        }
        break;
    case CSSPropertyMaskType: // luminance | alpha | inherit
        if (id == CSSValueLuminance || id == CSSValueAlpha)
            validPrimitive = true;
        break;

    /* shorthand properties */
    case CSSPropertyMarker: {
        ShorthandScope scope(this, propId);
        CSSPropertyParser::ImplicitScope implicitScope(this, PropertyImplicit);
        if (!parseValue(CSSPropertyMarkerStart, important))
            return false;
        if (m_valueList->current()) {
            rollbackLastProperties(1);
            return false;
        }
        CSSValue* value = m_parsedProperties.last().value();
        addProperty(CSSPropertyMarkerMid, value, important);
        addProperty(CSSPropertyMarkerEnd, value, important);
        return true;
    }
    default:
        // If you crash here, it's because you added a css property and are not handling it
        // in either this switch statement or the one in CSSPropertyParser::parseValue
        ASSERT_WITH_MESSAGE(0, "unimplemented propertyID: %d", propId);
        return false;
    }

    if (validPrimitive) {
        if (id)
            parsedValue = CSSPrimitiveValue::createIdentifier(id);
        else if (value->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = CSSPrimitiveValue::create(value->string, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSParserValue::Q_EMS)
            parsedValue = CSSPrimitiveValue::createAllowingMarginQuirk(value->fValue, CSSPrimitiveValue::CSS_EMS);
        if (isCalculation(value)) {
            // FIXME calc() http://webkit.org/b/16662 : actually create a CSSPrimitiveValue here, ie
            // parsedValue = CSSPrimitiveValue::create(m_parsedCalculation.release());
            m_parsedCalculation.release();
            parsedValue = nullptr;
        }
        m_valueList->next();
    }
    if (!parsedValue || (m_valueList->current() && !inShorthand()))
        return false;

    addProperty(propId, parsedValue.release(), important);
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseSVGStrokeDasharray()
{
    RefPtrWillBeRawPtr<CSSValueList> ret = CSSValueList::createCommaSeparated();
    CSSParserValue* value = m_valueList->current();
    bool validPrimitive = true;
    while (value) {
        validPrimitive = validUnit(value, FLength | FPercent | FNonNeg, SVGAttributeMode);
        if (!validPrimitive)
            break;
        if (value->id)
            ret->append(CSSPrimitiveValue::createIdentifier(value->id));
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            ret->append(CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit));
        value = m_valueList->next();
        if (value && value->unit == CSSParserValue::Operator && value->iValue == ',')
            value = m_valueList->next();
    }
    if (!validPrimitive)
        return nullptr;
    return ret.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseSVGPaint()
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(m_valueList->current(), c))
        return SVGPaint::createUnknown();
    return SVGPaint::createColor(Color(c));
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseSVGColor()
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(m_valueList->current(), c))
        return nullptr;
    return SVGColor::createFromColor(Color(c));
}

// normal | [ fill || stroke || markers ]
PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parsePaintOrder() const
{
    if (m_valueList->size() > 3)
        return nullptr;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();

    // The default paint-order is: Fill, Stroke, Markers.
    bool seenFill = false, seenStroke = false, seenMarkers = false;

    do {
        switch (value->id) {
        case CSSValueNormal:
            // normal inside [fill || stroke || markers] not valid
            return nullptr;
        case CSSValueFill:
            if (seenFill)
                return nullptr;

            seenFill = true;
            break;
        case CSSValueStroke:
            if (seenStroke)
                return nullptr;

            seenStroke = true;
            break;
        case CSSValueMarkers:
            if (seenMarkers)
                return nullptr;

            seenMarkers = true;
            break;
        default:
            return nullptr;
        }

        parsedValues->append(CSSPrimitiveValue::createIdentifier(value->id));
    } while ((value = m_valueList->next()));

    // fill out the rest of the paint order
    if (!seenFill)
        parsedValues->append(CSSPrimitiveValue::createIdentifier(CSSValueFill));
    if (!seenStroke)
        parsedValues->append(CSSPrimitiveValue::createIdentifier(CSSValueStroke));
    if (!seenMarkers)
        parsedValues->append(CSSPrimitiveValue::createIdentifier(CSSValueMarkers));

    return parsedValues.release();
}

} // namespace WebCore
