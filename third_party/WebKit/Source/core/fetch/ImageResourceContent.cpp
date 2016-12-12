// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/ImageResourceContent.h"

#include "core/fetch/ImageResource.h"
#include "core/fetch/ImageResourceInfo.h"
#include "core/fetch/ImageResourceObserver.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/PlaceholderImage.h"
#include "platform/tracing/TraceEvent.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include <memory>
#include <v8.h>

namespace blink {
namespace {
class NullImageResourceInfo final
    : public GarbageCollectedFinalized<NullImageResourceInfo>,
      public ImageResourceInfo {
  USING_GARBAGE_COLLECTED_MIXIN(NullImageResourceInfo);

 public:
  NullImageResourceInfo() {}

  DEFINE_INLINE_VIRTUAL_TRACE() { ImageResourceInfo::trace(visitor); }

 private:
  const KURL& url() const override { return m_url; }
  bool isSchedulingReload() const override { return false; }
  bool hasDevicePixelRatioHeaderValue() const override { return false; }
  float devicePixelRatioHeaderValue() const override { return 1.0; }
  const ResourceResponse& response() const override { return m_response; }
  ResourceStatus getStatus() const override { return ResourceStatus::Cached; }
  bool isPlaceholder() const override { return false; }
  bool isCacheValidator() const override { return false; }
  bool schedulingReloadOrShouldReloadBrokenPlaceholder() const override {
    return false;
  }
  bool isAccessAllowed(
      SecurityOrigin*,
      DoesCurrentFrameHaveSingleSecurityOrigin) const override {
    return true;
  }
  bool hasCacheControlNoStoreHeader() const override { return false; }
  const ResourceError& resourceError() const override { return m_error; }

  void decodeError(bool allDataReceived) override {}
  void setDecodedSize(size_t) override {}
  void willAddClientOrObserver() override {}
  void didRemoveClientOrObserver() override {}
  void emulateLoadStartedForInspector(
      ResourceFetcher*,
      const KURL&,
      const AtomicString& initiatorName) override {}

  const KURL m_url;
  const ResourceResponse m_response;
  const ResourceError m_error;
};

}  // namespace

ImageResourceContent::ImageResourceContent(PassRefPtr<blink::Image> image)
    : m_image(image), m_isRefetchableDataFromDiskCache(true) {
  DEFINE_STATIC_LOCAL(NullImageResourceInfo, nullInfo,
                      (new NullImageResourceInfo()));
  m_info = &nullInfo;
}

ImageResourceContent* ImageResourceContent::fetch(FetchRequest& request,
                                                  ResourceFetcher* fetcher) {
  // TODO(hiroshige): Remove direct references to ImageResource by making
  // the dependencies around ImageResource and ImageResourceContent cleaner.
  ImageResource* resource = ImageResource::fetch(request, fetcher);
  if (!resource)
    return nullptr;
  return resource->getContent();
}

void ImageResourceContent::setImageResourceInfo(ImageResourceInfo* info) {
  m_info = info;
}

DEFINE_TRACE(ImageResourceContent) {
  visitor->trace(m_info);
  ImageObserver::trace(visitor);
}

void ImageResourceContent::markObserverFinished(
    ImageResourceObserver* observer) {
  auto it = m_observers.find(observer);
  if (it == m_observers.end())
    return;
  m_observers.remove(it);
  m_finishedObservers.add(observer);
}

void ImageResourceContent::addObserver(ImageResourceObserver* observer) {
  m_info->willAddClientOrObserver();

  m_observers.add(observer);

  if (m_info->isCacheValidator())
    return;

  if (m_image && !m_image->isNull()) {
    observer->imageChanged(this);
  }

  if (isLoaded() && m_observers.contains(observer) &&
      !m_info->schedulingReloadOrShouldReloadBrokenPlaceholder()) {
    markObserverFinished(observer);
    observer->imageNotifyFinished(this);
  }
}

void ImageResourceContent::removeObserver(ImageResourceObserver* observer) {
  DCHECK(observer);

  auto it = m_observers.find(observer);
  if (it != m_observers.end()) {
    m_observers.remove(it);
  } else {
    it = m_finishedObservers.find(observer);
    DCHECK(it != m_finishedObservers.end());
    m_finishedObservers.remove(it);
  }
  m_info->didRemoveClientOrObserver();
}

static void priorityFromObserver(const ImageResourceObserver* observer,
                                 ResourcePriority& priority) {
  ResourcePriority nextPriority = observer->computeResourcePriority();
  if (nextPriority.visibility == ResourcePriority::NotVisible)
    return;
  priority.visibility = ResourcePriority::Visible;
  priority.intraPriorityValue += nextPriority.intraPriorityValue;
}

ResourcePriority ImageResourceContent::priorityFromObservers() const {
  ResourcePriority priority;

  for (const auto& it : m_finishedObservers)
    priorityFromObserver(it.key, priority);
  for (const auto& it : m_observers)
    priorityFromObserver(it.key, priority);

  return priority;
}

void ImageResourceContent::destroyDecodedData() {
  if (!m_image)
    return;
  CHECK(!errorOccurred());
  m_image->destroyDecodedData();
}

void ImageResourceContent::doResetAnimation() {
  if (m_image)
    m_image->resetAnimation();
}

PassRefPtr<const SharedBuffer> ImageResourceContent::resourceBuffer() const {
  if (m_image)
    return m_image->data();
  return nullptr;
}

bool ImageResourceContent::shouldUpdateImageImmediately() const {
  // If we don't have the size available yet, then update immediately since
  // we need to know the image size as soon as possible. Likewise for
  // animated images, update right away since we shouldn't throttle animated
  // images.
  return m_sizeAvailable == Image::SizeUnavailable ||
         (m_image && m_image->maybeAnimated());
}

std::pair<blink::Image*, float> ImageResourceContent::brokenImage(
    float deviceScaleFactor) {
  if (deviceScaleFactor >= 2) {
    DEFINE_STATIC_REF(blink::Image, brokenImageHiRes,
                      (blink::Image::loadPlatformResource("missingImage@2x")));
    return std::make_pair(brokenImageHiRes, 2);
  }

  DEFINE_STATIC_REF(blink::Image, brokenImageLoRes,
                    (blink::Image::loadPlatformResource("missingImage")));
  return std::make_pair(brokenImageLoRes, 1);
}

blink::Image* ImageResourceContent::getImage() {
  if (errorOccurred()) {
    // Returning the 1x broken image is non-ideal, but we cannot reliably access
    // the appropriate deviceScaleFactor from here. It is critical that callers
    // use ImageResourceContent::brokenImage() when they need the real,
    // deviceScaleFactor-appropriate broken image icon.
    return brokenImage(1).first;
  }

  if (m_image)
    return m_image.get();

  return blink::Image::nullImage();
}

bool ImageResourceContent::usesImageContainerSize() const {
  if (m_image)
    return m_image->usesContainerSize();

  return false;
}

bool ImageResourceContent::imageHasRelativeSize() const {
  if (m_image)
    return m_image->hasRelativeSize();

  return false;
}

LayoutSize ImageResourceContent::imageSize(
    RespectImageOrientationEnum shouldRespectImageOrientation,
    float multiplier,
    SizeType sizeType) {
  if (!m_image)
    return LayoutSize();

  LayoutSize size;

  if (m_image->isBitmapImage() &&
      shouldRespectImageOrientation == RespectImageOrientation) {
    size =
        LayoutSize(toBitmapImage(m_image.get())->sizeRespectingOrientation());
  } else {
    size = LayoutSize(m_image->size());
  }

  if (sizeType == IntrinsicCorrectedToDPR && hasDevicePixelRatioHeaderValue() &&
      devicePixelRatioHeaderValue() > 0)
    multiplier = 1 / devicePixelRatioHeaderValue();

  if (multiplier == 1 || m_image->hasRelativeSize())
    return size;

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  LayoutSize minimumSize(
      size.width() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit(),
      LayoutUnit(size.height() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit()));
  size.scale(multiplier);
  size.clampToMinimumSize(minimumSize);
  return size;
}

void ImageResourceContent::notifyObservers(
    NotifyFinishOption notifyingFinishOption,
    const IntRect* changeRect) {
  for (auto* observer : m_finishedObservers.asVector()) {
    if (m_finishedObservers.contains(observer))
      observer->imageChanged(this, changeRect);
  }
  for (auto* observer : m_observers.asVector()) {
    if (m_observers.contains(observer)) {
      observer->imageChanged(this, changeRect);
      if (notifyingFinishOption == ShouldNotifyFinish &&
          m_observers.contains(observer) &&
          !m_info->schedulingReloadOrShouldReloadBrokenPlaceholder()) {
        markObserverFinished(observer);
        observer->imageNotifyFinished(this);
      }
    }
  }
}

PassRefPtr<Image> ImageResourceContent::createImage() {
  if (m_info->response().mimeType() == "image/svg+xml")
    return SVGImage::create(this);
  return BitmapImage::create(this);
}

void ImageResourceContent::clearImage() {
  if (!m_image)
    return;
  int64_t length = m_image->data() ? m_image->data()->size() : 0;
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-length);

