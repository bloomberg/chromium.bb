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

#include "core/svg/SVGAnimateTransformElement.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGColor.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGPointList.h"
#include "core/svg/SVGString.h"
#include "core/svg/SVGTransformList.h"

namespace WebCore {

SVGAnimatedNewPropertyAnimator::SVGAnimatedNewPropertyAnimator(AnimatedPropertyType type, SVGAnimationElement* animationElement, SVGElement* contextElement)
    : SVGAnimatedTypeAnimator(type, animationElement, contextElement)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    const QualifiedName& attributeName = m_animationElement->attributeName();
    m_animatedProperty = m_contextElement->propertyFromAttribute(attributeName);
    if (m_animatedProperty)
        ASSERT(m_animatedProperty->type() == m_type);
}

SVGAnimatedNewPropertyAnimator::~SVGAnimatedNewPropertyAnimator()
{
}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedNewPropertyAnimator::createPropertyForAnimation(const String& value)
{
    if (isAnimatingSVGDom()) {
        // SVG DOM animVal animation code-path.

        if (m_type == AnimatedTransformList) {
            // TransformList must be animated via <animateTransform>,
            // and its {from,by,to} attribute values needs to be parsed w.r.t. its "type" attribute.
            // Spec: http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
            ASSERT(m_animationElement);
            SVGTransformType transformType = toSVGAnimateTransformElement(m_animationElement)->transformType();
            return SVGTransformList::create(transformType, value);
        }

        ASSERT(m_animatedProperty);
        return m_animatedProperty->currentValueBase()->cloneForAnimation(value);
    }

    ASSERT(isAnimatingCSSProperty());

    // CSS properties animation code-path.
    // Create a basic instance of the corresponding SVG property.
    // The instance will not have full context info. (e.g. SVGLengthMode)

    switch (m_type) {
    case AnimatedColor:
        return SVGColorProperty::create(value.isEmpty() ? StyleColor::currentColor() : SVGColor::colorFromRGBColorString(value));
    case AnimatedNumber: {
        RefPtr<SVGNumber> property = SVGNumber::create();
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedLength: {
        RefPtr<SVGLength> property = SVGLength::create(LengthModeOther);
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedLengthList: {
        RefPtr<SVGLengthList> property = SVGLengthList::create(LengthModeOther);
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }
    case AnimatedString: {
        RefPtr<SVGString> property = SVGString::create();
        property->setValueAsString(value, IGNORE_EXCEPTION);
        return property.release();
    }

    // These types don't appear in the table in SVGElement::cssPropertyToTypeMap() and thus don't need support.
    case AnimatedBoolean:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPoint:
    case AnimatedPoints:
    case AnimatedRect:
    case AnimatedTransform:
    case AnimatedTransformList:
        ASSERT_NOT_REACHED();

    // These properties are not yet migrated to NewProperty implementation. see http://crbug.com/308818
    case AnimatedAngle:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedPath:
    case AnimatedPreserveAspectRatio:
    case AnimatedStringList:
        ASSERT_NOT_REACHED();

    case AnimatedUnknown:
        ASSERT_NOT_REACHED();
    };

    ASSERT_NOT_REACHED();
    return nullptr;
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNewPropertyAnimator::constructFromString(const String& value)
{
    return SVGAnimatedType::createNewProperty(createPropertyForAnimation(value));
}

namespace {

typedef void (NewSVGAnimatedPropertyBase::*NewSVGAnimatedPropertyMethod)();

void invokeMethodOnAllTargetProperties(const SVGElementAnimatedPropertyList& list, const QualifiedName& attributeName, NewSVGAnimatedPropertyMethod method)
{
    SVGElementAnimatedPropertyList::const_iterator it = list.begin();
    SVGElementAnimatedPropertyList::const_iterator itEnd = list.end();
    for (; it != itEnd; ++it) {
        RefPtr<NewSVGAnimatedPropertyBase> animatedProperty = it->element->propertyFromAttribute(attributeName);
        if (animatedProperty)
            (animatedProperty.get()->*method)();
    }
}

void setAnimatedValueOnAllTargetProperties(const SVGElementAnimatedPropertyList& list, const QualifiedName& attributeName, PassRefPtr<NewSVGPropertyBase> passValue)
{
    RefPtr<NewSVGPropertyBase> value = passValue;

    SVGElementAnimatedPropertyList::const_iterator it = list.begin();
    SVGElementAnimatedPropertyList::const_iterator itEnd = list.end();
    for (; it != itEnd; ++it) {
        RefPtr<NewSVGAnimatedPropertyBase> animatedProperty = it->element->propertyFromAttribute(attributeName);
        if (animatedProperty)
            animatedProperty->setAnimatedValue(value);
    }
}

}

PassRefPtr<NewSVGPropertyBase> SVGAnimatedNewPropertyAnimator::resetAnimation(const SVGElementAnimatedPropertyList& list)
{
    ASSERT(isAnimatingSVGDom());
    RefPtr<NewSVGPropertyBase> animatedValue = m_animatedProperty->createAnimatedValue();
    ASSERT(animatedValue->type() == m_type);
    setAnimatedValueOnAllTargetProperties(list, m_animatedProperty->attributeName(), animatedValue);

    return animatedValue.release();
}

PassOwnPtr<SVGAnimatedType> SVGAnimatedNewPropertyAnimator::startAnimValAnimation(const SVGElementAnimatedPropertyList& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    invokeMethodOnAllTargetProperties(list, m_animatedProperty->attributeName(), &NewSVGAnimatedPropertyBase::animationStarted);

    return SVGAnimatedType::createNewProperty(resetAnimation(list));
}

void SVGAnimatedNewPropertyAnimator::stopAnimValAnimation(const SVGElementAnimatedPropertyList& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    invokeMethodOnAllTargetProperties(list, m_animatedProperty->attributeName(), &NewSVGAnimatedPropertyBase::animationEnded);
}

void SVGAnimatedNewPropertyAnimator::resetAnimValToBaseVal(const SVGElementAnimatedPropertyList& list, SVGAnimatedType* animated)
{
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    animated->newProperty() = resetAnimation(list);
}

void SVGAnimatedNewPropertyAnimator::animValWillChange(const SVGElementAnimatedPropertyList& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    invokeMethodOnAllTargetProperties(list, m_animatedProperty->attributeName(), &NewSVGAnimatedPropertyBase::animValWillChange);
}

void SVGAnimatedNewPropertyAnimator::animValDidChange(const SVGElementAnimatedPropertyList& list)
{
    ASSERT(isAnimatingSVGDom());
    SVGElementInstance::InstanceUpdateBlocker blocker(m_contextElement);

    invokeMethodOnAllTargetProperties(list, m_animatedProperty->attributeName(), &NewSVGAnimatedPropertyBase::animValDidChange);
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
