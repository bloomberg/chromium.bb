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

#ifndef SVGPolyElement_h
#define SVGPolyElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGPointList.h"

namespace WebCore {

class SVGPolyElement : public SVGGeometryElement
                     , public SVGExternalResourcesRequired {
public:
    SVGListPropertyTearOff<SVGPointList>* points();
    SVGListPropertyTearOff<SVGPointList>* animatedPoints();

    SVGPointList& pointsCurrentValue();

    static const SVGPropertyInfo* pointsPropertyInfo();

protected:
    SVGPolyElement(const QualifiedName&, Document&);

private:
    virtual bool isValid() const { return SVGTests::isValid(); }
    virtual bool supportsFocus() const OVERRIDE { return hasFocusEventListeners(); }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool supportsMarkers() const { return true; }

    // Custom 'points' property
    static void synchronizePoints(SVGElement* contextElement);
    static PassRefPtr<SVGAnimatedProperty> lookupOrCreatePointsWrapper(SVGElement* contextElement);

    mutable SVGSynchronizableAnimatedProperty<SVGPointList> m_points;

private:
    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGPolyElement)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

inline bool isSVGPolyElement(const Node& node)
{
    return node.hasTagName(SVGNames::polygonTag) || node.hasTagName(SVGNames::polylineTag);
}

DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(SVGPolyElement);

} // namespace WebCore

#endif
