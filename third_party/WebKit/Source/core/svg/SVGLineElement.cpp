/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "core/svg/SVGLineElement.h"

#include "core/layout/svg/LayoutSVGShape.h"
#include "core/svg/SVGLength.h"

namespace blink {

inline SVGLineElement::SVGLineElement(Document& document)
    : SVGGeometryElement(SVGNames::lineTag, document)
    , m_x1(SVGAnimatedLength::create(this, SVGNames::x1Attr, SVGLength::create(SVGLengthMode::Width), AllowNegativeLengths))
    , m_y1(SVGAnimatedLength::create(this, SVGNames::y1Attr, SVGLength::create(SVGLengthMode::Height), AllowNegativeLengths))
    , m_x2(SVGAnimatedLength::create(this, SVGNames::x2Attr, SVGLength::create(SVGLengthMode::Width), AllowNegativeLengths))
    , m_y2(SVGAnimatedLength::create(this, SVGNames::y2Attr, SVGLength::create(SVGLengthMode::Height), AllowNegativeLengths))
{
    addToPropertyMap(m_x1);
    addToPropertyMap(m_y1);
    addToPropertyMap(m_x2);
    addToPropertyMap(m_y2);
}

DEFINE_TRACE(SVGLineElement)
{
    visitor->trace(m_x1);
    visitor->trace(m_y1);
    visitor->trace(m_x2);
    visitor->trace(m_y2);
    SVGGeometryElement::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGLineElement)

bool SVGLineElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        supportedAttributes.add(SVGNames::x1Attr);
        supportedAttributes.add(SVGNames::x2Attr);
        supportedAttributes.add(SVGNames::y1Attr);
        supportedAttributes.add(SVGNames::y2Attr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGLineElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    parseAttributeNew(name, value);
}

void SVGLineElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGeometryElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::x1Attr
                          || attrName == SVGNames::y1Attr
                          || attrName == SVGNames::x2Attr
                          || attrName == SVGNames::y2Attr;

    if (isLengthAttribute)
        updateRelativeLengthsInformation();

    LayoutSVGShape* renderer = toLayoutSVGShape(this->renderer());
    if (!renderer)
        return;

    if (isLengthAttribute) {
        renderer->setNeedsShapeUpdate();
        markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool SVGLineElement::selfHasRelativeLengths() const
{
    return m_x1->currentValue()->isRelative()
        || m_y1->currentValue()->isRelative()
        || m_x2->currentValue()->isRelative()
        || m_y2->currentValue()->isRelative();
}

} // namespace blink