  // If our Image has an observer, it's always us so we need to clear the back
  // pointer before dropping our reference.
  m_image->clearImageObserver();
  m_image.clear();
  m_sizeAvailable = Image::SizeUnavailable;
}

void ImageResourceContent::clearImageAndNotifyObservers(
    NotifyFinishOption notifyingFinishOption) {
  clearImage();
  notifyObservers(notifyingFinishOption);
}

void ImageResourceContent::updateImage(PassRefPtr<SharedBuffer> data,
                                       ClearImageOption clearImageOption,
                                       bool allDataReceived) {
  TRACE_EVENT0("blink", "ImageResourceContent::updateImage");

  if (clearImageOption == ImageResourceContent::ClearExistingImage) {
    clearImage();
  }

  // Have the image update its data from its internal buffer. It will not do
  // anything now, but will delay decoding until queried for info (like size or
  // specific image frames).
  if (data) {
    if (!m_image)
      m_image = createImage();
    DCHECK(m_image);
    m_sizeAvailable = m_image->setData(std::move(data), allDataReceived);
  }

  // Go ahead and tell our observers to try to draw if we have either received
  // all the data or the size is known. Each chunk from the network causes
  // observers to repaint, which will force that chunk to decode.
  if (m_sizeAvailable == Image::SizeUnavailable && !allDataReceived)
    return;

  if (m_info->isPlaceholder() && allDataReceived && m_image &&
      !m_image->isNull()) {
    if (m_sizeAvailable == Image::SizeAvailable) {
      // TODO(sclittle): Show the original image if the response consists of the
      // entire image, such as if the entire image response body is smaller than
      // the requested range.
      IntSize dimensions = m_image->size();

      clearImage();
      m_image = PlaceholderImage::create(this, dimensions);
    } else {
      // Clear the image so that it gets treated like a decoding error, since
      // the attempt to build a placeholder image failed.
      clearImage();
    }
  }

  if (!m_image || m_image->isNull()) {
    clearImage();
    m_info->decodeError(allDataReceived);
  }

  // It would be nice to only redraw the decoded band of the image, but with the
  // current design (decoding delayed until painting) that seems hard.
  notifyObservers(allDataReceived ? ShouldNotifyFinish : DoNotNotifyFinish);
}

