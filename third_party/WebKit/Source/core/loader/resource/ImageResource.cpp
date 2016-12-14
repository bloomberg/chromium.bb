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

#include "core/loader/resource/ImageResource.h"

#include "core/fetch/MemoryCache.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/ResourceLoadingLog.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceInfo.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/StdLibExtras.h"
#include <memory>
#include <v8.h>

namespace blink {
namespace {
// The amount of time to wait before informing the clients that the image has
// been updated (in seconds). This effectively throttles invalidations that
// result from new data arriving for this image.
constexpr double kFlushDelaySeconds = 1.;
}  // namespace

class ImageResource::ImageResourceInfoImpl final
    : public GarbageCollectedFinalized<ImageResourceInfoImpl>,
      public ImageResourceInfo {
  USING_GARBAGE_COLLECTED_MIXIN(ImageResourceInfoImpl);

 public:
  ImageResourceInfoImpl(ImageResource* resource) : m_resource(resource) {
    DCHECK(m_resource);
  }
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_resource);
    ImageResourceInfo::trace(visitor);
  }

 private:
  const KURL& url() const override { return m_resource->url(); }
  bool isSchedulingReload() const override {
    return m_resource->m_isSchedulingReload;
  }
  bool hasDevicePixelRatioHeaderValue() const override {
    return m_resource->m_hasDevicePixelRatioHeaderValue;
  }
  float devicePixelRatioHeaderValue() const override {
    return m_resource->m_devicePixelRatioHeaderValue;
  }
  const ResourceResponse& response() const override {
    return m_resource->response();
  }
  Resource::Status getStatus() const override {
    return m_resource->getStatus();
  }
  bool isPlaceholder() const override { return m_resource->isPlaceholder(); }
  bool isCacheValidator() const override {
    return m_resource->isCacheValidator();
  }
  bool schedulingReloadOrShouldReloadBrokenPlaceholder() const override {
    return m_resource->m_isSchedulingReload ||
           m_resource->shouldReloadBrokenPlaceholder();
  }
  bool isAccessAllowed(
      SecurityOrigin* securityOrigin,
      DoesCurrentFrameHaveSingleSecurityOrigin
          doesCurrentFrameHasSingleSecurityOrigin) const override {
    return m_resource->isAccessAllowed(securityOrigin,
                                       doesCurrentFrameHasSingleSecurityOrigin);
  }
  bool hasCacheControlNoStoreHeader() const override {
    return m_resource->hasCacheControlNoStoreHeader();
  }
  const ResourceError& resourceError() const override {
    return m_resource->resourceError();
  }

  void decodeError(bool allDataReceived) override {
    m_resource->decodeError(allDataReceived);
  }
  void setDecodedSize(size_t size) override {
    m_resource->setDecodedSize(size);
  }
  void willAddClientOrObserver() override {
    m_resource->willAddClientOrObserver(Resource::MarkAsReferenced);
  }
  void didRemoveClientOrObserver() override {
    m_resource->didRemoveClientOrObserver();
  }
  void emulateLoadStartedForInspector(
      ResourceFetcher* fetcher,
      const KURL& url,
      const AtomicString& initiatorName) override {
    fetcher->emulateLoadStartedForInspector(m_resource.get(), url,
                                            WebURLRequest::RequestContextImage,
                                            initiatorName);
  }

  const Member<ImageResource> m_resource;
};

class ImageResource::ImageResourceFactory : public ResourceFactory {
  STACK_ALLOCATED();

 public:
  ImageResourceFactory(const FetchRequest& fetchRequest)
      : ResourceFactory(Resource::Image), m_fetchRequest(&fetchRequest) {}

  Resource* create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const String&) const override {
    return new ImageResource(request, options, ImageResourceContent::create(),
                             m_fetchRequest->placeholderImageRequestType() ==
                                 FetchRequest::AllowPlaceholder);
  }

 private:
  // Weak, unowned pointer. Must outlive |this|.
  const FetchRequest* m_fetchRequest;
};

