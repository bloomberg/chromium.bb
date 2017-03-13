/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGLinearGradientElement.h"

#include "core/layout/svg/LayoutSVGResourceLinearGradient.h"
#include "core/svg/LinearGradientAttributes.h"
#include "core/svg/SVGLength.h"

namespace blink {

inline SVGLinearGradientElement::SVGLinearGradientElement(Document& document)
    : SVGGradientElement(SVGNames::linearGradientTag, document),
      m_x1(SVGAnimatedLength::create(this,
                                     SVGNames::x1Attr,
                                     SVGLength::create(SVGLengthMode::Width))),
      m_y1(SVGAnimatedLength::create(this,
                                     SVGNames::y1Attr,
                                     SVGLength::create(SVGLengthMode::Height))),
      m_x2(SVGAnimatedLength::create(this,
                                     SVGNames::x2Attr,
                                     SVGLength::create(SVGLengthMode::Width))),
      m_y2(
          SVGAnimatedLength::create(this,
                                    SVGNames::y2Attr,
                                    SVGLength::create(SVGLengthMode::Height))) {
  // Spec: If the x1|y1|y2 attribute is not specified, the effect is as if a
  // value of "0%" were specified.
  m_x1->setDefaultValueAsString("0%");
  m_y1->setDefaultValueAsString("0%");
  m_y2->setDefaultValueAsString("0%");

  // Spec: If the x2 attribute is not specified, the effect is as if a value of
  // "100%" were specified.
  m_x2->setDefaultValueAsString("100%");

  addToPropertyMap(m_x1);
  addToPropertyMap(m_y1);
  addToPropertyMap(m_x2);
  addToPropertyMap(m_y2);
}

DEFINE_TRACE(SVGLinearGradientElement) {
  visitor->trace(m_x1);
  visitor->trace(m_y1);
  visitor->trace(m_x2);
  visitor->trace(m_y2);
  SVGGradientElement::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGLinearGradientElement)

void SVGLinearGradientElement::svgAttributeChanged(
    const QualifiedName& attrName) {
  if (attrName == SVGNames::x1Attr || attrName == SVGNames::x2Attr ||
      attrName == SVGNames::y1Attr || attrName == SVGNames::y2Attr) {
    SVGElement::InvalidationGuard invalidationGuard(this);

    updateRelativeLengthsInformation();

    LayoutSVGResourceContainer* layoutObject =
        toLayoutSVGResourceContainer(this->layoutObject());
    if (layoutObject)
      layoutObject->invalidateCacheAndMarkForLayout();

    return;
  }

  SVGGradientElement::svgAttributeChanged(attrName);
}

LayoutObject* SVGLinearGradientElement::createLayoutObject(
    const ComputedStyle&) {
  return new LayoutSVGResourceLinearGradient(this);
}

static void setGradientAttributes(const SVGGradientElement& element,
                                  LinearGradientAttributes& attributes,
                                  bool isLinear) {
  element.collectCommonAttributes(attributes);

  if (!isLinear)
    return;
  const SVGLinearGradientElement& linear = toSVGLinearGradientElement(element);

  if (!attributes.hasX1() && linear.x1()->isSpecified())
    attributes.setX1(linear.x1()->currentValue());

  if (!attributes.hasY1() && linear.y1()->isSpecified())
    attributes.setY1(linear.y1()->currentValue());

  if (!attributes.hasX2() && linear.x2()->isSpecified())
    attributes.setX2(linear.x2()->currentValue());

  if (!attributes.hasY2() && linear.y2()->isSpecified())
    attributes.setY2(linear.y2()->currentValue());
}

bool SVGLinearGradientElement::collectGradientAttributes(
    LinearGradientAttributes& attributes) {
  DCHECK(layoutObject());

  VisitedSet visited;
  const SVGGradientElement* current = this;

  while (true) {
    setGradientAttributes(*current, attributes,
                          isSVGLinearGradientElement(*current));
    visited.insert(current);

    current = current->referencedElement();
    if (!current || visited.contains(current))
      break;
    if (!current->layoutObject())
      return false;
  }
  return true;
}

bool SVGLinearGradientElement::selfHasRelativeLengths() const {
  return m_x1->currentValue()->isRelative() ||
         m_y1->currentValue()->isRelative() ||
         m_x2->currentValue()->isRelative() ||
         m_y2->currentValue()->isRelative();
}

}  // namespace blink
