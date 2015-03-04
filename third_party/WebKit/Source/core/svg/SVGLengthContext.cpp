/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "core/svg/SVGLengthContext.h"

#include "core/css/CSSHelper.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/style/LayoutStyle.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/LengthFunctions.h"
#include "platform/fonts/FontMetrics.h"

namespace blink {

static inline float dimensionForLengthMode(SVGLengthMode mode, const FloatSize& viewportSize)
{
    switch (mode) {
    case SVGLengthMode::Width:
        return viewportSize.width();
    case SVGLengthMode::Height:
        return viewportSize.height();
    case SVGLengthMode::Other:
        return sqrtf(viewportSize.diagonalLengthSquared() / 2);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static float convertValueFromPercentageToUserUnits(const SVGLength& value, const FloatSize& viewportSize)
{
    return value.scaleByPercentage(dimensionForLengthMode(value.unitMode(), viewportSize));
}

SVGLengthContext::SVGLengthContext(const SVGElement* context)
    : m_context(context)
{
}

FloatRect SVGLengthContext::resolveRectangle(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const FloatRect& viewport, const SVGLength& x, const SVGLength& y, const SVGLength& width, const SVGLength& height)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type != SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE && !viewport.isEmpty()) {
        const FloatSize& viewportSize = viewport.size();
        return FloatRect(
            convertValueFromPercentageToUserUnits(x, viewportSize) + viewport.x(),
            convertValueFromPercentageToUserUnits(y, viewportSize) + viewport.y(),
            convertValueFromPercentageToUserUnits(width, viewportSize),
            convertValueFromPercentageToUserUnits(height, viewportSize));
    }

    SVGLengthContext lengthContext(context);
    return FloatRect(x.value(lengthContext), y.value(lengthContext), width.value(lengthContext), height.value(lengthContext));
}

FloatPoint SVGLengthContext::resolvePoint(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const SVGLength& x, const SVGLength& y)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        SVGLengthContext lengthContext(context);
        return FloatPoint(x.value(lengthContext), y.value(lengthContext));
    }

    // FIXME: valueAsPercentage() won't be correct for eg. cm units. They need to be resolved in user space and then be considered in objectBoundingBox space.
    return FloatPoint(x.valueAsPercentage(), y.valueAsPercentage());
}

float SVGLengthContext::resolveLength(const SVGElement* context, SVGUnitTypes::SVGUnitType type, const SVGLength& x)
{
    ASSERT(type != SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN);
    if (type == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        SVGLengthContext lengthContext(context);
        return x.value(lengthContext);
    }

    // FIXME: valueAsPercentage() won't be correct for eg. cm units. They need to be resolved in user space and then be considered in objectBoundingBox space.
    return x.valueAsPercentage();
}

float SVGLengthContext::valueForLength(const Length& length, const LayoutStyle& style, SVGLengthMode mode) const
{
    float dimension = 0;
    if (length.isPercent()) {
        FloatSize viewportSize;
        determineViewport(viewportSize);
        // The viewport will be unaffected by zoom.
        dimension = dimensionForLengthMode(mode, viewportSize);
    }
    return valueForLength(length, style, dimension);
}

float SVGLengthContext::valueForLength(const Length& length, const LayoutStyle& style, float dimension)
{
    const float zoom = style.effectiveZoom();
    ASSERT(zoom != 0);
    return floatValueForLength(length, dimension * zoom) / zoom;
}

