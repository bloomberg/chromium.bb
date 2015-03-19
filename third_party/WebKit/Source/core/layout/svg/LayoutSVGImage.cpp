/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "core/layout/svg/LayoutSVGImage.h"

#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutImageResource.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGImagePainter.h"
#include "core/svg/SVGImageElement.h"
#include "platform/LengthFunctions.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

LayoutSVGImage::LayoutSVGImage(SVGImageElement* impl)
    : LayoutSVGModelObject(impl)
    , m_needsBoundariesUpdate(true)
    , m_needsTransformUpdate(true)
    , m_imageResource(LayoutImageResource::create())
{
    m_imageResource->initialize(this);
}

LayoutSVGImage::~LayoutSVGImage()
{
}

void LayoutSVGImage::destroy()
{
    ImageQualityController::remove(this);
    m_imageResource->shutdown();
    LayoutSVGModelObject::destroy();
}

FloatSize LayoutSVGImage::computeImageViewportSize(ImageResource& cachedImage) const
{
    if (toSVGImageElement(element())->preserveAspectRatio()->currentValue()->align() != SVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE)
        return m_objectBoundingBox.size();

    // Images with preserveAspectRatio=none should force non-uniform
    // scaling. This can be achieved by setting the image's container size to
    // its viewport size (i.e. if a viewBox is available - use that - else use intrinsic size.)
    // See: http://www.w3.org/TR/SVG/single-page.html, 7.8 The 'preserveAspectRatio' attribute.
    Length intrinsicWidth;
    Length intrinsicHeight;
    FloatSize intrinsicRatio;
    cachedImage.computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
    return intrinsicRatio;
}

static bool containerSizeIsSetForLayoutObject(ImageResource& cachedImage, const LayoutObject* layoutObject)
{
    const Image* image = cachedImage.image();
    // If a container size has been specified for this layoutObject, then
    // imageForLayoutObject() will return the SVGImageForContainer while image()
    // will return the underlying SVGImage.
    return !image->isSVGImage() || image != cachedImage.imageForLayoutObject(layoutObject);
}

bool LayoutSVGImage::updateImageViewport()
{
    SVGImageElement* image = toSVGImageElement(element());
    FloatRect oldBoundaries = m_objectBoundingBox;

    SVGLengthContext lengthContext(image);
    m_objectBoundingBox = FloatRect(
        lengthContext.valueForLength(styleRef().svgStyle().x(), styleRef(), SVGLengthMode::Width),
        lengthContext.valueForLength(styleRef().svgStyle().y(), styleRef(), SVGLengthMode::Height),
        lengthContext.valueForLength(styleRef().width(), styleRef(), SVGLengthMode::Width),
        lengthContext.valueForLength(styleRef().height(), styleRef(), SVGLengthMode::Height));
    bool boundsChanged = oldBoundaries != m_objectBoundingBox;

    bool updatedViewport = false;
    ImageResource* cachedImage = m_imageResource->cachedImage();
    if (cachedImage && cachedImage->usesImageContainerSize()) {
        FloatSize imageViewportSize = computeImageViewportSize(*cachedImage);
        if (LayoutSize(imageViewportSize) != m_imageResource->imageSize(styleRef().effectiveZoom())
            || !containerSizeIsSetForLayoutObject(*cachedImage, this)) {
            m_imageResource->setContainerSizeForLayoutObject(roundedIntSize(imageViewportSize));
            updatedViewport = true;
        }
    }

    m_needsBoundariesUpdate |= boundsChanged;
    return updatedViewport || boundsChanged;
}

void LayoutSVGImage::layout()
{
    ASSERT(needsLayout());

    updateImageViewport();

    bool transformOrBoundariesUpdate = m_needsTransformUpdate || m_needsBoundariesUpdate;
    if (m_needsTransformUpdate) {
        m_localTransform = toSVGImageElement(element())->calculateAnimatedLocalTransform();
        m_needsTransformUpdate = false;
    }

    if (m_needsBoundariesUpdate) {
        m_bufferedForeground.clear();

        m_paintInvalidationBoundingBox = m_objectBoundingBox;
        SVGLayoutSupport::intersectPaintInvalidationRectWithResources(this, m_paintInvalidationBoundingBox);

        m_needsBoundariesUpdate = false;
    }

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // If our bounds changed, notify the parents.
    if (transformOrBoundariesUpdate)
        LayoutSVGModelObject::setNeedsBoundariesUpdate();

    clearNeedsLayout();
}

void LayoutSVGImage::paint(const PaintInfo& paintInfo, const LayoutPoint&)
{
    SVGImagePainter(*this).paint(paintInfo);
}

bool LayoutSVGImage::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_IMAGE_HITTESTING, request, style()->pointerEvents());
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        FloatPoint localPoint;
        if (!SVGLayoutSupport::transformToUserSpaceAndCheckClipping(this, localToParentTransform(), pointInParent, localPoint))
            return false;

        if (hitRules.canHitFill || hitRules.canHitBoundingBox) {
            if (m_objectBoundingBox.contains(localPoint)) {
                updateHitTestResult(result, roundedLayoutPoint(localPoint));
                return true;
            }
        }
    }

    return false;
}

void LayoutSVGImage::imageChanged(WrappedImagePtr, const IntRect*)
{
    // The image resource defaults to nullImage until the resource arrives.
    // This empty image may be cached by SVG resources which must be invalidated.
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(this))
        resources->removeClientFromCache(this);

    // Eventually notify parent resources, that we've changed.
    LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(this, false);

    // Update the SVGImageCache sizeAndScales entry in case image loading finished after layout.
    // (https://bugs.webkit.org/show_bug.cgi?id=99489)
    m_objectBoundingBox = FloatRect();
    updateImageViewport();

    m_bufferedForeground.clear();

    setShouldDoFullPaintInvalidation();
}

void LayoutSVGImage::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&) const
{
    // this is called from paint() after the localTransform has already been applied
    LayoutRect contentRect = LayoutRect(paintInvalidationRectInLocalCoordinates());
    if (!contentRect.isEmpty())
        rects.append(contentRect);
}

} // namespace blink
