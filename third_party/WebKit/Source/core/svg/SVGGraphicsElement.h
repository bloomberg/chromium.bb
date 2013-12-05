/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGGraphicsElement_h
#define SVGGraphicsElement_h

#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGTests.h"

namespace WebCore {

class AffineTransform;
class Path;

class SVGGraphicsElement : public SVGElement, public SVGTests {
public:
    virtual ~SVGGraphicsElement();

    enum StyleUpdateStrategy { AllowStyleUpdate, DisallowStyleUpdate };

    AffineTransform getCTM(StyleUpdateStrategy = AllowStyleUpdate);
    AffineTransform getScreenCTM(StyleUpdateStrategy = AllowStyleUpdate);
    AffineTransform getTransformToElement(SVGElement*, ExceptionState&);
    SVGElement* nearestViewportElement() const;
    SVGElement* farthestViewportElement() const;

    virtual AffineTransform localCoordinateSpaceTransform(SVGElement::CTMScope) const OVERRIDE { return animatedLocalTransform(); }
    virtual AffineTransform animatedLocalTransform() const;
    virtual AffineTransform* supplementalTransform();

    virtual SVGRect getBBox();
    SVGRect getStrokeBBox();

    // "base class" methods for all the elements which render as paths
    virtual void toClipPath(Path&);
    virtual RenderObject* createRenderer(RenderStyle*);

protected:
    SVGGraphicsElement(const QualifiedName&, Document&, ConstructionType = CreateSVGElement);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void svgAttributeChanged(const QualifiedName&);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGGraphicsElement)
        DECLARE_ANIMATED_TRANSFORM_LIST(Transform, transform)
    END_DECLARE_ANIMATED_PROPERTIES

private:
    virtual bool isSVGGraphicsElement() const OVERRIDE { return true; }

    // SVGTests
    virtual void synchronizeRequiredFeatures() { SVGTests::synchronizeRequiredFeatures(this); }
    virtual void synchronizeRequiredExtensions() { SVGTests::synchronizeRequiredExtensions(this); }
    virtual void synchronizeSystemLanguage() { SVGTests::synchronizeSystemLanguage(this); }

    // Used by <animateMotion>
    OwnPtr<AffineTransform> m_supplementalTransform;
};

inline bool isSVGGraphicsElement(const Node& node)
{
    return node.isSVGElement() && toSVGElement(node).isSVGGraphicsElement();
}

DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(SVGGraphicsElement);

} // namespace WebCore

#endif // SVGGraphicsElement_h