float SVGLengthContext::convertValueToUserUnits(float value, SVGLengthMode mode, SVGLengthType fromUnit) const
{
    switch (fromUnit) {
    case LengthTypeUnknown:
        return 0;
    case LengthTypeNumber:
        return value;
    case LengthTypePX:
        return value;
    case LengthTypePercentage: {
        FloatSize viewportSize;
        if (!determineViewport(viewportSize))
            return 0;
        return value * dimensionForLengthMode(mode, viewportSize) / 100;
    }
    case LengthTypeEMS:
        return convertValueFromEMSToUserUnits(value);
    case LengthTypeEXS:
        return convertValueFromEXSToUserUnits(value);
    case LengthTypeCM:
        return value * cssPixelsPerCentimeter;
    case LengthTypeMM:
        return value * cssPixelsPerMillimeter;
    case LengthTypeIN:
        return value * cssPixelsPerInch;
    case LengthTypePT:
        return value * cssPixelsPerPoint;
    case LengthTypePC:
        return value * cssPixelsPerPica;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

float SVGLengthContext::convertValueFromUserUnits(float value, SVGLengthMode mode, SVGLengthType toUnit) const
{
    switch (toUnit) {
    case LengthTypeUnknown:
        return 0;
    case LengthTypeNumber:
        return value;
    case LengthTypePercentage: {
        FloatSize viewportSize;
        if (!determineViewport(viewportSize))
            return 0;
        // LengthTypePercentage is represented with 100% = 100.0.
        // Good for accuracy but could eventually be changed.
        return value * 100 / dimensionForLengthMode(mode, viewportSize);
    }
    case LengthTypeEMS:
        return convertValueFromUserUnitsToEMS(value);
    case LengthTypeEXS:
        return convertValueFromUserUnitsToEXS(value);
    case LengthTypePX:
        return value;
    case LengthTypeCM:
        return value / cssPixelsPerCentimeter;
    case LengthTypeMM:
        return value / cssPixelsPerMillimeter;
    case LengthTypeIN:
        return value / cssPixelsPerInch;
    case LengthTypePT:
        return value / cssPixelsPerPoint;
    case LengthTypePC:
        return value / cssPixelsPerPica;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static inline LayoutStyle* layoutStyleForLengthResolving(const SVGElement* context)
{
    if (!context)
        return 0;

    const ContainerNode* currentContext = context;
    do {
        if (currentContext->renderer())
            return currentContext->renderer()->style();
        currentContext = currentContext->parentNode();
    } while (currentContext);

    // There must be at least a LayoutSVGRoot renderer, carrying a style.
    ASSERT_NOT_REACHED();
    return 0;
}

float SVGLengthContext::convertValueFromUserUnitsToEMS(float value) const
{
    LayoutStyle* style = layoutStyleForLengthResolving(m_context);
    if (!style)
        return 0;

    float fontSize = style->specifiedFontSize();
    if (!fontSize)
        return 0;

    return value / fontSize;
}

float SVGLengthContext::convertValueFromEMSToUserUnits(float value) const
{
    LayoutStyle* style = layoutStyleForLengthResolving(m_context);
    if (!style)
        return 0;
    return value * style->specifiedFontSize();
}

float SVGLengthContext::convertValueFromUserUnitsToEXS(float value) const
{
    LayoutStyle* style = layoutStyleForLengthResolving(m_context);
    if (!style)
        return 0;

    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
    // if this causes problems in real world cases maybe it would be best to remove this
    float xHeight = ceilf(style->fontMetrics().xHeight());
    if (!xHeight)
        return 0;

    return value / xHeight;
}

float SVGLengthContext::convertValueFromEXSToUserUnits(float value) const
{
    LayoutStyle* style = layoutStyleForLengthResolving(m_context);
    if (!style)
        return 0;

    // Use of ceil allows a pixel match to the W3Cs expected output of coords-units-03-b.svg
    // if this causes problems in real world cases maybe it would be best to remove this
    return value * ceilf(style->fontMetrics().xHeight());
}

bool SVGLengthContext::determineViewport(FloatSize& viewportSize) const
{
    if (!m_context)
        return false;

    // Root <svg> element lengths are resolved against the top level viewport.
    if (m_context->isOutermostSVGSVGElement()) {
        viewportSize = toSVGSVGElement(m_context)->currentViewportSize();
        return true;
    }

    // Take size from nearest viewport element.
    SVGElement* viewportElement = m_context->viewportElement();
    if (!isSVGSVGElement(viewportElement))
        return false;

    const SVGSVGElement& svg = toSVGSVGElement(*viewportElement);
    viewportSize = svg.currentViewBoxRect().size();
    if (viewportSize.isEmpty())
        viewportSize = svg.currentViewportSize();

    return true;
}

}