ImageResource* ImageResource::fetch(FetchRequest& request,
                                    ResourceFetcher* fetcher) {
  if (request.resourceRequest().requestContext() ==
      WebURLRequest::RequestContextUnspecified) {
    request.mutableResourceRequest().setRequestContext(
        WebURLRequest::RequestContextImage);
  }
  if (fetcher->context().pageDismissalEventBeingDispatched()) {
    KURL requestURL = request.resourceRequest().url();
    if (requestURL.isValid()) {
      ResourceRequestBlockedReason blockReason = fetcher->context().canRequest(
          Resource::Image, request.resourceRequest(), requestURL,
          request.options(), request.forPreload(),
          request.getOriginRestriction());
      if (blockReason == ResourceRequestBlockedReason::None)
        fetcher->context().sendImagePing(requestURL);
    }
    return nullptr;
  }

  ImageResource* resource = toImageResource(
      fetcher->requestResource(request, ImageResourceFactory(request)));
  if (resource &&
      request.placeholderImageRequestType() != FetchRequest::AllowPlaceholder &&
      resource->m_isPlaceholder) {
    // If the image is a placeholder, but this fetch doesn't allow a
    // placeholder, then load the original image. Note that the cache is not
    // bypassed here - it should be fine to use a cached copy if possible.
    resource->reloadIfLoFiOrPlaceholderImage(
        fetcher, kReloadAlwaysWithExistingCachePolicy);
  }
  return resource;
}

ImageResource* ImageResource::create(const ResourceRequest& request) {
  return new ImageResource(request, ResourceLoaderOptions(),
                           ImageResourceContent::create(), false);
}

ImageResource::ImageResource(const ResourceRequest& resourceRequest,
                             const ResourceLoaderOptions& options,
                             ImageResourceContent* content,
                             bool isPlaceholder)
    : Resource(resourceRequest, Image, options),
      m_content(content),
      m_devicePixelRatioHeaderValue(1.0),
      m_hasDevicePixelRatioHeaderValue(false),
      m_isSchedulingReload(false),
      m_isPlaceholder(isPlaceholder),
      m_flushTimer(this, &ImageResource::flushImageIfNeeded) {
  DCHECK(getContent());
  RESOURCE_LOADING_DVLOG(1) << "new ImageResource(ResourceRequest) " << this;
  getContent()->setImageResourceInfo(new ImageResourceInfoImpl(this));
}

ImageResource::~ImageResource() {
  RESOURCE_LOADING_DVLOG(1) << "~ImageResource " << this;
}

DEFINE_TRACE(ImageResource) {
  visitor->trace(m_multipartParser);
  visitor->trace(m_content);
  Resource::trace(visitor);
  MultipartImageResourceParser::Client::trace(visitor);
}

void ImageResource::checkNotify() {
  // Don't notify clients of completion if this ImageResource is
  // about to be reloaded.
  if (m_isSchedulingReload || shouldReloadBrokenPlaceholder())
    return;

  Resource::checkNotify();
}

bool ImageResource::hasClientsOrObservers() const {
  return Resource::hasClientsOrObservers() || getContent()->hasObservers();
}

void ImageResource::didAddClient(ResourceClient* client) {
  DCHECK((m_multipartParser && isLoading()) || !data() ||
         getContent()->hasImage());

  // Don't notify observers and clients of completion if this ImageResource is
  // about to be reloaded.
  if (m_isSchedulingReload || shouldReloadBrokenPlaceholder())
    return;

  Resource::didAddClient(client);
}

void ImageResource::destroyDecodedDataForFailedRevalidation() {
  getContent()->clearImage();
  setDecodedSize(0);
}

void ImageResource::destroyDecodedDataIfPossible() {
  getContent()->destroyDecodedData();
  if (getContent()->hasImage() && !isPreloaded() &&
      getContent()->isRefetchableDataFromDiskCache()) {
    UMA_HISTOGRAM_MEMORY_KB("Memory.Renderer.EstimatedDroppableEncodedSize",
                            encodedSize() / 1024);
  }
}

void ImageResource::allClientsAndObserversRemoved() {
  CHECK(!getContent()->hasImage() || !errorOccurred());
  // If possible, delay the resetting until back at the event loop. Doing so
  // after a conservative GC prevents resetAnimation() from upsetting ongoing
  // animation updates (crbug.com/613709)
  if (!ThreadHeap::willObjectBeLazilySwept(this)) {
    Platform::current()->currentThread()->getWebTaskRunner()->postTask(
        BLINK_FROM_HERE, WTF::bind(&ImageResourceContent::doResetAnimation,
                                   wrapWeakPersistent(getContent())));
  } else {
    getContent()->doResetAnimation();
  }
  if (m_multipartParser)
    m_multipartParser->cancel();
  Resource::allClientsAndObserversRemoved();
}

