/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "core/svg/SVGAnimateElement.h"

#include "CSSPropertyNames.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/QualifiedName.h"
#include "core/svg/SVGAnimatedTypeAnimator.h"
#include "core/svg/SVGAnimatorFactory.h"
#include "core/svg/SVGDocumentExtensions.h"

namespace WebCore {

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName, Document& document)
    : SVGAnimationElement(tagName, document)
    , m_animatedPropertyType(AnimatedString)
{
    ASSERT(hasTagName(SVGNames::animateTag) || hasTagName(SVGNames::setTag) || hasTagName(SVGNames::animateTransformTag));
    ScriptWrappable::init(this);
}

PassRefPtr<SVGAnimateElement> SVGAnimateElement::create(Document& document)
{
    return adoptRef(new SVGAnimateElement(SVGNames::animateTag, document));
}

SVGAnimateElement::~SVGAnimateElement()
{
    if (targetElement())
        clearAnimatedType(targetElement());
}

bool SVGAnimateElement::hasValidAttributeType()
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    return m_animatedPropertyType != AnimatedUnknown && !hasInvalidCSSAttributeType();
}

void SVGAnimateElement::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGSMILElement* resultElement)
{
    ASSERT(resultElement);
    SVGElement* targetElement = this->targetElement();
    if (!targetElement || !isSVGAnimateElement(*resultElement))
        return;

    ASSERT(m_animatedPropertyType == determineAnimatedPropertyType());

    ASSERT(percentage >= 0 && percentage <= 1);
    ASSERT(m_animatedPropertyType != AnimatedTransformList || hasTagName(SVGNames::animateTransformTag));
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animator);
    ASSERT(m_animator->type() == m_animatedPropertyType);
    ASSERT(m_fromProperty);
    ASSERT(m_fromProperty->type() == m_animatedPropertyType);
    ASSERT(m_toProperty);

    SVGAnimateElement* resultAnimationElement = toSVGAnimateElement(resultElement);
    ASSERT(resultAnimationElement->m_animatedProperty);
    ASSERT(resultAnimationElement->m_animatedPropertyType == m_animatedPropertyType);

    if (hasTagName(SVGNames::setTag))
        percentage = 1;

    if (calcMode() == CalcModeDiscrete)
        percentage = percentage < 0.5 ? 0 : 1;

    // Target element might have changed.
    m_animator->setContextElement(targetElement);

    // Be sure to detach list wrappers before we modfiy their underlying value. If we'd do
    // if after calculateAnimatedValue() ran the cached pointers in the list propery tear
    // offs would point nowhere, and we couldn't create copies of those values anymore,
    // while detaching. This is covered by assertions, moving this down would fire them.
    if (!m_animatedProperties.isEmpty())
        m_animator->animValWillChange(m_animatedProperties);

    // Values-animation accumulates using the last values entry corresponding to the end of duration time.
    NewSVGPropertyBase* toAtEndOfDurationProperty = m_toAtEndOfDurationProperty ? m_toAtEndOfDurationProperty.get() : m_toProperty.get();
    m_animator->calculateAnimatedValue(percentage, repeatCount, m_fromProperty.get(), m_toProperty.get(), toAtEndOfDurationProperty, resultAnimationElement->m_animatedProperty.get());
}

bool SVGAnimateElement::calculateToAtEndOfDurationValue(const String& toAtEndOfDurationString)
{
    if (toAtEndOfDurationString.isEmpty())
        return false;
    m_toAtEndOfDurationProperty = ensureAnimator()->constructFromString(toAtEndOfDurationString);
    return true;
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString, const String& toString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    determinePropertyValueTypes(fromString, toString);
    ensureAnimator()->calculateFromAndToValues(m_fromProperty, m_toProperty, fromString, toString);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString, const String& byString)
{
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return false;

    if (animationMode() == ByAnimation && !isAdditive())
        return false;

    // from-by animation may only be used with attributes that support addition (e.g. most numeric attributes).
    if (animationMode() == FromByAnimation && !animatedPropertyTypeSupportsAddition())
        return false;

    ASSERT(!hasTagName(SVGNames::setTag));

    determinePropertyValueTypes(fromString, byString);
    ensureAnimator()->calculateFromAndByValues(m_fromProperty, m_toProperty, fromString, byString);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return true;
}

