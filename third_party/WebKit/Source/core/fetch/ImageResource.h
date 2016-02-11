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

#include "core/CoreExport.h"
#include "core/fetch/Resource.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSizeHash.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/ImageOrientation.h"
#include "wtf/HashMap.h"

namespace blink {

class ImageResourceClient;
class FetchRequest;
class ResourceFetcher;
class FloatSize;
class Length;
class MemoryCache;
class SecurityOrigin;

class CORE_EXPORT ImageResource final : public Resource, public ImageObserver {
    friend class MemoryCache;
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ImageResource);
public:
    using ClientType = ImageResourceClient;

    static PassRefPtrWillBeRawPtr<ImageResource> fetch(FetchRequest&, ResourceFetcher*);

    static PassRefPtrWillBeRawPtr<ImageResource> create(blink::Image* image)
    {
        return adoptRefWillBeNoop(new ImageResource(image));
    }

    // Exposed for testing
    static PassRefPtrWillBeRawPtr<ImageResource> create(const ResourceRequest& request, blink::Image* image)
    {
        return adoptRefWillBeNoop(new ImageResource(request, image));
    }


    ~ImageResource() override;

    void load(ResourceFetcher*, const ResourceLoaderOptions&) override;

    blink::Image* image(); // Returns the nullImage() if the image is not available yet.
    bool hasImage() const { return m_image.get(); }

    static std::pair<blink::Image*, float> brokenImage(float deviceScaleFactor); // Returns an image and the image's resolution scale factor.
    bool willPaintBrokenImage() const;

    // Assumes that image rotation or scale doesn't effect the image size being empty or not.
    bool canRender() { return !errorOccurred() && !imageSize(DoNotRespectImageOrientation, 1).isEmpty(); }

    bool usesImageContainerSize() const;
    bool imageHasRelativeSize() const;
    // The device pixel ratio we got from the server for this image, or 1.0.
    float devicePixelRatioHeaderValue() const { return m_devicePixelRatioHeaderValue; }
    bool hasDevicePixelRatioHeaderValue() const { return m_hasDevicePixelRatioHeaderValue; }

    enum SizeType {
        IntrinsicSize, // Report the intrinsic size.
        IntrinsicCorrectedToDPR, // Report the intrinsic size corrected to account for image density.
    };
    // This method takes a zoom multiplier that can be used to increase the natural size of the image by the zoom.
    LayoutSize imageSize(RespectImageOrientationEnum shouldRespectImageOrientation, float multiplier, SizeType = IntrinsicSize);
    void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    bool isAccessAllowed(SecurityOrigin*);

    void updateImageAnimationPolicy();

    // If this ImageResource has the Lo-Fi response headers, reload it with
    // the Lo-Fi state set to off and bypassing the cache.
    void reloadIfLoFi(ResourceFetcher*);

    void didAddClient(ResourceClient*) override;
    void didRemoveClient(ResourceClient*) override;

    void allClientsRemoved() override;

    void appendData(const char*, size_t) override;
    void error(Resource::Status) override;
    void responseReceived(const ResourceResponse&, PassOwnPtr<WebDataConsumerHandle>) override;
    void finishOnePart() override;

    // For compatibility, images keep loading even if there are HTTP errors.
    bool shouldIgnoreHTTPStatusCodeErrors() const override { return true; }

    bool isImage() const override { return true; }
    bool stillNeedsLoad() const override { return !errorOccurred() && status() == Unknown && !isLoading(); }

    // ImageObserver
    void decodedSizeChanged(const blink::Image*, int delta) override;
    void didDraw(const blink::Image*) override;

    bool shouldPauseAnimation(const blink::Image*) override;
    void animationAdvanced(const blink::Image*) override;
    void changedInRect(const blink::Image*, const IntRect&) override;

    DECLARE_VIRTUAL_TRACE();

protected:
    bool isSafeToUnlock() const override;
    void destroyDecodedDataIfPossible() override;
    void destroyDecodedDataForFailedRevalidation() override;

private:
    explicit ImageResource(blink::Image*);
    ImageResource(const ResourceRequest&, blink::Image*);

    class ImageResourceFactory : public ResourceFactory {
    public:
        ImageResourceFactory()
            : ResourceFactory(Resource::Image) { }

        PassRefPtrWillBeRawPtr<Resource> create(const ResourceRequest& request, const String&) const override
        {
            return adoptRefWillBeNoop(new ImageResource(request));
        }
    };
    ImageResource(const ResourceRequest&);

    void clear();

    void setCustomAcceptHeader();
    void createImage();
    void updateImage(bool allDataReceived);
    void clearImage();
    // If not null, changeRect is the changed part of the image.
    void notifyObservers(const IntRect* changeRect = nullptr);
    bool loadingMultipartContent() const;

    float m_devicePixelRatioHeaderValue;

    RefPtr<blink::Image> m_image;
    bool m_hasDevicePixelRatioHeaderValue;
};

DEFINE_RESOURCE_TYPE_CASTS(Image);

} // namespace blink

#endif
