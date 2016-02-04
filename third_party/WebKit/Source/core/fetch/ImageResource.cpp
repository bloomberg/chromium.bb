/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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

#include "core/fetch/ImageResource.h"

#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceClientWalker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/BitmapImage.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"

namespace blink {

ResourcePtr<ImageResource> ImageResource::fetch(FetchRequest& request, ResourceFetcher* fetcher)
{
    if (request.resourceRequest().requestContext() == WebURLRequest::RequestContextUnspecified)
        request.mutableResourceRequest().setRequestContext(WebURLRequest::RequestContextImage);
    if (fetcher->context().pageDismissalEventBeingDispatched()) {
        KURL requestURL = request.resourceRequest().url();
        if (requestURL.isValid() && fetcher->context().canRequest(Resource::Image, request.resourceRequest(), requestURL, request.options(), request.forPreload(), request.originRestriction()))
            fetcher->context().sendImagePing(requestURL);
        return 0;
    }

    if (fetcher->clientDefersImage(request.resourceRequest().url()))
        request.setDefer(FetchRequest::DeferredByClient);

    return toImageResource(fetcher->requestResource(request, ImageResourceFactory()));
}

ImageResource::ImageResource(const ResourceRequest& resourceRequest)
    : Resource(resourceRequest, Image)
    , m_devicePixelRatioHeaderValue(1.0)
    , m_image(nullptr)
    , m_hasDevicePixelRatioHeaderValue(false)
{
    WTF_LOG(Timers, "new ImageResource(ResourceRequest) %p", this);
    setStatus(Unknown);
    setCustomAcceptHeader();
}

ImageResource::ImageResource(blink::Image* image)
    : Resource(ResourceRequest(""), Image)
    , m_devicePixelRatioHeaderValue(1.0)
    , m_image(image)
    , m_hasDevicePixelRatioHeaderValue(false)
{
    WTF_LOG(Timers, "new ImageResource(Image) %p", this);
    setStatus(Cached);
    setLoading(false);
    setCustomAcceptHeader();
}

ImageResource::ImageResource(const ResourceRequest& resourceRequest, blink::Image* image)
    : Resource(resourceRequest, Image)
    , m_image(image)
{
    WTF_LOG(Timers, "new ImageResource(ResourceRequest, Image) %p", this);
    setStatus(Cached);
    setLoading(false);
    setCustomAcceptHeader();
}

ImageResource::~ImageResource()
{
    WTF_LOG(Timers, "~ImageResource %p", this);
    clearImage();
}

void ImageResource::load(ResourceFetcher* fetcher, const ResourceLoaderOptions& options)
{
    if (!fetcher || fetcher->autoLoadImages())
        Resource::load(fetcher, options);
    else
        setLoading(false);
}

void ImageResource::didAddClient(ResourceClient* c)
{
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data, true);
    }

    ASSERT(c->resourceClientType() == ImageResourceClient::expectedType());
    if (m_image && !m_image->isNull())
        static_cast<ImageResourceClient*>(c)->imageChanged(this);

    Resource::didAddClient(c);
}

void ImageResource::didRemoveClient(ResourceClient* c)
{
    ASSERT(c);
    ASSERT(c->resourceClientType() == ImageResourceClient::expectedType());

    Resource::didRemoveClient(c);
}

bool ImageResource::isSafeToUnlock() const
{
    // Note that |m_image| holds a reference to |m_data| in addition to the one held by the Resource parent class.
    return !m_image || (m_image->hasOneRef() && m_data->refCount() == 2);
}

void ImageResource::destroyDecodedDataForFailedRevalidation()
{
    m_image = nullptr;
    setDecodedSize(0);
}

void ImageResource::destroyDecodedDataIfPossible()
{
    if (!hasClients() && !isLoading() && (!m_image || (m_image->hasOneRef() && m_image->isBitmapImage()))) {
        m_image = nullptr;
        setDecodedSize(0);
    } else if (m_image && !errorOccurred()) {
        m_image->destroyDecodedData(true);
    }
}

void ImageResource::allClientsRemoved()
{
    if (m_image && !errorOccurred())
        m_image->resetAnimation();
    Resource::allClientsRemoved();
}

