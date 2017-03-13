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

#include "core/svg/SVGRadialGradientElement.h"

#include "core/layout/svg/LayoutSVGResourceRadialGradient.h"
#include "core/svg/RadialGradientAttributes.h"

namespace blink {

inline SVGRadialGradientElement::SVGRadialGradientElement(Document& document)
    : SVGGradientElement(SVGNames::radialGradientTag, document),
      m_cx(SVGAnimatedLength::create(this,
                                     SVGNames::cxAttr,
                                     SVGLength::create(SVGLengthMode::Width))),
      m_cy(SVGAnimatedLength::create(this,
                                     SVGNames::cyAttr,
                                     SVGLength::create(SVGLengthMode::Height))),
      m_r(SVGAnimatedLength::create(this,
                                    SVGNames::rAttr,
                                    SVGLength::create(SVGLengthMode::Other))),
      m_fx(SVGAnimatedLength::create(this,
                                     SVGNames::fxAttr,
                                     SVGLength::create(SVGLengthMode::Width))),
      m_fy(SVGAnimatedLength::create(this,
                                     SVGNames::fyAttr,
                                     SVGLength::create(SVGLengthMode::Height))),
      m_fr(SVGAnimatedLength::create(this,
                                     SVGNames::frAttr,
                                     SVGLength::create(SVGLengthMode::Other))) {
  // Spec: If the cx/cy/r attribute is not specified, the effect is as if a
  // value of "50%" were specified.
  m_cx->setDefaultValueAsString("50%");
  m_cy->setDefaultValueAsString("50%");
  m_r->setDefaultValueAsString("50%");

  // SVG2-Draft Spec: If the fr attributed is not specified, the effect is as if
  // a value of "0%" were specified.
  m_fr->setDefaultValueAsString("0%");

  addToPropertyMap(m_cx);
  addToPropertyMap(m_cy);
  addToPropertyMap(m_r);
  addToPropertyMap(m_fx);
  addToPropertyMap(m_fy);
  addToPropertyMap(m_fr);
}

DEFINE_TRACE(SVGRadialGradientElement) {
  visitor->trace(m_cx);
  visitor->trace(m_cy);
  visitor->trace(m_r);
  visitor->trace(m_fx);
  visitor->trace(m_fy);
  visitor->trace(m_fr);
  SVGGradientElement::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGRadialGradientElement)

void SVGRadialGradientElement::svgAttributeChanged(
    const QualifiedName& attrName) {
  if (attrName == SVGNames::cxAttr || attrName == SVGNames::cyAttr ||
      attrName == SVGNames::fxAttr || attrName == SVGNames::fyAttr ||
      attrName == SVGNames::rAttr || attrName == SVGNames::frAttr) {
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

LayoutObject* SVGRadialGradientElement::createLayoutObject(
    const ComputedStyle&) {
  return new LayoutSVGResourceRadialGradient(this);
}

static void setGradientAttributes(const SVGGradientElement& element,
                                  RadialGradientAttributes& attributes,
                                  bool isRadial) {
  element.collectCommonAttributes(attributes);

  if (!isRadial)
    return;
  const SVGRadialGradientElement& radial = toSVGRadialGradientElement(element);

  if (!attributes.hasCx() && radial.cx()->isSpecified())
    attributes.setCx(radial.cx()->currentValue());

  if (!attributes.hasCy() && radial.cy()->isSpecified())
    attributes.setCy(radial.cy()->currentValue());

  if (!attributes.hasR() && radial.r()->isSpecified())
    attributes.setR(radial.r()->currentValue());

  if (!attributes.hasFx() && radial.fx()->isSpecified())
    attributes.setFx(radial.fx()->currentValue());

  if (!attributes.hasFy() && radial.fy()->isSpecified())
    attributes.setFy(radial.fy()->currentValue());

  if (!attributes.hasFr() && radial.fr()->isSpecified())
    attributes.setFr(radial.fr()->currentValue());
}

bool SVGRadialGradientElement::collectGradientAttributes(
    RadialGradientAttributes& attributes) {
  DCHECK(layoutObject());

  VisitedSet visited;
  const SVGGradientElement* current = this;

  while (true) {
    setGradientAttributes(*current, attributes,
                          isSVGRadialGradientElement(*current));
    visited.insert(current);

    current = current->referencedElement();
    if (!current || visited.contains(current))
      break;
    if (!current->layoutObject())
      return false;
  }

  // Handle default values for fx/fy
  if (!attributes.hasFx())
    attributes.setFx(attributes.cx());

  if (!attributes.hasFy())
    attributes.setFy(attributes.cy());

  return true;
}

bool SVGRadialGradientElement::selfHasRelativeLengths() const {
  return m_cx->currentValue()->isRelative() ||
         m_cy->currentValue()->isRelative() ||
         m_r->currentValue()->isRelative() ||
         m_fx->currentValue()->isRelative() ||
         m_fy->currentValue()->isRelative() ||
         m_fr->currentValue()->isRelative();
}

}  // namespace blink
