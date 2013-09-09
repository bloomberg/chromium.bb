/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#include "SVGNames.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedInteger.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGExternalResourcesRequired.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"

namespace WebCore {

class SVGFilterElement FINAL : public SVGElement,
                               public SVGURIReference,
                               public SVGExternalResourcesRequired {
public:
    static PassRefPtr<SVGFilterElement> create(const QualifiedName&, Document&);

    void setFilterRes(unsigned filterResX, unsigned filterResY);
    void addClient(Node*);
    void removeClient(Node*);

private:
    SVGFilterElement(const QualifiedName&, Document&);

    virtual bool needsPendingResourceHandling() const { return false; }

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual bool childShouldCreateRenderer(const Node& child) const OVERRIDE;

    virtual bool selfHasRelativeLengths() const;

    static const AtomicString& filterResXIdentifier();
    static const AtomicString& filterResYIdentifier();

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGFilterElement)
        DECLARE_ANIMATED_ENUMERATION(FilterUnits, filterUnits, SVGUnitTypes::SVGUnitType)
        DECLARE_ANIMATED_ENUMERATION(PrimitiveUnits, primitiveUnits, SVGUnitTypes::SVGUnitType)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_INTEGER(FilterResX, filterResX)
        DECLARE_ANIMATED_INTEGER(FilterResY, filterResY)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    HashSet<RefPtr<Node> > m_clientsToAdd;
};

inline SVGFilterElement* toSVGFilterElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->hasTagName(SVGNames::filterTag));
    return static_cast<SVGFilterElement*>(node);
}

}

#endif