std::pair<blink::Image*, float> ImageResource::brokenImage(float deviceScaleFactor)
{
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(blink::Image, brokenImageHiRes, (blink::Image::loadPlatformResource("missingImage@2x")));
        return std::make_pair(brokenImageHiRes, 2);
    }

    DEFINE_STATIC_REF(blink::Image, brokenImageLoRes, (blink::Image::loadPlatformResource("missingImage")));
    return std::make_pair(brokenImageLoRes, 1);
}

bool ImageResource::willPaintBrokenImage() const
{
    return errorOccurred();
}

blink::Image* ImageResource::image()
{
    ASSERT(!isPurgeable());

    if (errorOccurred()) {
        // Returning the 1x broken image is non-ideal, but we cannot reliably access the appropriate
        // deviceScaleFactor from here. It is critical that callers use ImageResource::brokenImage()
        // when they need the real, deviceScaleFactor-appropriate broken image icon.
        return brokenImage(1).first;
    }

    if (m_image)
        return m_image.get();

    return blink::Image::nullImage();
}

bool ImageResource::usesImageContainerSize() const
{
    if (m_image)
        return m_image->usesContainerSize();

    return false;
}

bool ImageResource::imageHasRelativeSize() const
{
    if (m_image)
        return m_image->hasRelativeSize();

    return false;
}

LayoutSize ImageResource::imageSize(RespectImageOrientationEnum shouldRespectImageOrientation, float multiplier, SizeType sizeType)
{
    ASSERT(!isPurgeable());

    if (!m_image)
        return LayoutSize();

    LayoutSize size;

    if (m_image->isBitmapImage() && shouldRespectImageOrientation == RespectImageOrientation)
        size = LayoutSize(toBitmapImage(m_image.get())->sizeRespectingOrientation());
    else
        size = LayoutSize(m_image->size());

    if (sizeType == IntrinsicCorrectedToDPR && m_hasDevicePixelRatioHeaderValue && m_devicePixelRatioHeaderValue > 0)
        multiplier = 1 / m_devicePixelRatioHeaderValue;

    if (multiplier == 1 || m_image->hasRelativeSize())
        return size;

    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    LayoutSize minimumSize(size.width() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit(),
        LayoutUnit(size.height() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit()));
    size.scale(multiplier);
    size.clampToMinimumSize(minimumSize);
    return size;
}

void ImageResource::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (m_image)
        m_image->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

void ImageResource::notifyObservers(const IntRect* changeRect)
{
    ResourceClientWalker<ImageResourceClient> w(m_clients);
    while (ImageResourceClient* c = w.next())
        c->imageChanged(this, changeRect);

    ResourceClientWalker<ImageResourceClient> w2(m_finishedClients);
    while (ImageResourceClient* c = w2.next())
        c->imageChanged(this, changeRect);
}

void ImageResource::clear()
{
    prune();
    clearImage();
    setEncodedSize(0);
}

void ImageResource::setCustomAcceptHeader()
{
    DEFINE_STATIC_LOCAL(const AtomicString, acceptImages, ("image/webp,image/*,*/*;q=0.8", AtomicString::ConstructFromLiteral));
    setAccept(acceptImages);
}

inline void ImageResource::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;

    if (m_response.mimeType() == "image/svg+xml") {
        m_image = SVGImage::create(this);
    } else {
        m_image = BitmapImage::create(this);
    }
}

inline void ImageResource::clearImage()
{
    // If our Image has an observer, it's always us so we need to clear the back pointer
    // before dropping our reference.
    if (m_image)
        m_image->setImageObserver(nullptr);
    m_image.clear();
}

void ImageResource::appendData(const char* data, size_t length)
{
    Resource::appendData(data, length);
    if (!loadingMultipartContent())
        updateImage(false);
}

void ImageResource::updateImage(bool allDataReceived)
{
    TRACE_EVENT0("blink", "ImageResource::updateImage");

    if (m_data)
        createImage();

    bool sizeAvailable = false;

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until
    // queried for info (like size or specific image frames).
    if (m_image)
        sizeAvailable = m_image->setData(m_data, allDataReceived);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known. Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || allDataReceived) {
        if (!m_image || m_image->isNull()) {
            error(errorOccurred() ? status() : DecodeError);
            if (memoryCache()->contains(this))
                memoryCache()->remove(this);
            return;
        }

        // It would be nice to only redraw the decoded band of the image, but with the current design
        // (decoding delayed until painting) that seems hard.
        notifyObservers();
    }
}

