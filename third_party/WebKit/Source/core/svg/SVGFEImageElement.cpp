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

#include "core/svg/SVGFEImageElement.h"

#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/svg/SVGPreserveAspectRatio.h"
#include "core/svg/graphics/filters/SVGFEImage.h"
#include "platform/graphics/Image.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

inline SVGFEImageElement::SVGFEImageElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feImageTag, document),
      SVGURIReference(this),
      m_preserveAspectRatio(SVGAnimatedPreserveAspectRatio::create(
          this,
          SVGNames::preserveAspectRatioAttr)) {
  addToPropertyMap(m_preserveAspectRatio);
}

DEFINE_NODE_FACTORY(SVGFEImageElement)

SVGFEImageElement::~SVGFEImageElement() {
  clearImageResource();
}

DEFINE_TRACE(SVGFEImageElement) {
  visitor->trace(m_preserveAspectRatio);
  visitor->trace(m_cachedImage);
  visitor->trace(m_targetIdObserver);
  SVGFilterPrimitiveStandardAttributes::trace(visitor);
  SVGURIReference::trace(visitor);
}

bool SVGFEImageElement::currentFrameHasSingleSecurityOrigin() const {
  if (m_cachedImage && m_cachedImage->getImage())
    return m_cachedImage->getImage()->currentFrameHasSingleSecurityOrigin();

  return true;
}

void SVGFEImageElement::clearResourceReferences() {
  clearImageResource();
  unobserveTarget(m_targetIdObserver);
  removeAllOutgoingReferences();
}

void SVGFEImageElement::fetchImageResource() {
  FetchRequest request(ResourceRequest(document().completeURL(hrefString())),
                       localName());
  m_cachedImage = ImageResourceContent::fetch(request, document().fetcher());

  if (m_cachedImage)
    m_cachedImage->addObserver(this);
}

void SVGFEImageElement::clearImageResource() {
  if (!m_cachedImage)
    return;
  m_cachedImage->removeObserver(this);
  m_cachedImage = nullptr;
}

void SVGFEImageElement::buildPendingResource() {
  clearResourceReferences();
  if (!isConnected())
    return;

  Element* target = observeTarget(m_targetIdObserver, *this);
  if (!target) {
    if (!SVGURLReferenceResolver(hrefString(), document()).isLocal())
      fetchImageResource();
  } else if (target->isSVGElement()) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    addReferenceTo(toSVGElement(target));
  }

  invalidate();
}

void SVGFEImageElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (attrName == SVGNames::preserveAspectRatioAttr) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    invalidate();
    return;
  }

  if (SVGURIReference::isKnownAttribute(attrName)) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    buildPendingResource();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

Node::InsertionNotificationRequest SVGFEImageElement::insertedInto(
    ContainerNode* rootParent) {
  SVGFilterPrimitiveStandardAttributes::insertedInto(rootParent);
  buildPendingResource();
  return InsertionDone;
}

void SVGFEImageElement::removedFrom(ContainerNode* rootParent) {
  SVGFilterPrimitiveStandardAttributes::removedFrom(rootParent);
  if (rootParent->isConnected())
    clearResourceReferences();
}

void SVGFEImageElement::imageNotifyFinished(ImageResourceContent*) {
  if (!isConnected())
    return;

  Element* parent = parentElement();
  if (!parent || !isSVGFilterElement(parent) || !parent->layoutObject())
    return;

  if (LayoutObject* layoutObject = this->layoutObject())
    markForLayoutAndParentResourceInvalidation(layoutObject);
}

FilterEffect* SVGFEImageElement::build(SVGFilterBuilder*, Filter* filter) {
  if (m_cachedImage) {
    // Don't use the broken image icon on image loading errors.
    RefPtr<Image> image =
        m_cachedImage->errorOccurred() ? nullptr : m_cachedImage->getImage();
    return FEImage::createWithImage(filter, image,
                                    m_preserveAspectRatio->currentValue());
  }

  return FEImage::createWithIRIReference(filter, treeScope(), hrefString(),
                                         m_preserveAspectRatio->currentValue());
}

}  // namespace blink
