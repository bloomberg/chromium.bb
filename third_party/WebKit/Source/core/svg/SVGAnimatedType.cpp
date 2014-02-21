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

#include "config.h"
#include "core/svg/SVGAnimatedType.h"

#include "bindings/v8/ExceptionState.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGPathByteStream.h"

namespace WebCore {

SVGAnimatedType::SVGAnimatedType(AnimatedPropertyType type)
    : m_type(type)
{
}

SVGAnimatedType::~SVGAnimatedType()
{
    switch (m_type) {
    case AnimatedPath:
        delete m_data.path;
        break;
    // Below properties are migrated to new property implementation.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedColor:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedNumber:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedLength:
    case AnimatedLengthList:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedRect:
    case AnimatedString:
    case AnimatedStringList:
    case AnimatedTransformList:
        // handled by RefPtr
        break;

    // There is no SVGAnimated{Point,Transform}
    case AnimatedPoint:
    case AnimatedTransform:
        ASSERT_NOT_REACHED();
        break;

    case AnimatedUnknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createPath(PassOwnPtr<SVGPathByteStream> path)
{
    ASSERT(path);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(AnimatedPath));
    animatedType->m_data.path = path.leakPtr();
    return animatedType.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedType::createNewProperty(PassRefPtr<NewSVGPropertyBase> newProperty)
{
    ASSERT(newProperty);
    OwnPtr<SVGAnimatedType> animatedType = adoptPtr(new SVGAnimatedType(newProperty->type()));
    animatedType->m_newProperty = newProperty;
    return animatedType.release();
}

String SVGAnimatedType::valueAsString()
{
    switch (m_type) {
    // Below properties have migrated to new property implementation.
    case AnimatedColor:
    case AnimatedNumber:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedLength:
    case AnimatedLengthList:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedRect:
    case AnimatedString:
    case AnimatedStringList:
        return m_newProperty->valueAsString();

    // These types don't appear in the table in SVGElement::cssPropertyToTypeMap() and thus don't need valueAsString() support.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedPath:
    case AnimatedPoint:
    case AnimatedTransform:
    case AnimatedTransformList:
    case AnimatedUnknown:
        // Only SVG DOM animations use these property types - that means valueAsString() is never used for those.
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return String();
}

bool SVGAnimatedType::setValueAsString(const QualifiedName& attrName, const String& value)
{
    switch (m_type) {
    // Below properties have migrated to new property implementation.
    case AnimatedColor:
    case AnimatedNumber:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedLength:
    case AnimatedLengthList:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedRect:
    case AnimatedString:
    case AnimatedStringList:
        // Always use createForAnimation call path for these implementations.
        return false;

    // These types don't appear in the table in SVGElement::cssPropertyToTypeMap() and thus don't need setValueAsString() support.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedPath:
    case AnimatedPoint:
    case AnimatedTransform:
    case AnimatedTransformList:
    case AnimatedUnknown:
        // Only SVG DOM animations use these property types - that means setValueAsString() is never used for those.
        ASSERT_NOT_REACHED();
        break;
    }
    return true;
}

bool SVGAnimatedType::supportsAnimVal(AnimatedPropertyType type)
{
    // AnimatedColor is only used for CSS property animations.
    return type != AnimatedUnknown && type != AnimatedColor;
}

} // namespace WebCore
