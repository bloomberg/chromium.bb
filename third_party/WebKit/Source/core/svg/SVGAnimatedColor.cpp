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

#include "core/svg/SVGAnimatedColor.h"

#include "core/rendering/RenderObject.h"
#include "core/svg/ColorDistance.h"
#include "core/svg/SVGAnimateElement.h"
#include "core/svg/SVGColor.h"

namespace WebCore {

SVGAnimatedColorAnimator::SVGAnimatedColorAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedColor, animationElement, contextElement)
{
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedColorAnimator::constructFromString(const String& string)
{
    StyleColor styleColor = string.isEmpty() ? StyleColor::currentColor() : SVGColor::colorFromRGBColorString(string);
    OwnPtr<SVGAnimatedType> animtedType = SVGAnimatedType::createColor(new StyleColor(styleColor));
    return animtedType.release();
}

static inline Color fallbackColorForCurrentColor(SVGElement* targetElement)
{
    ASSERT(targetElement);
    if (RenderObject* targetRenderer = targetElement->renderer())
        return targetRenderer->style()->visitedDependentColor(CSSPropertyColor);
    else
        return Color::transparent;
}

void SVGAnimatedColorAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    ASSERT(from->type() == AnimatedColor);
    ASSERT(from->type() == to->type());
    Color fallbackColor = fallbackColorForCurrentColor(m_contextElement);
    Color fromColor = from->color().resolve(fallbackColor);
    Color toColor = to->color().resolve(fallbackColor);
    to->color() = ColorDistance::addColors(fromColor, toColor);
}

static StyleColor parseColorFromString(SVGAnimationElement*, const String& string)
{
    return SVGColor::colorFromRGBColorString(string);
}

void SVGAnimatedColorAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    StyleColor fromStyleColor = m_animationElement->animationMode() == ToAnimation ? animated->color() : from->color();
    StyleColor toStyleColor = to->color();
    StyleColor toAtEndOfDurationStyleColor = toAtEndOfDuration->color();
    StyleColor& animatedStyleColor = animated->color();

    // Apply CSS inheritance rules.
    m_animationElement->adjustForInheritance<StyleColor>(parseColorFromString, m_animationElement->fromPropertyValueType(), fromStyleColor, m_contextElement);
    m_animationElement->adjustForInheritance<StyleColor>(parseColorFromString, m_animationElement->toPropertyValueType(), toStyleColor, m_contextElement);

    // Apply currentColor rules.
    Color fallbackColor = fallbackColorForCurrentColor(m_contextElement);
    Color fromColor = fromStyleColor.resolve(fallbackColor);
    Color toColor = toStyleColor.resolve(fallbackColor);
    Color toAtEndOfDurationColor = toAtEndOfDurationStyleColor.resolve(fallbackColor);
    Color animatedColor = animatedStyleColor.resolve(fallbackColor);

    float animatedRed = animatedColor.red();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.red(), toColor.red(), toAtEndOfDurationColor.red(), animatedRed);

    float animatedGreen = animatedColor.green();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.green(), toColor.green(), toAtEndOfDurationColor.green(), animatedGreen);

    float animatedBlue = animatedColor.blue();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.blue(), toColor.blue(), toAtEndOfDurationColor.blue(), animatedBlue);

    float animatedAlpha = animatedColor.alpha();
    m_animationElement->animateAdditiveNumber(percentage, repeatCount, fromColor.alpha(), toColor.alpha(), toAtEndOfDurationColor.alpha(), animatedAlpha);

    animatedStyleColor = StyleColor(makeRGBA(roundf(animatedRed), roundf(animatedGreen), roundf(animatedBlue), roundf(animatedAlpha)));
}

float SVGAnimatedColorAnimator::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: currentColor should be resolved
    ASSERT(m_contextElement);
    StyleColor from = SVGColor::colorFromRGBColorString(fromString);
    if (from.isCurrentColor())
        return -1;
    StyleColor to = SVGColor::colorFromRGBColorString(toString);
    if (to.isCurrentColor())
        return -1;
    return ColorDistance::distance(from.color(), to.color());
}

}
