/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#include "core/svg/SVGGraphicsElement.h"

#include "core/SVGNames.h"
#include "core/rendering/svg/RenderSVGPath.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/SVGPathData.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tagName, Document& document, ConstructionType constructionType)
    : SVGElement(tagName, document, constructionType)
    , SVGTests(this)
    , m_transform(SVGAnimatedTransformList::create(this, SVGNames::transformAttr, SVGTransformList::create()))
{
    addToPropertyMap(m_transform);
}

SVGGraphicsElement::~SVGGraphicsElement()
{
}

bool SVGGraphicsElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == SVGNames::transformAttr)
        return true;
    return SVGElement::isPresentationAttribute(name);
}

void SVGGraphicsElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == SVGNames::transformAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyTransform, value);
    else
        SVGElement::collectStyleForPresentationAttribute(name, value, style);
}

PassRefPtr<SVGMatrixTearOff> SVGGraphicsElement::getTransformToElement(SVGElement* target, ExceptionState& exceptionState)
{
    AffineTransform ctm = getCTM(AllowStyleUpdate);

    if (target && target->isSVGGraphicsElement()) {
        AffineTransform targetCTM = toSVGGraphicsElement(target)->getCTM(AllowStyleUpdate);
        if (!targetCTM.isInvertible()) {
            exceptionState.throwDOMException(InvalidStateError, "The target transformation is not invertable.");
            return nullptr;
        }
        ctm = targetCTM.inverse() * ctm;
    }

    return SVGMatrixTearOff::create(ctm);
}

static bool isViewportElement(const Element& element)
{
    return (isSVGSVGElement(element)
        || isSVGSymbolElement(element)
        || isSVGForeignObjectElement(element)
        || isSVGImageElement(element));
}

AffineTransform SVGGraphicsElement::computeCTM(SVGElement::CTMScope mode,
    SVGGraphicsElement::StyleUpdateStrategy styleUpdateStrategy, const SVGGraphicsElement* ancestor) const
{
    if (styleUpdateStrategy == AllowStyleUpdate)
        document().updateLayoutIgnorePendingStylesheets();

    AffineTransform ctm;
    bool done = false;

    for (const Element* currentElement = this; currentElement && !done;
        currentElement = currentElement->parentOrShadowHostElement()) {
        if (!currentElement->isSVGElement())
            break;

        ctm = toSVGElement(currentElement)->localCoordinateSpaceTransform(mode).multiply(ctm);

        switch (mode) {
        case NearestViewportScope:
            // Stop at the nearest viewport ancestor.
            done = currentElement != this && isViewportElement(*currentElement);
            break;
        case AncestorScope:
            // Stop at the designated ancestor.
            done = currentElement == ancestor;
            break;
        default:
            ASSERT(mode == ScreenScope);
            break;
        }
    }

    return ctm;
}

AffineTransform SVGGraphicsElement::getCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeCTM(NearestViewportScope, styleUpdateStrategy);
}

AffineTransform SVGGraphicsElement::getScreenCTM(StyleUpdateStrategy styleUpdateStrategy)
{
    return computeCTM(ScreenScope, styleUpdateStrategy);
}

PassRefPtr<SVGMatrixTearOff> SVGGraphicsElement::getCTMFromJavascript()
{
    return SVGMatrixTearOff::create(getCTM());
}

PassRefPtr<SVGMatrixTearOff> SVGGraphicsElement::getScreenCTMFromJavascript()
{
    return SVGMatrixTearOff::create(getScreenCTM());
}

AffineTransform SVGGraphicsElement::animatedLocalTransform() const
{
    AffineTransform matrix;
    // If CSS property was set, use that, otherwise fallback to attribute (if set).
    if (!getStyleTransform(matrix))
        m_transform->currentValue()->concatenate(matrix);

    if (m_supplementalTransform)
        return *m_supplementalTransform * matrix;
    return matrix;
}

AffineTransform* SVGGraphicsElement::supplementalTransform()
{
    if (!m_supplementalTransform)
        m_supplementalTransform = adoptPtr(new AffineTransform);
    return m_supplementalTransform.get();
}

bool SVGGraphicsElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGTests::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::transformAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGGraphicsElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    parseAttributeNew(name, value);
}

void SVGGraphicsElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    // Reattach so the isValid() check will be run again during renderer creation.
    if (SVGTests::isKnownAttribute(attrName)) {
        lazyReattachIfAttached();
        return;
    }

    RenderObject* object = renderer();
    if (!object)
        return;

    if (attrName == SVGNames::transformAttr) {
        invalidateSVGPresentationAttributeStyle();
        setNeedsStyleRecalc(LocalStyleChange);
        object->setNeedsTransformUpdate();
        RenderSVGResource::markForLayoutAndParentResourceInvalidation(object);
        return;
    }

    ASSERT_NOT_REACHED();
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const
{
    for (Element* current = parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        if (isViewportElement(*current))
            return toSVGElement(current);
    }

    return 0;
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const
{
    SVGElement* farthest = 0;
    for (Element* current = parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        if (isViewportElement(*current))
            farthest = toSVGElement(current);
    }
    return farthest;
}

FloatRect SVGGraphicsElement::getBBox()
{
    document().updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support getBBox for detached elements.
    if (!renderer())
        return FloatRect();

    return renderer()->objectBoundingBox();
}

PassRefPtr<SVGRectTearOff> SVGGraphicsElement::getBBoxFromJavascript()
{
    return SVGRectTearOff::create(SVGRect::create(getBBox()), 0, PropertyIsNotAnimVal);
}

RenderObject* SVGGraphicsElement::createRenderer(RenderStyle*)
{
    // By default, any subclass is expected to do path-based drawing
    return new RenderSVGPath(this);
}

void SVGGraphicsElement::toClipPath(Path& path)
{
    updatePathFromGraphicsElement(this, path);
    // FIXME: How do we know the element has done a layout?
    path.transform(animatedLocalTransform());
}

}
