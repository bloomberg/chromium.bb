/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "core/style/StyleFetchedImage.h"

#include "core/css/CSSImageValue.h"
#include "core/fetch/ImageResource.h"
#include "core/layout/LayoutObject.h"
#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"

namespace blink {

StyleFetchedImage::StyleFetchedImage(ImageResource* image, Document* document, const KURL& url)
    : m_image(image)
    , m_document(document)
    , m_url(url)
{
    m_isImageResource = true;
    m_image->addClient(this);
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(this);
#endif
}

StyleFetchedImage::~StyleFetchedImage()
{
#if !ENABLE(OILPAN)
    dispose();
#endif
}

void StyleFetchedImage::dispose()
{
    m_image->removeClient(this);
}

WrappedImagePtr StyleFetchedImage::data() const
{
    return m_image.get();
}

ImageResource* StyleFetchedImage::cachedImage() const
{
    return m_image.get();
}

PassRefPtrWillBeRawPtr<CSSValue> StyleFetchedImage::cssValue() const
{
    return CSSImageValue::create(m_image->url(), const_cast<StyleFetchedImage*>(this));
}

PassRefPtrWillBeRawPtr<CSSValue> StyleFetchedImage::computedCSSValue() const
{
    return cssValue();
}

bool StyleFetchedImage::canRender() const
{
    return m_image->canRender();
}

bool StyleFetchedImage::isLoaded() const
{
    return m_image->isLoaded();
}

bool StyleFetchedImage::errorOccurred() const
{
    return m_image->errorOccurred();
}

LayoutSize StyleFetchedImage::imageSize(const LayoutObject* layoutObject, float multiplier) const
{
    return m_image->imageSize(LayoutObject::shouldRespectImageOrientation(layoutObject), multiplier);
}

bool StyleFetchedImage::imageHasRelativeWidth() const
{
    return m_image->imageHasRelativeWidth();
}

bool StyleFetchedImage::imageHasRelativeHeight() const
{
    return m_image->imageHasRelativeHeight();
}

void StyleFetchedImage::computeIntrinsicDimensions(const LayoutObject*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    m_image->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

bool StyleFetchedImage::usesImageContainerSize() const
{
    return m_image->usesImageContainerSize();
}

void StyleFetchedImage::addClient(LayoutObject* layoutObject)
{
    m_image->addClient(layoutObject);
}

void StyleFetchedImage::removeClient(LayoutObject* layoutObject)
{
    m_image->removeClient(layoutObject);
}

void StyleFetchedImage::notifyFinished(Resource* resource)
{
    if (m_document && m_image && m_image->image() && m_image->image()->isSVGImage())
        toSVGImage(m_image->image())->updateUseCounters(*m_document);
    // Oilpan: do not prolong the Document's lifetime.
    m_document.clear();
}

PassRefPtr<Image> StyleFetchedImage::image(const LayoutObject*, const IntSize& containerSize, float zoom) const
{
    if (!m_image->image()->isSVGImage())
        return m_image->image();

    return SVGImageForContainer::create(toSVGImage(m_image->image()), containerSize, zoom, m_url);
}

bool StyleFetchedImage::knownToBeOpaque(const LayoutObject* layoutObject) const
{
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage", "data", InspectorPaintImageEvent::data(layoutObject, *m_image.get()));
    return m_image->image()->currentFrameKnownToBeOpaque(Image::PreCacheMetadata);
}

DEFINE_TRACE(StyleFetchedImage)
{
    visitor->trace(m_document);
    StyleImage::trace(visitor);
}

} // namespace blink
