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

#include "core/svg/SVGAnimateElement.h"

#include "core/XLinkNames.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/StyleChangeReason.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGString.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

namespace {

bool isTargetAttributeCSSProperty(const SVGElement& targetElement,
                                  const QualifiedName& attributeName) {
  return SVGElement::isAnimatableCSSProperty(attributeName) ||
         targetElement.isPresentationAttribute(attributeName);
}

String computeCSSPropertyValue(SVGElement* element, CSSPropertyID id) {
  DCHECK(element);
  // TODO(fs): StyleEngine doesn't support document without a frame.
  // Refer to comment in Element::computedStyle.
  DCHECK(element->inActiveDocument());

  // Don't include any properties resulting from CSS Transitions/Animations or
  // SMIL animations, as we want to retrieve the "base value".
  element->setUseOverrideComputedStyle(true);
  String value =
      CSSComputedStyleDeclaration::create(element)->getPropertyValue(id);
  element->setUseOverrideComputedStyle(false);
  return value;
}

AnimatedPropertyValueType propertyValueType(const QualifiedName& attributeName,
                                            const String& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, inherit, ("inherit"));
  if (value.isEmpty() || value != inherit ||
      !SVGElement::isAnimatableCSSProperty(attributeName))
    return RegularPropertyValue;
  return InheritValue;
}

QualifiedName constructQualifiedName(const SVGElement& svgElement,
                                     const AtomicString& attributeName) {
  if (attributeName.isEmpty())
    return anyQName();
  if (!attributeName.contains(':'))
    return QualifiedName(nullAtom, attributeName, nullAtom);

  AtomicString prefix;
  AtomicString localName;
  if (!Document::parseQualifiedName(attributeName, prefix, localName,
                                    IGNORE_EXCEPTION_FOR_TESTING))
    return anyQName();

  const AtomicString& namespaceURI = svgElement.lookupNamespaceURI(prefix);
  if (namespaceURI.isEmpty())
    return anyQName();

  QualifiedName resolvedAttrName(nullAtom, localName, namespaceURI);
  // "Animation elements treat attributeName='xlink:href' as being an alias
  // for targetting the 'href' attribute."
  // https://svgwg.org/svg2-draft/types.html#__svg__SVGURIReference__href
  if (resolvedAttrName == XLinkNames::hrefAttr)
    return SVGNames::hrefAttr;
  return resolvedAttrName;
}

}  // unnamed namespace

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tagName,
                                     Document& document)
    : SVGAnimationElement(tagName, document),
      m_type(AnimatedUnknown),
      m_cssPropertyId(CSSPropertyInvalid),
      m_fromPropertyValueType(RegularPropertyValue),
      m_toPropertyValueType(RegularPropertyValue),
      m_attributeType(AttributeTypeAuto),
      m_hasInvalidCSSAttributeType(false) {}

SVGAnimateElement* SVGAnimateElement::create(Document& document) {
  return new SVGAnimateElement(SVGNames::animateTag, document);
}

SVGAnimateElement::~SVGAnimateElement() {}

bool SVGAnimateElement::isSVGAnimationAttributeSettingJavaScriptURL(
    const Attribute& attribute) const {
  if ((attribute.name() == SVGNames::fromAttr ||
       attribute.name() == SVGNames::toAttr) &&
      attributeValueIsJavaScriptURL(attribute))
    return true;

  if (attribute.name() == SVGNames::valuesAttr) {
    Vector<String> parts;
    if (!parseValues(attribute.value(), parts)) {
      // Assume the worst.
      return true;
    }
    for (const auto& part : parts) {
      if (protocolIsJavaScript(part))
        return true;
    }
  }

  return SVGSMILElement::isSVGAnimationAttributeSettingJavaScriptURL(attribute);
}

Node::InsertionNotificationRequest SVGAnimateElement::insertedInto(
    ContainerNode* rootParent) {
  SVGAnimationElement::insertedInto(rootParent);
  if (rootParent->isConnected()) {
    setAttributeName(constructQualifiedName(
        *this, fastGetAttribute(SVGNames::attributeNameAttr)));
  }
  return InsertionDone;
}

