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
#include "core/svg/SVGPathElement.h"

#include "core/layout/svg/LayoutSVGPath.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGMPathElement.h"
#include "core/svg/SVGPathUtilities.h"
#include "core/svg/SVGPointTearOff.h"

namespace blink {

class SVGAnimatedPathLength final : public SVGAnimatedNumber {
public:
    static PassRefPtrWillBeRawPtr<SVGAnimatedPathLength> create(SVGPathElement* contextElement)
    {
        return adoptRefWillBeNoop(new SVGAnimatedPathLength(contextElement));
    }

    void setBaseValueAsString(const String& value, SVGParsingError& parseError) override
    {
        SVGAnimatedNumber::setBaseValueAsString(value, parseError);

        ASSERT(contextElement());
        if (parseError == NoError && baseValue()->value() < 0)
            contextElement()->document().accessSVGExtensions().reportError("A negative value for path attribute <pathLength> is not allowed");
    }

private:
    explicit SVGAnimatedPathLength(SVGPathElement* contextElement)
        : SVGAnimatedNumber(contextElement, SVGNames::pathLengthAttr, SVGNumber::create())
    {
    }
};

inline SVGPathElement::SVGPathElement(Document& document)
    : SVGGeometryElement(SVGNames::pathTag, document)
    , m_pathLength(SVGAnimatedPathLength::create(this))
    , m_path(SVGAnimatedPath::create(this, SVGNames::dAttr))
{
    addToPropertyMap(m_pathLength);
    addToPropertyMap(m_path);
}

DEFINE_TRACE(SVGPathElement)
{
    visitor->trace(m_pathLength);
    visitor->trace(m_path);
    SVGGeometryElement::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGPathElement)

Path SVGPathElement::asPath() const
{
    // If this is a <use> instance, return the referenced path to maximize geometry sharing.
    if (const SVGElement* element = correspondingElement())
        return toSVGPathElement(element)->asPath();

    return m_path->currentValue()->path();
}

float SVGPathElement::getTotalLength()
{
    return getTotalLengthOfSVGPathByteStream(pathByteStream());
}

PassRefPtrWillBeRawPtr<SVGPointTearOff> SVGPathElement::getPointAtLength(float length)
{
    FloatPoint point = getPointAtLengthOfSVGPathByteStream(pathByteStream(), length);
    return SVGPointTearOff::create(SVGPoint::create(point), 0, PropertyIsNotAnimVal);
}

unsigned SVGPathElement::getPathSegAtLength(float length)
{
    return getSVGPathSegAtLengthFromSVGPathByteStream(pathByteStream(), length);
}

void SVGPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (attrName == SVGNames::dAttr || attrName == SVGNames::pathLengthAttr) {
        SVGElement::InvalidationGuard invalidationGuard(this);

        LayoutSVGShape* layoutObject = toLayoutSVGShape(this->layoutObject());

        if (attrName == SVGNames::dAttr) {
            if (layoutObject)
                layoutObject->setNeedsShapeUpdate();

            invalidateMPathDependencies();
        }

        if (layoutObject)
            markForLayoutAndParentResourceInvalidation(layoutObject);

        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

void SVGPathElement::invalidateMPathDependencies()
{
    // <mpath> can only reference <path> but this dependency is not handled in
    // markForLayoutAndParentResourceInvalidation so we update any mpath dependencies manually.
    if (SVGElementSet* dependencies = setOfIncomingReferences()) {
        for (SVGElement* element : *dependencies) {
            if (isSVGMPathElement(*element))
                toSVGMPathElement(element)->targetPathChanged();
        }
    }
}

Node::InsertionNotificationRequest SVGPathElement::insertedInto(ContainerNode* rootParent)
{
    SVGGeometryElement::insertedInto(rootParent);
    invalidateMPathDependencies();
    return InsertionDone;
}

void SVGPathElement::removedFrom(ContainerNode* rootParent)
{
    SVGGeometryElement::removedFrom(rootParent);
    invalidateMPathDependencies();
}

} // namespace blink