#ifndef NDEBUG
static inline bool propertyTypesAreConsistent(AnimatedPropertyType expectedPropertyType, const SVGElementAnimatedPropertyList& animatedTypes)
{
    SVGElementAnimatedPropertyList::const_iterator end = animatedTypes.end();
    for (SVGElementAnimatedPropertyList::const_iterator it = animatedTypes.begin(); it != end; ++it) {
        for (size_t i = 0; i < it->properties.size(); ++i) {
            if (expectedPropertyType != it->properties[i]->animatedPropertyType()) {
                // This is the only allowed inconsistency. SVGAnimatedAngleAnimator handles both SVGAnimatedAngle & SVGAnimatedEnumeration for markers orient attribute.
                if (expectedPropertyType == AnimatedAngle && it->properties[i]->animatedPropertyType() == AnimatedEnumeration)
                    return true;
                return false;
            }
        }
    }

    return true;
}
#endif

void SVGAnimateElement::resetAnimatedType()
{
    SVGAnimatedTypeAnimator* animator = ensureAnimator();
    ASSERT(m_animatedPropertyType == animator->type());

    SVGElement* targetElement = this->targetElement();
    const QualifiedName& attributeName = this->attributeName();
    ShouldApplyAnimation shouldApply = shouldApplyAnimation(targetElement, attributeName);

    if (shouldApply == DontApplyAnimation)
        return;

    if (shouldApply == ApplyXMLAnimation) {
        // SVG DOM animVal animation code-path.
        m_animatedProperties = animator->findAnimatedPropertiesForAttributeName(targetElement, attributeName);
        SVGElementAnimatedPropertyList::const_iterator end = m_animatedProperties.end();
        for (SVGElementAnimatedPropertyList::const_iterator it = m_animatedProperties.begin(); it != end; ++it)
            document().accessSVGExtensions().addElementReferencingTarget(this, it->element);

        ASSERT(!m_animatedProperties.isEmpty());

        ASSERT(propertyTypesAreConsistent(m_animatedPropertyType, m_animatedProperties));
        if (!m_animatedProperty)
            m_animatedProperty = animator->startAnimValAnimation(m_animatedProperties);
        else {
            m_animatedProperty = animator->resetAnimValToBaseVal(m_animatedProperties);
            animator->animValDidChange(m_animatedProperties);
        }
        return;
    }

    // CSS properties animation code-path.
    ASSERT(m_animatedProperties.isEmpty());
    String baseValue;

    if (shouldApply == ApplyCSSAnimation) {
        ASSERT(SVGAnimationElement::isTargetAttributeCSSProperty(targetElement, attributeName));
        computeCSSPropertyValue(targetElement, cssPropertyID(attributeName.localName()), baseValue);
    }

    m_animatedProperty = animator->constructFromString(baseValue);
}

static inline void applyCSSPropertyToTarget(SVGElement* targetElement, CSSPropertyID id, const String& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!targetElement->m_deletionHasBegun);

    MutableStylePropertySet* propertySet = targetElement->ensureAnimatedSMILStyleProperties();
    if (!propertySet->setProperty(id, value, false, 0))
        return;

    targetElement->setNeedsStyleRecalc(LocalStyleChange);
}

static inline void removeCSSPropertyFromTarget(SVGElement* targetElement, CSSPropertyID id)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!targetElement->m_deletionHasBegun);
    targetElement->ensureAnimatedSMILStyleProperties()->removeProperty(id);
    targetElement->setNeedsStyleRecalc(LocalStyleChange);
}

static inline void applyCSSPropertyToTargetAndInstances(SVGElement* targetElement, const QualifiedName& attributeName, const String& valueAsString)
{
    ASSERT(targetElement);
    if (attributeName == anyQName() || !targetElement->inDocument() || !targetElement->parentNode())
        return;

    CSSPropertyID id = cssPropertyID(attributeName.localName());

    SVGElementInstance::InstanceUpdateBlocker blocker(targetElement);
    applyCSSPropertyToTarget(targetElement, id, valueAsString);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    const HashSet<SVGElementInstance*>& instances = targetElement->instancesForElement();
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGElement* shadowTreeElement = (*it)->shadowTreeElement())
            applyCSSPropertyToTarget(shadowTreeElement, id, valueAsString);
    }
}

static inline void removeCSSPropertyFromTargetAndInstances(SVGElement* targetElement, const QualifiedName& attributeName)
{
    ASSERT(targetElement);
    if (attributeName == anyQName() || !targetElement->inDocument() || !targetElement->parentNode())
        return;

    CSSPropertyID id = cssPropertyID(attributeName.localName());

    SVGElementInstance::InstanceUpdateBlocker blocker(targetElement);
    removeCSSPropertyFromTarget(targetElement, id);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    const HashSet<SVGElementInstance*>& instances = targetElement->instancesForElement();
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGElement* shadowTreeElement = (*it)->shadowTreeElement())
            removeCSSPropertyFromTarget(shadowTreeElement, id);
    }
}

