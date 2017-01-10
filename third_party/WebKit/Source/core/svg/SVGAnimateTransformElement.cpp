/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "core/svg/SVGAnimateTransformElement.h"

#include "core/SVGNames.h"
#include "core/svg/SVGTransformList.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

inline SVGAnimateTransformElement::SVGAnimateTransformElement(
    Document& document)
    : SVGAnimateElement(SVGNames::animateTransformTag, document),
      m_transformType(kSvgTransformUnknown) {}

DEFINE_NODE_FACTORY(SVGAnimateTransformElement)

bool SVGAnimateTransformElement::hasValidTarget() {
  if (!SVGAnimateElement::hasValidTarget())
    return false;
  if (getAttributeType() == AttributeTypeCSS)
    return false;
  return m_type == AnimatedTransformList;
}

void SVGAnimateTransformElement::resolveTargetProperty() {
  DCHECK(targetElement());
  m_targetProperty = targetElement()->propertyFromAttribute(attributeName());
  m_type = m_targetProperty ? m_targetProperty->type() : AnimatedUnknown;
  // <animateTransform> only animates AnimatedTransformList.
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
  if (m_type != AnimatedTransformList)
    m_type = AnimatedUnknown;
  // Because of the syntactic mismatch between the CSS and SVGProperty
  // representations, disallow CSS animations of transforms. Support for that
  // is better added to the <animate> element since the <animateTransform>
  // element is deprecated and quirky. (We also reject this case via
  // hasValidAttributeType above.)
  m_cssPropertyId = CSSPropertyInvalid;
}

SVGPropertyBase* SVGAnimateTransformElement::createPropertyForAnimation(
    const String& value) const {
  DCHECK(isAnimatingSVGDom());
  return SVGTransformList::create(m_transformType, value);
}

void SVGAnimateTransformElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == SVGNames::typeAttr) {
    m_transformType = parseTransformType(params.newValue);
    if (m_transformType == kSvgTransformMatrix)
      m_transformType = kSvgTransformUnknown;
    return;
  }

  SVGAnimateElement::parseAttribute(params);
}

}  // namespace blink
