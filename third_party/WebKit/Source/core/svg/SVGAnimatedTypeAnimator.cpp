/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "core/svg/SVGAnimatedTypeAnimator.h"

#include "core/svg/SVGElement.h"

namespace WebCore {

SVGAnimatedTypeAnimator::SVGAnimatedTypeAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
    : m_type(type)
    , m_animationElement(animationElement)
    , m_contextElement(contextElement)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
}

SVGAnimatedTypeAnimator::~SVGAnimatedTypeAnimator()
{
}

void SVGAnimatedTypeAnimator::calculateFromAndToValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedTypeAnimator::calculateFromAndByValues(RefPtr<NewSVGPropertyBase>& from, RefPtr<NewSVGPropertyBase>& to, const String& fromString, const String& byString)
{
    from = constructFromString(fromString);
    to = constructFromString(byString);
    to->add(from, m_contextElement);
}

}
