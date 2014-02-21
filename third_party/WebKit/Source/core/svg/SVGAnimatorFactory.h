/*
 * Copyright (C) Research In Motion Limited 2011, 2012. All rights reserved.
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

#ifndef SVGAnimatorFactory_h
#define SVGAnimatorFactory_h

#include "core/svg/SVGAnimatedAngle.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNewPropertyAnimator.h"
#include "core/svg/SVGAnimatedPath.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGAnimatedRect.h"
#include "core/svg/SVGAnimatedString.h"
#include "core/svg/SVGAnimatedTransformList.h"

namespace WebCore {

class SVGAnimationElement;

class SVGAnimatorFactory {
public:
    static PassOwnPtr<SVGAnimatedTypeAnimator> create(SVGAnimationElement* animationElement, SVGElement* contextElement, AnimatedPropertyType attributeType)
    {
        ASSERT(animationElement);
        ASSERT(contextElement);

        switch (attributeType) {
        case AnimatedPath:
            return adoptPtr(new SVGAnimatedPathAnimator(animationElement, contextElement));
        // Below properties have migrated to new property implementation.
        case AnimatedAngle:
        case AnimatedEnumeration:
        case AnimatedBoolean:
        case AnimatedColor:
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
        case AnimatedTransformList:
            return adoptPtr(new SVGAnimatedNewPropertyAnimator(attributeType, animationElement, contextElement));

        // SVGAnimated{Point,StringList,Transform} does not exist.
        case AnimatedPoint:
        case AnimatedStringList:
        case AnimatedTransform:
            ASSERT_NOT_REACHED();

        case AnimatedUnknown:
            break;
        }

        ASSERT_NOT_REACHED();
        return nullptr;
    }

private:
    SVGAnimatorFactory() { }

};

} // namespace WebCore

#endif // SVGAnimatorFactory_h