void SVGAnimateElement::removedFrom(ContainerNode* rootParent) {
  if (rootParent->isConnected())
    setAttributeName(anyQName());
  SVGAnimationElement::removedFrom(rootParent);
}

void SVGAnimateElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == SVGNames::attributeTypeAttr) {
    setAttributeType(params.newValue);
    animationAttributeChanged();
    return;
  }
  if (params.name == SVGNames::attributeNameAttr) {
    setAttributeName(constructQualifiedName(*this, params.newValue));
    animationAttributeChanged();
    return;
  }
  SVGAnimationElement::parseAttribute(params);
}

void SVGAnimateElement::resolveTargetProperty() {
  DCHECK(targetElement());
  m_targetProperty = targetElement()->propertyFromAttribute(attributeName());
  if (m_targetProperty) {
    m_type = m_targetProperty->type();
    m_cssPropertyId = m_targetProperty->cssPropertyId();

    // Only <animateTransform> is allowed to animate AnimatedTransformList.
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (m_type == AnimatedTransformList) {
      m_type = AnimatedUnknown;
      m_cssPropertyId = CSSPropertyInvalid;
    }
  } else {
    m_type = SVGElement::animatedPropertyTypeForCSSAttribute(attributeName());
    m_cssPropertyId = m_type != AnimatedUnknown
                          ? cssPropertyID(attributeName().localName())
                          : CSSPropertyInvalid;
  }
  // Blacklist <script> targets here for now to prevent unpleasantries. This
  // also disallows the perfectly "valid" animation of 'className' on said
  // element. If SVGScriptElement.href is transitioned off of SVGAnimatedHref,
  // this can be removed.
  if (isSVGScriptElement(*targetElement())) {
    m_type = AnimatedUnknown;
    m_cssPropertyId = CSSPropertyInvalid;
  }
  DCHECK(m_type != AnimatedPoint && m_type != AnimatedStringList &&
         m_type != AnimatedTransform && m_type != AnimatedTransformList);
}

void SVGAnimateElement::clearTargetProperty() {
  m_targetProperty = nullptr;
  m_type = AnimatedUnknown;
  m_cssPropertyId = CSSPropertyInvalid;
}

AnimatedPropertyType SVGAnimateElement::animatedPropertyType() {
  if (!targetElement())
    return AnimatedUnknown;
  resolveTargetProperty();
  return m_type;
}

bool SVGAnimateElement::hasValidTarget() {
  if (!SVGAnimationElement::hasValidTarget())
    return false;
  if (!hasValidAttributeName())
    return false;
  resolveTargetProperty();
  return m_type != AnimatedUnknown && !m_hasInvalidCSSAttributeType;
}

bool SVGAnimateElement::hasValidAttributeName() const {
  return attributeName() != anyQName();
}

bool SVGAnimateElement::shouldApplyAnimation(
    const SVGElement& targetElement,
    const QualifiedName& attributeName) {
  if (!hasValidTarget() || !targetElement.parentNode())
    return false;

  // Always animate CSS properties using the ApplyCSSAnimation code path,
  // regardless of the attributeType value.
  if (isTargetAttributeCSSProperty(targetElement, attributeName))
    return true;

  // If attributeType="CSS" and attributeName doesn't point to a CSS property,
  // ignore the animation.
  return getAttributeType() != AttributeTypeCSS;
}

SVGPropertyBase* SVGAnimateElement::createPropertyForAttributeAnimation(
    const String& value) const {
  // SVG DOM animVal animation code-path.
  // TransformList must be animated via <animateTransform>, and its
  // {from,by,to} attribute values needs to be parsed w.r.t. its "type"
  // attribute. Spec:
  // http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
  DCHECK_NE(m_type, AnimatedTransformList);
  DCHECK(m_targetProperty);
  return m_targetProperty->currentValueBase()->cloneForAnimation(value);
}