static inline void notifyTargetAboutAnimValChange(SVGElement* targetElement, const QualifiedName& attributeName)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!targetElement->m_deletionHasBegun);
    targetElement->invalidateSVGAttributes();
    targetElement->svgAttributeChanged(attributeName);
}

static inline void notifyTargetAndInstancesAboutAnimValChange(SVGElement* targetElement, const QualifiedName& attributeName)
{
    ASSERT(targetElement);
    if (attributeName == anyQName() || !targetElement->inDocument() || !targetElement->parentNode())
        return;

    SVGElementInstance::InstanceUpdateBlocker blocker(targetElement);
    notifyTargetAboutAnimValChange(targetElement, attributeName);

    // If the target element has instances, update them as well, w/o requiring the <use> tree to be rebuilt.
    const HashSet<SVGElementInstance*>& instances = targetElement->instancesForElement();
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGElement* shadowTreeElement = (*it)->shadowTreeElement())
            notifyTargetAboutAnimValChange(shadowTreeElement, attributeName);
    }
}

void SVGAnimateElement::clearAnimatedType(SVGElement* targetElement)
{
    if (!m_animatedProperty)
        return;

    if (!targetElement) {
        m_animatedProperty.clear();
        return;
    }

    if (m_animatedProperties.isEmpty()) {
        // CSS properties animation code-path.
        removeCSSPropertyFromTargetAndInstances(targetElement, attributeName());
        m_animatedProperty.clear();
        return;
    }

    // SVG DOM animVal animation code-path.
    if (m_animator) {
        m_animator->stopAnimValAnimation(m_animatedProperties);
        notifyTargetAndInstancesAboutAnimValChange(targetElement, attributeName());
    }

    m_animatedProperties.clear();
    m_animatedProperty.clear();
}

void SVGAnimateElement::applyResultsToTarget()
{
    ASSERT(m_animatedPropertyType != AnimatedTransformList || hasTagName(SVGNames::animateTransformTag));
    ASSERT(m_animatedPropertyType != AnimatedUnknown);
    ASSERT(m_animator);

    // Early exit if our animated type got destructed by a previous endedActiveInterval().
    if (!m_animatedProperty)
        return;

    if (m_animatedProperties.isEmpty()) {
        // CSS properties animation code-path.
        // Convert the result of the animation to a String and apply it as CSS property on the target & all instances.
        applyCSSPropertyToTargetAndInstances(targetElement(), attributeName(), m_animatedProperty->valueAsString());
        return;
    }

    // SVG DOM animVal animation code-path.
    // At this point the SVG DOM values are already changed, unlike for CSS.
    // We only have to trigger update notifications here.
    m_animator->animValDidChange(m_animatedProperties);
    notifyTargetAndInstancesAboutAnimValChange(targetElement(), attributeName());
}

bool SVGAnimateElement::animatedPropertyTypeSupportsAddition() const
{
    // Spec: http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
    switch (m_animatedPropertyType) {
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedPreserveAspectRatio:
    case AnimatedString:
    case AnimatedUnknown:
        return false;
    default:
        return true;
    }
}

bool SVGAnimateElement::isAdditive() const
{
    if (animationMode() == ByAnimation || animationMode() == FromByAnimation)
        if (!animatedPropertyTypeSupportsAddition())
            return false;

    return SVGAnimationElement::isAdditive();
}

float SVGAnimateElement::calculateDistance(const String& fromString, const String& toString)
{
    // FIXME: A return value of float is not enough to support paced animations on lists.
    SVGElement* targetElement = this->targetElement();
    if (!targetElement)
        return -1;

    return ensureAnimator()->calculateDistance(fromString, toString);
}

void SVGAnimateElement::setTargetElement(SVGElement* target)
{
    SVGAnimationElement::setTargetElement(target);
    resetAnimatedPropertyType();
}

void SVGAnimateElement::setAttributeName(const QualifiedName& attributeName)
{
    SVGAnimationElement::setAttributeName(attributeName);
    resetAnimatedPropertyType();
}

void SVGAnimateElement::resetAnimatedPropertyType()
{
    ASSERT(!m_animatedProperty);
    m_fromProperty.clear();
    m_toProperty.clear();
    m_toAtEndOfDurationProperty.clear();
    m_animator.clear();
    m_animatedPropertyType = determineAnimatedPropertyType();
}

SVGAnimatedTypeAnimator* SVGAnimateElement::ensureAnimator()
{
    if (!m_animator)
        m_animator = SVGAnimatorFactory::create(this, targetElement(), m_animatedPropertyType);
    ASSERT(m_animatedPropertyType == m_animator->type());
    return m_animator.get();
}

}
