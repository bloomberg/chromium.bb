/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
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

#include "core/svg/SVGImageElement.h"

#include "core/CSSPropertyNames.h"
#include "core/XLinkNames.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/svg/LayoutSVGImage.h"

namespace blink {

inline SVGImageElement::SVGImageElement(Document& document)
    : SVGGraphicsElement(SVGNames::imageTag, document)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(SVGLengthMode::Width), AllowNegativeLengths))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(SVGLengthMode::Height), AllowNegativeLengths))
    , m_width(SVGAnimatedLength::create(this, SVGNames::widthAttr, SVGLength::create(SVGLengthMode::Width), ForbidNegativeLengths))
    , m_height(SVGAnimatedLength::create(this, SVGNames::heightAttr, SVGLength::create(SVGLengthMode::Height), ForbidNegativeLengths))
    , m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create()))
    , m_imageLoader(SVGImageLoader::create(this))
    , m_needsLoaderURIUpdate(true)
{
    SVGURIReference::initialize(this);

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_width);
    addToPropertyMap(m_height);
    addToPropertyMap(m_preserveAspectRatio);
}

DEFINE_NODE_FACTORY(SVGImageElement)

DEFINE_TRACE(SVGImageElement)
{
    visitor->trace(m_x);
    visitor->trace(m_y);
    visitor->trace(m_width);
    visitor->trace(m_height);
    visitor->trace(m_preserveAspectRatio);
    visitor->trace(m_imageLoader);
    SVGGraphicsElement::trace(visitor);
    SVGURIReference::trace(visitor);
}

bool SVGImageElement::currentFrameHasSingleSecurityOrigin() const
{
    if (LayoutSVGImage* renderSVGImage = toLayoutSVGImage(layoutObject())) {
        if (renderSVGImage->imageResource()->hasImage()) {
            if (Image* image = renderSVGImage->imageResource()->cachedImage()->image())
                return image->currentFrameHasSingleSecurityOrigin();
        }
    }

    return true;
}

bool SVGImageElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
        supportedAttributes.add(SVGNames::preserveAspectRatioAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

bool SVGImageElement::isPresentationAttribute(const QualifiedName& attrName) const
{
    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr)
        return true;
    return SVGGraphicsElement::isPresentationAttribute(attrName);
}

bool SVGImageElement::isPresentationAttributeWithSVGDOM(const QualifiedName& attrName) const
{
    if (attrName == SVGNames::xAttr || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr)
        return true;
    return SVGGraphicsElement::isPresentationAttributeWithSVGDOM(attrName);
}

void SVGImageElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    RefPtrWillBeRawPtr<SVGAnimatedPropertyBase> property = propertyFromAttribute(name);

    if (property == m_width)
        addSVGLengthPropertyToPresentationAttributeStyle(style, CSSPropertyWidth, *m_width->currentValue());
    else if (property == m_height)
        addSVGLengthPropertyToPresentationAttributeStyle(style, CSSPropertyHeight, *m_height->currentValue());
    else if (property == m_x)
        addSVGLengthPropertyToPresentationAttributeStyle(style, CSSPropertyX, *m_x->currentValue());
    else if (property == m_y)
        addSVGLengthPropertyToPresentationAttributeStyle(style, CSSPropertyY, *m_y->currentValue());
    else
        SVGGraphicsElement::collectStyleForPresentationAttribute(name, value, style);
}

void SVGImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    bool isLengthAttribute = attrName == SVGNames::xAttr
                          || attrName == SVGNames::yAttr
                          || attrName == SVGNames::widthAttr
                          || attrName == SVGNames::heightAttr;

    if (isLengthAttribute) {
        invalidateSVGPresentationAttributeStyle();
        setNeedsStyleRecalc(LocalStyleChange,
            StyleChangeReasonForTracing::fromAttribute(attrName));
        updateRelativeLengthsInformation();
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        if (inDocument())
            imageLoader().updateFromElement(ImageLoader::UpdateIgnorePreviousError);
        else
            m_needsLoaderURIUpdate = true;
        return;
    }

    LayoutObject* renderer = this->layoutObject();
    if (!renderer)
        return;

    if (isLengthAttribute || attrName == SVGNames::preserveAspectRatioAttr) {
        // FIXME: if isLengthAttribute then we should avoid this
        // call if the viewport didn't change, however since we don't
        // have the computed style yet we can't use updateImageViewport.
        // See http://crbug.com/466200.
        markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    ASSERT_NOT_REACHED();
}

bool SVGImageElement::selfHasRelativeLengths() const
{
    return m_x->currentValue()->isRelative()
        || m_y->currentValue()->isRelative()
        || m_width->currentValue()->isRelative()
        || m_height->currentValue()->isRelative();
}

LayoutObject* SVGImageElement::createLayoutObject(const LayoutStyle&)
{
    return new LayoutSVGImage(this);
}

bool SVGImageElement::haveLoadedRequiredResources()
{
    return !m_needsLoaderURIUpdate && !imageLoader().hasPendingActivity();
}

void SVGImageElement::attach(const AttachContext& context)
{
    SVGGraphicsElement::attach(context);

    if (LayoutSVGImage* imageObj = toLayoutSVGImage(layoutObject())) {
        if (imageObj->imageResource()->hasImage())
            return;

        imageObj->imageResource()->setImageResource(imageLoader().image());
    }
}

Node::InsertionNotificationRequest SVGImageElement::insertedInto(ContainerNode* rootParent)
{
    SVGGraphicsElement::insertedInto(rootParent);
    if (!rootParent->inDocument())
        return InsertionDone;

    // We can only resolve base URIs properly after tree insertion - hence, URI mutations while
    // detached are deferred until this point.
    if (m_needsLoaderURIUpdate) {
        imageLoader().updateFromElement(ImageLoader::UpdateIgnorePreviousError);
        m_needsLoaderURIUpdate = false;
    } else {
        // A previous loader update may have failed to actually fetch the image if the document
        // was inactive. In that case, force a re-update (but don't clear previous errors).
        if (!imageLoader().image())
            imageLoader().updateFromElement();
    }

    return InsertionDone;
}

const AtomicString SVGImageElement::imageSourceURL() const
{
    return AtomicString(hrefString());
}

void SVGImageElement::didMoveToNewDocument(Document& oldDocument)
{
    imageLoader().elementDidMoveToNewDocument();
    SVGGraphicsElement::didMoveToNewDocument(oldDocument);
}

} // namespace blink
