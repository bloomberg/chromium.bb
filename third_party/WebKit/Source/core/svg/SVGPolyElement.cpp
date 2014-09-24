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
#include "core/svg/SVGPolyElement.h"

#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/svg/SVGAnimatedPointList.h"
#include "core/svg/SVGParserUtilities.h"

namespace blink {

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document)
    , m_points(SVGAnimatedPointList::create(this, SVGNames::pointsAttr, SVGPointList::create()))
{
    addToPropertyMap(m_points);
}

void SVGPolyElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    parseAttributeNew(name, value);
}

void SVGPolyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName != SVGNames::pointsAttr) {
        SVGGeometryElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    RenderSVGShape* renderer = toRenderSVGShape(this->renderer());
    if (!renderer)
        return;

    renderer->setNeedsShapeUpdate();
    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
}

}
