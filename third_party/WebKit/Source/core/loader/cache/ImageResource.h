/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef ImageResource_h
#define ImageResource_h

#include "core/loader/cache/Resource.h"
#include "core/platform/graphics/ImageObserver.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/IntSizeHash.h"
#include "core/platform/graphics/LayoutSize.h"
#include "core/svg/graphics/SVGImageCache.h"
#include "wtf/HashMap.h"

namespace WebCore {

class ImageResourceClient;
class ResourceFetcher;
class FloatSize;
class MemoryCache;
class RenderObject;
struct Length;

class ImageResource : public Resource, public ImageObserver {
    friend class MemoryCache;

public:
    ImageResource(const ResourceRequest&);
    ImageResource(WebCore::Image*);
    virtual ~ImageResource();

    virtual void load(ResourceFetcher*, const ResourceLoaderOptions&);

    WebCore::Image* image(); // Returns the nullImage() if the image is not available yet.
    WebCore::Image* imageForRenderer(const RenderObject*); // Returns the nullImage() if the image is not available yet.
    bool hasImage() const { return m_image.get(); }
    bool currentFrameKnownToBeOpaque(const RenderObject*); // Side effect: ensures decoded image is in cache, therefore should only be called when about to draw the image.

    std::pair<WebCore::Image*, float> brokenImage(float deviceScaleFactor) const; // Returns an image and the image's resolution scale factor.
    bool willPaintBrokenImage() const;

    bool canRender(const RenderObject* renderer, float multiplier) { return !errorOccurred() && !imageSizeForRenderer(renderer, multiplier).isEmpty(); }

    void setContainerSizeForRenderer(const ImageResourceClient*, const IntSize&, float);
    bool usesImageContainerSize() const;
    bool imageHasRelativeWidth() const;
    bool imageHasRelativeHeight() const;

    // This method takes a zoom multiplier that can be used to increase the natural size of the image by the zoom.
    LayoutSize imageSizeForRenderer(const RenderObject*, float multiplier); // returns the size of the complete image.
    void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    virtual void didAddClient(ResourceClient*);
    virtual void didRemoveClient(ResourceClient*);

    virtual void allClientsRemoved();
    virtual void destroyDecodedData();

    virtual void appendData(const char*, int) OVERRIDE;
    virtual void error(Resource::Status);
    virtual void responseReceived(const ResourceResponse&);
    virtual void finishOnePart() OVERRIDE;

    // For compatibility, images keep loading even if there are HTTP errors.
    virtual bool shouldIgnoreHTTPStatusCodeErrors() const { return true; }

    virtual bool isImage() const { return true; }
    virtual bool stillNeedsLoad() const OVERRIDE { return !errorOccurred() && status() == Unknown && !isLoading(); }

    // ImageObserver
    virtual void decodedSizeChanged(const WebCore::Image*, int delta);
    virtual void didDraw(const WebCore::Image*);

    virtual bool shouldPauseAnimation(const WebCore::Image*);
    virtual void animationAdvanced(const WebCore::Image*);
    virtual void changedInRect(const WebCore::Image*, const IntRect&);

private:
    void clear();

    void setCustomAcceptHeader();
    void createImage();
    void updateImage(bool allDataReceived);
    void clearImage();
    // If not null, changeRect is the changed part of the image.
    void notifyObservers(const IntRect* changeRect = 0);

    virtual void switchClientsToRevalidatedResource() OVERRIDE;

    typedef pair<IntSize, float> SizeAndZoom;
    typedef HashMap<const ImageResourceClient*, SizeAndZoom> ContainerSizeRequests;
    ContainerSizeRequests m_pendingContainerSizeRequests;

    RefPtr<WebCore::Image> m_image;
    OwnPtr<SVGImageCache> m_svgImageCache;
    bool m_loadingMultipartContent;
};

}

#endif