void ImageResource::finishOnePart()
{
    if (loadingMultipartContent())
        clear();
    updateImage(true);
    if (loadingMultipartContent())
        m_data.clear();
    Resource::finishOnePart();
}

void ImageResource::error(Resource::Status status)
{
    clear();
    Resource::error(status);
    notifyObservers();
}

void ImageResource::responseReceived(const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    if (loadingMultipartContent() && m_data)
        finishOnePart();
    Resource::responseReceived(response, handle);
    if (RuntimeEnabledFeatures::clientHintsEnabled()) {
        m_devicePixelRatioHeaderValue = m_response.httpHeaderField(HTTPNames::Content_DPR).toFloat(&m_hasDevicePixelRatioHeaderValue);
        if (!m_hasDevicePixelRatioHeaderValue || m_devicePixelRatioHeaderValue <= 0.0) {
            m_devicePixelRatioHeaderValue = 1.0;
            m_hasDevicePixelRatioHeaderValue = false;
        }

    }
}

void ImageResource::decodedSizeChanged(const blink::Image* image, int delta)
{
    if (!image || image != m_image)
        return;

    // TODO(bsep): Crash on underflow, which is possible if an error causes
    // decodedSize to be 0.
    setDecodedSize(decodedSize() + delta);
}

void ImageResource::didDraw(const blink::Image* image)
{
    if (!image || image != m_image)
        return;
    // decodedSize() == 0 indicates that the image is decoded into DiscardableMemory,
    // not in MemoryCache. So we don't need to call Resource::didAccessDecodedData()
    // to update MemoryCache.
    if (decodedSize() != 0)
        Resource::didAccessDecodedData();
}

bool ImageResource::shouldPauseAnimation(const blink::Image* image)
{
    if (!image || image != m_image)
        return false;

    ResourceClientWalker<ImageResourceClient> w(m_clients);
    while (ImageResourceClient* c = w.next()) {
        if (c->willRenderImage(this))
            return false;
    }

    ResourceClientWalker<ImageResourceClient> w2(m_finishedClients);
    while (ImageResourceClient* c = w2.next()) {
        if (c->willRenderImage(this))
            return false;
    }

    return true;
}

void ImageResource::animationAdvanced(const blink::Image* image)
{
    if (!image || image != m_image)
        return;
    notifyObservers();
}

void ImageResource::updateImageAnimationPolicy()
{
    if (!m_image)
        return;

    ImageAnimationPolicy newPolicy = ImageAnimationPolicyAllowed;
    ResourceClientWalker<ImageResourceClient> w(m_clients);
    while (ImageResourceClient* c = w.next()) {
        if (c->getImageAnimationPolicy(this, newPolicy))
            break;
    }

    ResourceClientWalker<ImageResourceClient> w2(m_finishedClients);
    while (ImageResourceClient* c = w2.next()) {
        if (c->getImageAnimationPolicy(this, newPolicy))
            break;
    }

    if (m_image->animationPolicy() != newPolicy) {
        m_image->resetAnimation();
        m_image->setAnimationPolicy(newPolicy);
    }
}

void ImageResource::reloadIfLoFi(ResourceFetcher* fetcher)
{
    if (!m_response.httpHeaderField("chrome-proxy").contains("q=low"))
        return;
    m_resourceRequest.setCachePolicy(ResourceRequestCachePolicy::ReloadBypassingCache);
    m_resourceRequest.setLoFiState(WebURLRequest::LoFiOff);
    error(Resource::LoadError);
    load(fetcher, fetcher->defaultResourceOptions());
}

void ImageResource::changedInRect(const blink::Image* image, const IntRect& rect)
{
    if (!image || image != m_image)
        return;
    notifyObservers(&rect);
}

bool ImageResource::isAccessAllowed(SecurityOrigin* securityOrigin)
{
    if (response().wasFetchedViaServiceWorker())
        return response().serviceWorkerResponseType() != WebServiceWorkerResponseTypeOpaque;
    if (!image()->currentFrameHasSingleSecurityOrigin())
        return false;
    if (passesAccessControlCheck(securityOrigin))
        return true;
    return !securityOrigin->taintsCanvas(response().url());
}

bool ImageResource::loadingMultipartContent() const
{
    return m_loader && m_loader->loadingMultipartContent();
}

} // namespace blink