SVGPropertyBase* SVGAnimateElement::createPropertyForCSSAnimation(
    const String& value) const {
  // CSS properties animation code-path.
  // Create a basic instance of the corresponding SVG property.
  // The instance will not have full context info. (e.g. SVGLengthMode)
  switch (m_type) {
    case AnimatedColor:
      return SVGColorProperty::create(value);
    case AnimatedNumber: {
      SVGNumber* property = SVGNumber::create();
      property->setValueAsString(value);
      return property;
    }
    case AnimatedLength: {
      SVGLength* property = SVGLength::create();
      property->setValueAsString(value);
      return property;
    }
    case AnimatedLengthList: {
      SVGLengthList* property = SVGLengthList::create();
      property->setValueAsString(value);
      return property;
    }
    case AnimatedString: {
      SVGString* property = SVGString::create();
      property->setValueAsString(value);
      return property;
    }
    // These types don't appear in the table in
    // SVGElement::animatedPropertyTypeForCSSAttribute() and thus don't need
    // support.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPath:
    case AnimatedPoint:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedRect:
    case AnimatedStringList:
    case AnimatedTransform:
    case AnimatedTransformList:
    case AnimatedUnknown:
      break;
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

SVGPropertyBase* SVGAnimateElement::createPropertyForAnimation(
    const String& value) const {
  if (isAnimatingSVGDom())
    return createPropertyForAttributeAnimation(value);
  DCHECK(isAnimatingCSSProperty());
  return createPropertyForCSSAnimation(value);
}

SVGPropertyBase* SVGAnimateElement::adjustForInheritance(
    SVGPropertyBase* propertyValue,
    AnimatedPropertyValueType valueType) const {
  if (valueType != InheritValue)
    return propertyValue;
  // TODO(fs): At the moment the computed style gets returned as a String and
  // needs to get parsed again. In the future we might want to work with the
  // value type directly to avoid the String parsing.
  DCHECK(targetElement());
  Element* parent = targetElement()->parentElement();
  if (!parent || !parent->isSVGElement())
    return propertyValue;
  SVGElement* svgParent = toSVGElement(parent);
  // Replace 'inherit' by its computed property value.
  String value = computeCSSPropertyValue(svgParent, m_cssPropertyId);
  return createPropertyForAnimation(value);
}

void SVGAnimateElement::calculateAnimatedValue(float percentage,
                                               unsigned repeatCount,
                                               SVGSMILElement* resultElement) {
  DCHECK(resultElement);
  DCHECK(targetElement());
  if (!isSVGAnimateElement(*resultElement))
    return;

  DCHECK(percentage >= 0 && percentage <= 1);
  DCHECK_NE(animatedPropertyType(), AnimatedUnknown);
  DCHECK(m_fromProperty);
  DCHECK_EQ(m_fromProperty->type(), animatedPropertyType());
  DCHECK(m_toProperty);

  SVGAnimateElement* resultAnimationElement =
      toSVGAnimateElement(resultElement);
  DCHECK(resultAnimationElement->m_animatedValue);
  DCHECK_EQ(resultAnimationElement->animatedPropertyType(),
            animatedPropertyType());

  if (isSVGSetElement(*this))
    percentage = 1;

  if (getCalcMode() == CalcModeDiscrete)
    percentage = percentage < 0.5 ? 0 : 1;

  // Target element might have changed.
  SVGElement* targetElement = this->targetElement();

  // Values-animation accumulates using the last values entry corresponding to
  // the end of duration time.
  SVGPropertyBase* animatedValue = resultAnimationElement->m_animatedValue;
  SVGPropertyBase* toAtEndOfDurationValue =
      m_toAtEndOfDurationProperty ? m_toAtEndOfDurationProperty : m_toProperty;
  SVGPropertyBase* fromValue =
      getAnimationMode() == ToAnimation ? animatedValue : m_fromProperty.get();
  SVGPropertyBase* toValue = m_toProperty;

  // Apply CSS inheritance rules.
  fromValue = adjustForInheritance(fromValue, m_fromPropertyValueType);
  toValue = adjustForInheritance(toValue, m_toPropertyValueType);

  animatedValue->calculateAnimatedValue(this, percentage, repeatCount,
                                        fromValue, toValue,
                                        toAtEndOfDurationValue, targetElement);
}

bool SVGAnimateElement::calculateToAtEndOfDurationValue(
    const String& toAtEndOfDurationString) {
  if (toAtEndOfDurationString.isEmpty())
    return false;
  m_toAtEndOfDurationProperty =
      createPropertyForAnimation(toAtEndOfDurationString);
  return true;
}

bool SVGAnimateElement::calculateFromAndToValues(const String& fromString,
                                                 const String& toString) {
  DCHECK(targetElement());
  m_fromProperty = createPropertyForAnimation(fromString);
  m_fromPropertyValueType = propertyValueType(attributeName(), fromString);
  m_toProperty = createPropertyForAnimation(toString);
  m_toPropertyValueType = propertyValueType(attributeName(), toString);
  return true;
}

bool SVGAnimateElement::calculateFromAndByValues(const String& fromString,
                                                 const String& byString) {
  DCHECK(targetElement());

  if (getAnimationMode() == ByAnimation && !isAdditive())
    return false;

  // from-by animation may only be used with attributes that support addition
  // (e.g. most numeric attributes).
  if (getAnimationMode() == FromByAnimation &&
      !animatedPropertyTypeSupportsAddition())
    return false;

  DCHECK(!isSVGSetElement(*this));

  m_fromProperty = createPropertyForAnimation(fromString);
  m_fromPropertyValueType = propertyValueType(attributeName(), fromString);
  m_toProperty = createPropertyForAnimation(byString);
  m_toPropertyValueType = propertyValueType(attributeName(), byString);
  m_toProperty->add(m_fromProperty, targetElement());
  return true;
}

void SVGAnimateElement::resetAnimatedType() {
  resolveTargetProperty();

  SVGElement* targetElement = this->targetElement();
  const QualifiedName& attributeName = this->attributeName();

  if (!shouldApplyAnimation(*targetElement, attributeName))
    return;
  if (isAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    m_animatedValue = m_targetProperty->createAnimatedValue();
    DCHECK_EQ(m_animatedValue->type(), m_type);
    targetElement->setAnimatedAttribute(attributeName, m_animatedValue);
    return;
  }
  DCHECK(isAnimatingCSSProperty());
  // Presentation attributes which has an SVG DOM representation should use the
  // "SVG DOM" code-path (above.)
  DCHECK(SVGElement::isAnimatableCSSProperty(attributeName));

  // CSS properties animation code-path.
  String baseValue = computeCSSPropertyValue(targetElement, m_cssPropertyId);
  m_animatedValue = createPropertyForAnimation(baseValue);
}

void SVGAnimateElement::clearAnimatedType() {
  if (!m_animatedValue)
    return;

  // The animated property lock is held for the "result animation" (see
  // SMILTimeContainer::updateAnimations()) while we're processing an animation
  // group. We will very likely crash later if we clear the animated type while
  // the lock is held. See crbug.com/581546.
  DCHECK(!animatedTypeIsLocked());

  SVGElement* targetElement = this->targetElement();
  if (!targetElement) {
    m_animatedValue.clear();
    return;
  }

  bool shouldApply = shouldApplyAnimation(*targetElement, attributeName());
  if (isAnimatingCSSProperty()) {
    // CSS properties animation code-path.
    if (shouldApply) {
      MutableStylePropertySet* propertySet =
          targetElement->ensureAnimatedSMILStyleProperties();
      if (propertySet->removeProperty(m_cssPropertyId)) {
        targetElement->setNeedsStyleRecalc(
            LocalStyleChange,
            StyleChangeReasonForTracing::create(StyleChangeReason::Animation));
      }
    }
  }
  if (isAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    targetElement->clearAnimatedAttribute(attributeName());
    if (shouldApply)
      targetElement->invalidateAnimatedAttribute(attributeName());
  }

  m_animatedValue.clear();
  clearTargetProperty();
}

void SVGAnimateElement::applyResultsToTarget() {
  DCHECK_NE(animatedPropertyType(), AnimatedUnknown);

  // Early exit if our animated type got destructed by a previous
  // endedActiveInterval().
  if (!m_animatedValue)
    return;

  if (!shouldApplyAnimation(*targetElement(), attributeName()))
    return;

  // We do update the style and the animation property independent of each
  // other.
  if (isAnimatingCSSProperty()) {
    // CSS properties animation code-path.
    // Convert the result of the animation to a String and apply it as CSS
    // property on the target.
    MutableStylePropertySet* propertySet =
        targetElement()->ensureAnimatedSMILStyleProperties();
    if (propertySet
            ->setProperty(m_cssPropertyId, m_animatedValue->valueAsString(),
                          false, 0)
            .didChange) {
      targetElement()->setNeedsStyleRecalc(
          LocalStyleChange,
          StyleChangeReasonForTracing::create(StyleChangeReason::Animation));
    }
  }
  if (isAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    // At this point the SVG DOM values are already changed, unlike for CSS.
    // We only have to trigger update notifications here.
    targetElement()->invalidateAnimatedAttribute(attributeName());
  }
}

bool SVGAnimateElement::animatedPropertyTypeSupportsAddition() {
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
  switch (animatedPropertyType()) {
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

bool SVGAnimateElement::isAdditive() {
  if (getAnimationMode() == ByAnimation ||
      getAnimationMode() == FromByAnimation) {
    if (!animatedPropertyTypeSupportsAddition())
      return false;
  }

  return SVGAnimationElement::isAdditive();
}

float SVGAnimateElement::calculateDistance(const String& fromString,
                                           const String& toString) {
  DCHECK(targetElement());
  // FIXME: A return value of float is not enough to support paced animations on
  // lists.
  SVGPropertyBase* fromValue = createPropertyForAnimation(fromString);
  SVGPropertyBase* toValue = createPropertyForAnimation(toString);
  return fromValue->calculateDistance(toValue, targetElement());
}

void SVGAnimateElement::setTargetElement(SVGElement* target) {
  SVGAnimationElement::setTargetElement(target);
  checkInvalidCSSAttributeType();
  resetAnimatedPropertyType();
}

void SVGAnimateElement::setAttributeName(const QualifiedName& attributeName) {
  unscheduleIfScheduled();
  if (targetElement())
    clearAnimatedType();
  m_attributeName = attributeName;
  schedule();
  checkInvalidCSSAttributeType();
  resetAnimatedPropertyType();
}

void SVGAnimateElement::setAttributeType(const AtomicString& attributeType) {
  if (attributeType == "CSS")
    m_attributeType = AttributeTypeCSS;
  else if (attributeType == "XML")
    m_attributeType = AttributeTypeXML;
  else
    m_attributeType = AttributeTypeAuto;
  checkInvalidCSSAttributeType();
}

void SVGAnimateElement::checkInvalidCSSAttributeType() {
  bool hasInvalidCSSAttributeType =
      targetElement() && hasValidAttributeName() &&
      getAttributeType() == AttributeTypeCSS &&
      !isTargetAttributeCSSProperty(*targetElement(), attributeName());

  if (hasInvalidCSSAttributeType != m_hasInvalidCSSAttributeType) {
    if (hasInvalidCSSAttributeType)
      unscheduleIfScheduled();

    m_hasInvalidCSSAttributeType = hasInvalidCSSAttributeType;

    if (!hasInvalidCSSAttributeType)
      schedule();
  }

  // Clear values that may depend on the previous target.
  if (targetElement())
    clearAnimatedType();
}

void SVGAnimateElement::resetAnimatedPropertyType() {
  DCHECK(!m_animatedValue);
  m_fromProperty.clear();
  m_toProperty.clear();
  m_toAtEndOfDurationProperty.clear();
  clearTargetProperty();
}

DEFINE_TRACE(SVGAnimateElement) {
  visitor->trace(m_fromProperty);
  visitor->trace(m_toProperty);
  visitor->trace(m_toAtEndOfDurationProperty);
  visitor->trace(m_animatedValue);
  visitor->trace(m_targetProperty);
  SVGAnimationElement::trace(visitor);
}

}  // namespace blink