void ImageResourceContent::decodedSizeChangedTo(const blink::Image* image,
                                                size_t newSize) {
  if (!image || image != m_image)
    return;

  m_info->setDecodedSize(newSize);
}

bool ImageResourceContent::shouldPauseAnimation(const blink::Image* image) {
  if (!image || image != m_image)
    return false;

  for (const auto& it : m_finishedObservers)
    if (it.key->willRenderImage())
      return false;

  for (const auto& it : m_observers)
    if (it.key->willRenderImage())
      return false;

  return true;
}

void ImageResourceContent::animationAdvanced(const blink::Image* image) {
  if (!image || image != m_image)
    return;
  notifyObservers(DoNotNotifyFinish);
}

void ImageResourceContent::updateImageAnimationPolicy() {
  if (!m_image)
    return;

  ImageAnimationPolicy newPolicy = ImageAnimationPolicyAllowed;
  for (const auto& it : m_finishedObservers) {
    if (it.key->getImageAnimationPolicy(newPolicy))
      break;
  }
  for (const auto& it : m_observers) {
    if (it.key->getImageAnimationPolicy(newPolicy))
      break;
  }

  if (m_image->animationPolicy() != newPolicy) {
    m_image->resetAnimation();
    m_image->setAnimationPolicy(newPolicy);
  }
}

void ImageResourceContent::changedInRect(const blink::Image* image,
                                         const IntRect& rect) {
  if (!image || image != m_image)
    return;
  notifyObservers(DoNotNotifyFinish, &rect);
}

bool ImageResourceContent::isAccessAllowed(SecurityOrigin* securityOrigin) {
  return m_info->isAccessAllowed(
      securityOrigin, getImage()->currentFrameHasSingleSecurityOrigin()
                          ? ImageResourceInfo::HasSingleSecurityOrigin
                          : ImageResourceInfo::HasMultipleSecurityOrigin);
}

void ImageResourceContent::emulateLoadStartedForInspector(
    ResourceFetcher* fetcher,
    const KURL& url,
    const AtomicString& initiatorName) {
  m_info->emulateLoadStartedForInspector(fetcher, url, initiatorName);
}

// TODO(hiroshige): Consider removing the following methods, or stoping
// redirecting to ImageResource.
bool ImageResourceContent::isLoaded() const {
  return getStatus() > ResourceStatus::Pending;
}

bool ImageResourceContent::isLoading() const {
  return getStatus() == ResourceStatus::Pending;
}

bool ImageResourceContent::errorOccurred() const {
  return getStatus() == ResourceStatus::LoadError ||
         getStatus() == ResourceStatus::DecodeError;
}

bool ImageResourceContent::loadFailedOrCanceled() const {
  return getStatus() == ResourceStatus::LoadError;
}

ResourceStatus ImageResourceContent::getStatus() const {
  return m_info->getStatus();
}

const KURL& ImageResourceContent::url() const {
  return m_info->url();
}

bool ImageResourceContent::hasCacheControlNoStoreHeader() const {
  return m_info->hasCacheControlNoStoreHeader();
}

float ImageResourceContent::devicePixelRatioHeaderValue() const {
  return m_info->devicePixelRatioHeaderValue();
}

bool ImageResourceContent::hasDevicePixelRatioHeaderValue() const {
  return m_info->hasDevicePixelRatioHeaderValue();
}

const ResourceResponse& ImageResourceContent::response() const {
  return m_info->response();
}

const ResourceError& ImageResourceContent::resourceError() const {
  return m_info->resourceError();
}

}  // namespace blink
