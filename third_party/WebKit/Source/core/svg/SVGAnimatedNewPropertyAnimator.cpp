/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/svg/SVGAnimatedNewPropertyAnimator.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGElementInstance.h"

namespace WebCore {

SVGAnimatedNewPropertyAnimator::SVGAnimatedNewPropertyAnimator(SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(AnimatedNewProperty, animationElement, contextElement)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    const QualifiedName& attributeName = m_animationElement->attributeName();
    m_animatedProperty = m_contextElement->propertyFromAttribute(attributeName);
    ASSERT(m_animatedProperty);
}

SVGAnimatedNewPropertyAnimator::~SVGAnimatedNewPropertyAnimator()
{
}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedNewPropertyAnimator::createPropertyForAnimation(const String& value)
{
    return m_animatedProperty->currentValueBase()->cloneForAnimation(value);
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNewPropertyAnimator::constructFromString(const String& value)
{
    return SVGAnimatedType::createNewProperty(createPropertyForAnimation(value));
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNewPropertyAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList&)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    m_animatedProperty->animationStarted();
    return SVGAnimatedType::createNewProperty(m_animatedProperty->currentValueBase());
}

void SVGAnimatedNewPropertyAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList&)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    m_animatedProperty->animationEnded();
}

void SVGAnimatedNewPropertyAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList&, SVGAnimatedType* animated)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    m_animatedProperty->resetToBaseVal();
    animated->newProperty() = m_animatedProperty->currentValueBase();
}

void SVGAnimatedNewPropertyAnimator::animValWillChange(const SVGElementAnimatedPropertyList&)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    m_animatedProperty->animValWillChange();
}

void SVGAnimatedNewPropertyAnimator::animValDidChange(const SVGElementAnimatedPropertyList&)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    m_animatedProperty->animValDidChange();
}

void SVGAnimatedNewPropertyAnimator::addAnimatedTypes(SVGAnimatedType* from, SVGAnimatedType* to)
{
    to->newProperty()->add(from->newProperty(), m_contextElement);
}

class ParsePropertyFromString {
public:
    explicit ParsePropertyFromString(SVGAnimatedNewPropertyAnimator* animator)
        : m_animator(animator)
    {
    }

    PassRefPtr<NewSVGPropertyBase> operator()(SVGAnimationElement*, const String& value)
    {
        return m_animator->createPropertyForAnimation(value);
    }

private:
    SVGAnimatedNewPropertyAnimator* m_animator;
};

void SVGAnimatedNewPropertyAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGAnimatedType* from, SVGAnimatedType* to, SVGAnimatedType* toAtEndOfDuration, SVGAnimatedType* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    RefPtr<NewSVGPropertyBase> fromValue = m_animationElement->animationMode() == ToAnimation ? animated->newProperty() : from->newProperty();
    RefPtr<NewSVGPropertyBase> toValue = to->newProperty();
    RefPtr<NewSVGPropertyBase> toAtEndOfDurationValue = toAtEndOfDuration->newProperty();
    RefPtr<NewSVGPropertyBase> animatedValue = animated->newProperty();

    // Apply CSS inheritance rules.
    ParsePropertyFromString parsePropertyFromString(this);
    m_animationElement->adjustForInheritance<RefPtr<NewSVGPropertyBase>, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->fromPropertyValueType(), fromValue, m_contextElement);
    m_animationElement->adjustForInheritance<RefPtr<NewSVGPropertyBase>, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->toPropertyValueType(), toValue, m_contextElement);

    animatedValue->calculateAnimatedValue(m_animationElement, percentage, repeatCount, fromValue, toValue, toAtEndOfDurationValue, m_contextElement);
}

float SVGAnimatedNewPropertyAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);
    RefPtr<NewSVGPropertyBase> fromValue = createPropertyForAnimation(fromString);
    RefPtr<NewSVGPropertyBase> toValue = createPropertyForAnimation(toString);
    return fromValue->calculateDistance(toValue, m_contextElement);
}

} // namespace WebCore