PassRefPtr<const SharedBuffer> ImageResource::resourceBuffer() const {
  if (data())
    return data();
  return getContent()->resourceBuffer();
}

void ImageResource::appendData(const char* data, size_t length) {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(length);
  if (m_multipartParser) {
    m_multipartParser->appendData(data, length);
  } else {
    Resource::appendData(data, length);

    // Update the image immediately if needed.
    if (getContent()->shouldUpdateImageImmediately()) {
      getContent()->updateImage(this->data(),
                                ImageResourceContent::KeepExistingImage, false);
      return;
    }

    // For other cases, only update at |kFlushDelaySeconds| intervals. This
    // throttles how frequently we update |m_image| and how frequently we
    // inform the clients which causes an invalidation of this image. In other
    // words, we only invalidate this image every |kFlushDelaySeconds| seconds
    // while loading.
    if (!m_flushTimer.isActive()) {
      double now = WTF::monotonicallyIncreasingTime();
      if (!m_lastFlushTime)
        m_lastFlushTime = now;

      DCHECK_LE(m_lastFlushTime, now);
      double flushDelay = m_lastFlushTime - now + kFlushDelaySeconds;
      if (flushDelay < 0.)
        flushDelay = 0.;
      m_flushTimer.startOneShot(flushDelay, BLINK_FROM_HERE);
    }
  }
}

void ImageResource::flushImageIfNeeded(TimerBase*) {
  // We might have already loaded the image fully, in which case we don't need
  // to call |updateImage()|.
  if (isLoading()) {
    m_lastFlushTime = WTF::monotonicallyIncreasingTime();
    getContent()->updateImage(this->data(),
                              ImageResourceContent::KeepExistingImage, false);
  }
}

bool ImageResource::willPaintBrokenImage() const {
  return errorOccurred();
}

void ImageResource::decodeError(bool allDataReceived) {
  size_t size = encodedSize();

  clearData();
  setEncodedSize(0);
  if (!errorOccurred())
    setStatus(DecodeError);

  if (!allDataReceived && loader()) {
    // TODO(hiroshige): Do not call didFinishLoading() directly.
    loader()->didFinishLoading(monotonicallyIncreasingTime(), size, size);
  }

  memoryCache()->remove(this);
}

void ImageResource::updateImageAndClearBuffer() {
  getContent()->updateImage(data(), ImageResourceContent::ClearExistingImage,
                            true);
  clearData();
}

void ImageResource::finish(double loadFinishTime) {
  if (m_multipartParser) {
    m_multipartParser->finish();
    if (data())
      updateImageAndClearBuffer();
  } else {
    getContent()->updateImage(data(), ImageResourceContent::KeepExistingImage,
                              true);
    // As encoded image data can be created from m_image  (see
    // ImageResource::resourceBuffer(), we don't have to keep m_data. Let's
    // clear this. As for the lifetimes of m_image and m_data, see this
    // document:
    // https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
    clearData();
  }
  Resource::finish(loadFinishTime);
}

void ImageResource::error(const ResourceError& error) {
  if (m_multipartParser)
    m_multipartParser->cancel();
  // TODO(hiroshige): Move setEncodedSize() call to Resource::error() if it
  // is really needed, or remove it otherwise.
  setEncodedSize(0);
  Resource::error(error);
  getContent()->clearImageAndNotifyObservers(
      ImageResourceContent::ShouldNotifyFinish);
}

