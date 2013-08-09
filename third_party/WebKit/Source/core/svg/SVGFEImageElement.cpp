/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
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

#include "core/svg/SVGFEImageElement.h"

#include "SVGNames.h"
#include "XLinkNames.h"
#include "core/dom/Document.h"
#include "core/loader/cache/FetchRequest.h"
#include "core/loader/cache/ImageResource.h"
#include "core/loader/cache/ResourceFetcher.h"
#include "core/platform/graphics/Image.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGPreserveAspectRatio.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_PRESERVEASPECTRATIO(SVGFEImageElement, SVGNames::preserveAspectRatioAttr, PreserveAspectRatio, preserveAspectRatio)
DEFINE_ANIMATED_STRING(SVGFEImageElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGFEImageElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGFEImageElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(preserveAspectRatio)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGFilterPrimitiveStandardAttributes)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGFEImageElement::SVGFEImageElement(const QualifiedName& tagName, Document* document)
    : SVGFilterPrimitiveStandardAttributes(tagName, document)
{
    ASSERT(hasTagName(SVGNames::feImageTag));
    ScriptWrappable::init(this);
    registerAnimatedPropertiesForSVGFEImageElement();
}

PassRefPtr<SVGFEImageElement> SVGFEImageElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new SVGFEImageElement(tagName, document));
}

SVGFEImageElement::~SVGFEImageElement()
{
    clearResourceReferences();
}

void SVGFEImageElement::clearResourceReferences()
{
    if (m_cachedImage) {
        m_cachedImage->removeClient(this);
        m_cachedImage = 0;
    }

    ASSERT(document());
    document()->accessSVGExtensions()->removeAllTargetReferencesForElement(this);
}

void SVGFEImageElement::requestImageResource()
{
    FetchRequest request(ResourceRequest(ownerDocument()->completeURL(hrefCurrentValue())), localName());
    m_cachedImage = document()->fetcher()->requestImage(request);

    if (m_cachedImage)
        m_cachedImage->addClient(this);
}

void SVGFEImageElement::buildPendingResource()
{
    clearResourceReferences();
    if (!inDocument())
        return;

    String id;
    Element* target = SVGURIReference::targetElementFromIRIString(hrefCurrentValue(), document(), &id);
    if (!target) {
        if (id.isEmpty())
            requestImageResource();
        else {
            document()->accessSVGExtensions()->addPendingResource(id, this);
            ASSERT(hasPendingResources());
        }
    } else if (target->isSVGElement()) {
        // Register us with the target in the dependencies map. Any change of hrefElement
        // that leads to relayout/repainting now informs us, so we can react to it.
        document()->accessSVGExtensions()->addElementReferencingTarget(this, toSVGElement(target));
    }

    invalidate();
}

bool SVGFEImageElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::preserveAspectRatioAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGFEImageElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGFilterPrimitiveStandardAttributes::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::preserveAspectRatioAttr) {
        SVGPreserveAspectRatio preserveAspectRatio;
        preserveAspectRatio.parse(value);
        setPreserveAspectRatioBaseValue(preserveAspectRatio);
        return;
    }

    if (SVGURIReference::parseAttribute(name, value))
        return;
    if (SVGExternalResourcesRequired::parseAttribute(name, value))
        return;

    ASSERT_NOT_REACHED();
}

void SVGFEImageElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    if (attrName == SVGNames::preserveAspectRatioAttr) {
        invalidate();
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        buildPendingResource();
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        return;

    ASSERT_NOT_REACHED();
}

Node::InsertionNotificationRequest SVGFEImageElement::insertedInto(ContainerNode* rootParent)
{
    SVGFilterPrimitiveStandardAttributes::insertedInto(rootParent);
    buildPendingResource();
    return InsertionDone;
}

void SVGFEImageElement::removedFrom(ContainerNode* rootParent)
{
    SVGFilterPrimitiveStandardAttributes::removedFrom(rootParent);
    if (rootParent->inDocument())
        clearResourceReferences();
}

void SVGFEImageElement::notifyFinished(Resource*)
{
    if (!inDocument())
        return;

    Element* parent = parentElement();
    ASSERT(parent);

    if (!parent->hasTagName(SVGNames::filterTag) || !parent->renderer())
        return;

    RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer());
}

PassRefPtr<FilterEffect> SVGFEImageElement::build(SVGFilterBuilder*, Filter* filter)
{
    if (m_cachedImage)
        return FEImage::createWithImage(filter, m_cachedImage->imageForRenderer(renderer()), preserveAspectRatioCurrentValue());
    return FEImage::createWithIRIReference(filter, document(), hrefCurrentValue(), preserveAspectRatioCurrentValue());
}

void SVGFEImageElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    SVGFilterPrimitiveStandardAttributes::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(hrefCurrentValue()));
}

}
