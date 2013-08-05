/*
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

#ifndef SVGLengthContext_h
#define SVGLengthContext_h

#include "core/platform/graphics/FloatRect.h"
#include "core/svg/SVGUnitTypes.h"

namespace WebCore {

class ExceptionState;
class SVGElement;
class SVGLength;

enum SVGLengthType {
    LengthTypeUnknown = 0,
    LengthTypeNumber,
    LengthTypePercentage,
    LengthTypeEMS,
    LengthTypeEXS,
    LengthTypePX,
    LengthTypeCM,
    LengthTypeMM,
    LengthTypeIN,
    LengthTypePT,
    LengthTypePC
};

enum SVGLengthMode {
    LengthModeWidth = 0,
    LengthModeHeight,
    LengthModeOther
};

class SVGLengthContext {
public:
    explicit SVGLengthContext(const SVGElement*);

    template<typename T>
    static FloatRect resolveRectangle(const T* context, SVGUnitTypes::SVGUnitType type, const FloatRect& viewport)
    {
        return SVGLengthContext::resolveRectangle(context, type, viewport, context->xCurrentValue(), context->yCurrentValue(), context->widthCurrentValue(), context->heightCurrentValue());
    }

    static FloatRect resolveRectangle(const SVGElement*, SVGUnitTypes::SVGUnitType, const FloatRect& viewport, const SVGLength& x, const SVGLength& y, const SVGLength& width, const SVGLength& height);
    static FloatPoint resolvePoint(const SVGElement*, SVGUnitTypes::SVGUnitType, const SVGLength& x, const SVGLength& y);
    static float resolveLength(const SVGElement*, SVGUnitTypes::SVGUnitType, const SVGLength&);

    float convertValueToUserUnits(float, SVGLengthMode, SVGLengthType fromUnit, ExceptionState&) const;
    float convertValueFromUserUnits(float, SVGLengthMode, SVGLengthType toUnit, ExceptionState&) const;

    bool determineViewport(float& width, float& height) const;

private:
    SVGLengthContext(const SVGElement*, const FloatRect& viewport);

    float convertValueFromUserUnitsToPercentage(float value, SVGLengthMode, ExceptionState&) const;
    float convertValueFromPercentageToUserUnits(float value, SVGLengthMode, ExceptionState&) const;

    float convertValueFromUserUnitsToEMS(float value, ExceptionState&) const;
    float convertValueFromEMSToUserUnits(float value, ExceptionState&) const;

    float convertValueFromUserUnitsToEXS(float value, ExceptionState&) const;
    float convertValueFromEXSToUserUnits(float value, ExceptionState&) const;

    const SVGElement* m_context;
    FloatRect m_overridenViewport;
};

} // namespace WebCore

#endif // SVGLengthContext_h