void ImageResource::responseReceived(
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  DCHECK(!m_multipartParser);
  // If there's no boundary, just handle the request normally.
  if (response.isMultipart() && !response.multipartBoundary().isEmpty()) {
    m_multipartParser = new MultipartImageResourceParser(
        response, response.multipartBoundary(), this);
  }
  Resource::responseReceived(response, std::move(handle));
  if (RuntimeEnabledFeatures::clientHintsEnabled()) {
    m_devicePixelRatioHeaderValue =
        this->response()
            .httpHeaderField(HTTPNames::Content_DPR)
            .toFloat(&m_hasDevicePixelRatioHeaderValue);
    if (!m_hasDevicePixelRatioHeaderValue ||
        m_devicePixelRatioHeaderValue <= 0.0) {
      m_devicePixelRatioHeaderValue = 1.0;
      m_hasDevicePixelRatioHeaderValue = false;
    }
  }
}

static bool isLoFiImage(const ImageResource& resource) {
  if (resource.resourceRequest().loFiState() != WebURLRequest::LoFiOn)
    return false;
  return !resource.isLoaded() ||
         resource.response()
             .httpHeaderField("chrome-proxy-content-transform")
             .contains("empty-image");
}

void ImageResource::reloadIfLoFiOrPlaceholderImage(
    ResourceFetcher* fetcher,
    ReloadLoFiOrPlaceholderPolicy policy) {
  if (policy == kReloadIfNeeded && !shouldReloadBrokenPlaceholder())
    return;

  if (!m_isPlaceholder && !isLoFiImage(*this))
    return;

  // Prevent clients and observers from being notified of completion while the
  // reload is being scheduled, so that e.g. canceling an existing load in
  // progress doesn't cause clients and observers to be notified of completion
  // prematurely.
  DCHECK(!m_isSchedulingReload);
  m_isSchedulingReload = true;

  if (policy != kReloadAlwaysWithExistingCachePolicy)
    setCachePolicyBypassingCache();
  setLoFiStateOff();

  if (m_isPlaceholder) {
    m_isPlaceholder = false;
    clearRangeRequestHeader();
  }

  if (isLoading()) {
    loader()->cancel();
    // Canceling the loader causes error() to be called, which in turn calls
    // clear() and notifyObservers(), so there's no need to call these again
    // here.
  } else {
    clearData();
    setEncodedSize(0);
    getContent()->clearImageAndNotifyObservers(
        ImageResourceContent::DoNotNotifyFinish);
  }

  setStatus(NotStarted);

  DCHECK(m_isSchedulingReload);
  m_isSchedulingReload = false;

  fetcher->startLoad(this);
}

void ImageResource::onePartInMultipartReceived(
    const ResourceResponse& response) {
  DCHECK(m_multipartParser);

  setResponse(response);
  if (m_multipartParsingState == MultipartParsingState::WaitingForFirstPart) {
    // We have nothing to do because we don't have any data.
    m_multipartParsingState = MultipartParsingState::ParsingFirstPart;
    return;
  }
  updateImageAndClearBuffer();

  if (m_multipartParsingState == MultipartParsingState::ParsingFirstPart) {
    m_multipartParsingState = MultipartParsingState::FinishedParsingFirstPart;
    // Notify finished when the first part ends.
    if (!errorOccurred())
      setStatus(Cached);
    // We notify clients and observers of finish in checkNotify() and
    // updateImageAndClearBuffer(), respectively, and they will not be
    // notified again in Resource::finish()/error().
    checkNotify();
    if (loader())
      loader()->didFinishLoadingFirstPartInMultipart();
  }
}

void ImageResource::multipartDataReceived(const char* bytes, size_t size) {
  DCHECK(m_multipartParser);
  Resource::appendData(bytes, size);
}

bool ImageResource::isAccessAllowed(
    SecurityOrigin* securityOrigin,
    ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin
        doesCurrentFrameHasSingleSecurityOrigin) const {
  if (response().wasFetchedViaServiceWorker()) {
    return response().serviceWorkerResponseType() !=
           WebServiceWorkerResponseTypeOpaque;
  }
  if (doesCurrentFrameHasSingleSecurityOrigin !=
      ImageResourceInfo::HasSingleSecurityOrigin)
    return false;
  if (passesAccessControlCheck(securityOrigin))
    return true;
  return !securityOrigin->taintsCanvas(response().url());
}

ImageResourceContent* ImageResource::getContent() {
  return m_content;
}

const ImageResourceContent* ImageResource::getContent() const {
  return m_content;
}

ResourcePriority ImageResource::priorityFromObservers() {
  return getContent()->priorityFromObservers();
}

}  // namespace blink
