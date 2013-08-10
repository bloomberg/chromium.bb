/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "config.h"

#include "core/svg/SVGGradientElement.h"

#include "SVGNames.h"
#include "XLinkNames.h"
#include "core/dom/Attribute.h"
#include "core/rendering/svg/RenderSVGHiddenContainer.h"
#include "core/rendering/svg/RenderSVGPath.h"
#include "core/rendering/svg/RenderSVGResourceLinearGradient.h"
#include "core/rendering/svg/RenderSVGResourceRadialGradient.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGStopElement.h"
#include "core/svg/SVGTransformList.h"
#include "core/svg/SVGTransformable.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_ENUMERATION(SVGGradientElement, SVGNames::spreadMethodAttr, SpreadMethod, spreadMethod, SVGSpreadMethodType)
DEFINE_ANIMATED_ENUMERATION(SVGGradientElement, SVGNames::gradientUnitsAttr, GradientUnits, gradientUnits, SVGUnitTypes::SVGUnitType)
DEFINE_ANIMATED_TRANSFORM_LIST(SVGGradientElement, SVGNames::gradientTransformAttr, GradientTransform, gradientTransform)
DEFINE_ANIMATED_STRING(SVGGradientElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGGradientElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGGradientElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(spreadMethod)
    REGISTER_LOCAL_ANIMATED_PROPERTY(gradientUnits)
    REGISTER_LOCAL_ANIMATED_PROPERTY(gradientTransform)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGElement)
END_REGISTER_ANIMATED_PROPERTIES

SVGGradientElement::SVGGradientElement(const QualifiedName& tagName, Document* document)
    : SVGElement(tagName, document)
    , m_spreadMethod(SVGSpreadMethodPad)
    , m_gradientUnits(SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
{
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGGradientElement();
}

bool SVGGradientElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::gradientUnitsAttr);
        supportedAttributes.add(SVGNames::gradientTransformAttr);
        supportedAttributes.add(SVGNames::spreadMethodAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGGradientElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::gradientUnitsAttr) {
        SVGUnitTypes::SVGUnitType propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(value);
        if (propertyValue > 0)
            setGradientUnitsBaseValue(propertyValue);
        return;
    }

    if (name == SVGNames::gradientTransformAttr) {
        SVGTransformList newList;
        newList.parse(value);
        detachAnimatedGradientTransformListWrappers(newList.size());
        setGradientTransformBaseValue(newList);
        return;
    }

    if (name == SVGNames::spreadMethodAttr) {
        SVGSpreadMethodType propertyValue = SVGPropertyTraits<SVGSpreadMethodType>::fromString(value);
        if (propertyValue > 0)
            setSpreadMethodBaseValue(propertyValue);
        return;
    }

    if (SVGURIReference::parseAttribute(name, value))
        return;
    if (SVGExternalResourcesRequired::parseAttribute(name, value))
        return;

    ASSERT_NOT_REACHED();
}

void SVGGradientElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

void SVGGradientElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    if (changedByParser)
        return;

    if (RenderObject* object = renderer())
        object->setNeedsLayout();
}

Vector<Gradient::ColorStop> SVGGradientElement::buildStops()
{
    Vector<Gradient::ColorStop> stops;

    float previousOffset = 0.0f;
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        SVGElement* element = n->isSVGElement() ? toSVGElement(n) : 0;
        if (!element || !element->isGradientStop())
            continue;

        SVGStopElement* stop = toSVGStopElement(element);
        Color color = stop->stopColorIncludingOpacity();

        // Figure out right monotonic offset
        float offset = stop->offsetCurrentValue();
        offset = std::min(std::max(previousOffset, offset), 1.0f);
        previousOffset = offset;

        // Extract individual channel values
        // FIXME: Why doesn't ColorStop take a Color and an offset??
        float r, g, b, a;
        color.getRGBA(r, g, b, a);

        stops.append(Gradient::ColorStop(offset, r, g, b, a));
    }

    return stops;
}

}
