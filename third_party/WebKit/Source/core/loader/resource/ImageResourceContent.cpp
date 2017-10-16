// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/ImageResourceContent.h"

#include <memory>

#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ImageResourceInfo.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/PlaceholderImage.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"
#include "v8/include/v8.h"

namespace blink {
namespace {
class NullImageResourceInfo final
    : public GarbageCollectedFinalized<NullImageResourceInfo>,
      public ImageResourceInfo {
  USING_GARBAGE_COLLECTED_MIXIN(NullImageResourceInfo);

 public:
  NullImageResourceInfo() {}

  DEFINE_INLINE_VIRTUAL_TRACE() { ImageResourceInfo::Trace(visitor); }

 private:
  const KURL& Url() const override { return url_; }
  bool IsSchedulingReload() const override { return false; }
  bool HasDevicePixelRatioHeaderValue() const override { return false; }
  float DevicePixelRatioHeaderValue() const override { return 1.0; }
  const ResourceResponse& GetResponse() const override { return response_; }
  bool ShouldShowPlaceholder() const override { return false; }
  bool IsCacheValidator() const override { return false; }
  bool SchedulingReloadOrShouldReloadBrokenPlaceholder() const override {
    return false;
  }
  bool IsAccessAllowed(
      SecurityOrigin*,
      DoesCurrentFrameHaveSingleSecurityOrigin) const override {
    return true;
  }
  bool HasCacheControlNoStoreHeader() const override { return false; }
  const ResourceError& GetResourceError() const override { return error_; }

  void SetDecodedSize(size_t) override {}
  void WillAddClientOrObserver() override {}
  void DidRemoveClientOrObserver() override {}
  void EmulateLoadStartedForInspector(
      ResourceFetcher*,
      const KURL&,
      const AtomicString& initiator_name) override {}

  const KURL url_;
  const ResourceResponse response_;
  const ResourceError error_;
};

}  // namespace

ImageResourceContent::ImageResourceContent(RefPtr<blink::Image> image)
    : is_refetchable_data_from_disk_cache_(true), image_(std::move(image)) {
  DEFINE_STATIC_LOCAL(NullImageResourceInfo, null_info,
                      (new NullImageResourceInfo()));
  info_ = &null_info;
}

ImageResourceContent* ImageResourceContent::CreateLoaded(
    RefPtr<blink::Image> image) {
  DCHECK(image);
  ImageResourceContent* content = new ImageResourceContent(std::move(image));
  content->content_status_ = ResourceStatus::kCached;
  return content;
}

ImageResourceContent* ImageResourceContent::Fetch(FetchParameters& params,
                                                  ResourceFetcher* fetcher) {
  // TODO(hiroshige): Remove direct references to ImageResource by making
  // the dependencies around ImageResource and ImageResourceContent cleaner.
  ImageResource* resource = ImageResource::Fetch(params, fetcher);
  if (!resource)
    return nullptr;
  return resource->GetContent();
}

void ImageResourceContent::SetImageResourceInfo(ImageResourceInfo* info) {
  info_ = info;
}

DEFINE_TRACE(ImageResourceContent) {
  visitor->Trace(info_);
  ImageObserver::Trace(visitor);
}

void ImageResourceContent::MarkObserverFinished(
    ImageResourceObserver* observer) {
  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);

  auto it = observers_.find(observer);
  if (it == observers_.end())
    return;
  observers_.erase(it);
  finished_observers_.insert(observer);
}

void ImageResourceContent::AddObserver(ImageResourceObserver* observer) {
  CHECK(!is_add_remove_observer_prohibited_);

  info_->WillAddClientOrObserver();

  {
    ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
        this);
    observers_.insert(observer);
  }

  if (info_->IsCacheValidator())
    return;

  if (image_ && !image_->IsNull()) {
    observer->ImageChanged(this);
  }

  if (IsLoaded() && observers_.Contains(observer) &&
      !info_->SchedulingReloadOrShouldReloadBrokenPlaceholder()) {
    MarkObserverFinished(observer);
    observer->ImageNotifyFinished(this);
  }
}

void ImageResourceContent::RemoveObserver(ImageResourceObserver* observer) {
  DCHECK(observer);
  CHECK(!is_add_remove_observer_prohibited_);
  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);

  auto it = observers_.find(observer);
  if (it != observers_.end()) {
    observers_.erase(it);
  } else {
    it = finished_observers_.find(observer);
    DCHECK(it != finished_observers_.end());
    finished_observers_.erase(it);
  }
  info_->DidRemoveClientOrObserver();
}

static void PriorityFromObserver(const ImageResourceObserver* observer,
                                 ResourcePriority& priority) {
  ResourcePriority next_priority = observer->ComputeResourcePriority();
  if (next_priority.visibility == ResourcePriority::kNotVisible)
    return;
  priority.visibility = ResourcePriority::kVisible;
  priority.intra_priority_value += next_priority.intra_priority_value;
}

ResourcePriority ImageResourceContent::PriorityFromObservers() const {
  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);
  ResourcePriority priority;

  for (const auto& it : finished_observers_)
    PriorityFromObserver(it.key, priority);
  for (const auto& it : observers_)
    PriorityFromObserver(it.key, priority);

  return priority;
}

void ImageResourceContent::DestroyDecodedData() {
  if (!image_)
    return;
  CHECK(!ErrorOccurred());
  image_->DestroyDecodedData();
}

void ImageResourceContent::DoResetAnimation() {
  if (image_)
    image_->ResetAnimation();
}

RefPtr<const SharedBuffer> ImageResourceContent::ResourceBuffer() const {
  if (image_)
    return image_->Data();
  return nullptr;
}

bool ImageResourceContent::ShouldUpdateImageImmediately() const {
  // If we don't have the size available yet, then update immediately since
  // we need to know the image size as soon as possible. Likewise for
  // animated images, update right away since we shouldn't throttle animated
  // images.
  return size_available_ == Image::kSizeUnavailable ||
         (image_ && image_->MaybeAnimated());
}

std::pair<blink::Image*, float> ImageResourceContent::BrokenImage(
    float device_scale_factor) {
  if (device_scale_factor >= 2) {
    DEFINE_STATIC_REF(blink::Image, broken_image_hi_res,
                      (blink::Image::LoadPlatformResource("missingImage@2x")));
    return std::make_pair(broken_image_hi_res, 2);
  }

  DEFINE_STATIC_REF(blink::Image, broken_image_lo_res,
                    (blink::Image::LoadPlatformResource("missingImage")));
  return std::make_pair(broken_image_lo_res, 1);
}

blink::Image* ImageResourceContent::GetImage() {
  if (ErrorOccurred()) {
    // Returning the 1x broken image is non-ideal, but we cannot reliably access
    // the appropriate deviceScaleFactor from here. It is critical that callers
    // use ImageResourceContent::brokenImage() when they need the real,
    // deviceScaleFactor-appropriate broken image icon.
    return BrokenImage(1).first;
  }

  if (image_)
    return image_.get();

  return blink::Image::NullImage();
}

bool ImageResourceContent::UsesImageContainerSize() const {
  if (image_)
    return image_->UsesContainerSize();

  return false;
}

bool ImageResourceContent::ImageHasRelativeSize() const {
  if (image_)
    return image_->HasRelativeSize();

  return false;
}

LayoutSize ImageResourceContent::ImageSize(
    RespectImageOrientationEnum should_respect_image_orientation,
    float multiplier) {
  if (!image_)
    return LayoutSize();

  LayoutSize size;

  if (image_->IsBitmapImage() &&
      should_respect_image_orientation == kRespectImageOrientation) {
    size = LayoutSize(ToBitmapImage(image_.get())->SizeRespectingOrientation());
  } else {
    size = LayoutSize(image_->Size());
  }

  if (multiplier == 1 || image_->HasRelativeSize())
    return size;

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  LayoutSize minimum_size(
      size.Width() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit(),
      LayoutUnit(size.Height() > LayoutUnit() ? LayoutUnit(1) : LayoutUnit()));
  size.Scale(multiplier);
  size.ClampToMinimumSize(minimum_size);
  return size;
}

void ImageResourceContent::NotifyObservers(
    NotifyFinishOption notifying_finish_option,
    const IntRect* change_rect) {
  {
    Vector<ImageResourceObserver*> finished_observers_as_vector;
    {
      ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
          this);
      finished_observers_as_vector = finished_observers_.AsVector();
    }

    for (auto* observer : finished_observers_as_vector) {
      if (finished_observers_.Contains(observer))
        observer->ImageChanged(this, change_rect);
    }
  }
  {
    Vector<ImageResourceObserver*> observers_as_vector;
    {
      ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
          this);
      observers_as_vector = observers_.AsVector();
    }

    for (auto* observer : observers_as_vector) {
      if (observers_.Contains(observer)) {
        observer->ImageChanged(this, change_rect);
        if (notifying_finish_option == kShouldNotifyFinish &&
            observers_.Contains(observer) &&
            !info_->SchedulingReloadOrShouldReloadBrokenPlaceholder()) {
          MarkObserverFinished(observer);
          observer->ImageNotifyFinished(this);
        }
      }
    }
  }
}

RefPtr<Image> ImageResourceContent::CreateImage(bool is_multipart) {
  if (info_->GetResponse().MimeType() == "image/svg+xml")
    return SVGImage::Create(this, is_multipart);
  return BitmapImage::Create(this, is_multipart);
}

void ImageResourceContent::ClearImage() {
  if (!image_)
    return;
  int64_t length = image_->Data() ? image_->Data()->size() : 0;
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-length);

  // If our Image has an observer, it's always us so we need to clear the back
  // pointer before dropping our reference.
  image_->ClearImageObserver();
  image_ = nullptr;
  size_available_ = Image::kSizeUnavailable;
}

// |new_status| is the status of corresponding ImageResource.
void ImageResourceContent::UpdateToLoadedContentStatus(
    ResourceStatus new_status) {
  // When |ShouldNotifyFinish|, we set content_status_
  // to a loaded ResourceStatus.

  // Checks |new_status| (i.e. Resource's current status).
  switch (new_status) {
    case ResourceStatus::kCached:
    case ResourceStatus::kPending:
      // In case of successful load, Resource's status can be
      // kCached (e.g. for second part of multipart image) or
      // still Pending (e.g. for a non-multipart image).
      // Therefore we use kCached as the new state here.
      new_status = ResourceStatus::kCached;
      break;

    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // In case of error, Resource's status is set to an error status
      // before UpdateImage() and thus we use the error status as-is.
      break;

    case ResourceStatus::kNotStarted:
      CHECK(false);
      break;
  }

  // Checks ImageResourceContent's previous status.
  switch (GetContentStatus()) {
    case ResourceStatus::kPending:
      // A non-multipart image or the first part of a multipart image.
      break;

    case ResourceStatus::kCached:
    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // Second (or later) part of a multipart image.
      // TODO(hiroshige): Assert that this is actually a multipart image.
      break;

    case ResourceStatus::kNotStarted:
      // Should have updated to kPending via NotifyStartLoad().
      CHECK(false);
      break;
  }

  // Updates the status.
  content_status_ = new_status;
}

void ImageResourceContent::NotifyStartLoad() {
  // Checks ImageResourceContent's previous status.
  switch (GetContentStatus()) {
    case ResourceStatus::kPending:
      CHECK(false);
      break;

    case ResourceStatus::kNotStarted:
      // Normal load start.
      break;

    case ResourceStatus::kCached:
    case ResourceStatus::kLoadError:
    case ResourceStatus::kDecodeError:
      // Load start due to revalidation/reload.
      break;
  }

  content_status_ = ResourceStatus::kPending;
}

void ImageResourceContent::AsyncLoadCompleted(const blink::Image* image) {
  if (image_ != image)
    return;
  CHECK_EQ(size_available_, Image::kSizeAvailableAndLoadingAsynchronously);
  size_available_ = Image::kSizeAvailable;
  UpdateToLoadedContentStatus(ResourceStatus::kCached);
  NotifyObservers(kShouldNotifyFinish);
}

ImageResourceContent::UpdateImageResult ImageResourceContent::UpdateImage(
    RefPtr<SharedBuffer> data,
    ResourceStatus status,
    UpdateImageOption update_image_option,
    bool all_data_received,
    bool is_multipart) {
  TRACE_EVENT0("blink", "ImageResourceContent::updateImage");

#if DCHECK_IS_ON()
  DCHECK(!is_update_image_being_called_);
  AutoReset<bool> scope(&is_update_image_being_called_, true);
#endif

  CHECK_NE(GetContentStatus(), ResourceStatus::kNotStarted);

  // Clears the existing image, if instructed by |updateImageOption|.
  switch (update_image_option) {
    case kClearAndUpdateImage:
    case kClearImageAndNotifyObservers:
      ClearImage();
      break;
    case kUpdateImage:
      break;
  }

  // Updates the image, if instructed by |updateImageOption|.
  switch (update_image_option) {
    case kClearImageAndNotifyObservers:
      DCHECK(!data);
      break;

    case kUpdateImage:
    case kClearAndUpdateImage:
      // Have the image update its data from its internal buffer. It will not do
      // anything now, but will delay decoding until queried for info (like size
      // or specific image frames).
      if (data) {
        if (!image_)
          image_ = CreateImage(is_multipart);
        DCHECK(image_);
        size_available_ = image_->SetData(std::move(data), all_data_received);
        DCHECK(all_data_received ||
               size_available_ !=
                   Image::kSizeAvailableAndLoadingAsynchronously);
      }

      // Go ahead and tell our observers to try to draw if we have either
      // received all the data or the size is known. Each chunk from the network
      // causes observers to repaint, which will force that chunk to decode.
      if (size_available_ == Image::kSizeUnavailable && !all_data_received)
        return UpdateImageResult::kNoDecodeError;

      if (info_->ShouldShowPlaceholder() && all_data_received) {
        if (image_ && !image_->IsNull()) {
          IntSize dimensions = image_->Size();
          ClearImage();
          image_ = PlaceholderImage::Create(this, dimensions);
        }
      }

      if (!image_ || image_->IsNull()) {
        ClearImage();
        return UpdateImageResult::kShouldDecodeError;
      }
      break;
  }

  DCHECK(all_data_received ||
         size_available_ != Image::kSizeAvailableAndLoadingAsynchronously);

  // Notifies the observers.
  // It would be nice to only redraw the decoded band of the image, but with the
  // current design (decoding delayed until painting) that seems hard.
  //
  // In the case of kSizeAvailableAndLoadingAsynchronously, we are waiting for
  // SVG image completion, and thus we notify observers of kDoNotNotifyFinish
  // here, and will notify observers of finish later in AsyncLoadCompleted().
  if (all_data_received &&
      size_available_ != Image::kSizeAvailableAndLoadingAsynchronously) {
    UpdateToLoadedContentStatus(status);
    NotifyObservers(kShouldNotifyFinish);
  } else {
    NotifyObservers(kDoNotNotifyFinish);
  }

  return UpdateImageResult::kNoDecodeError;
}

void ImageResourceContent::DecodedSizeChangedTo(const blink::Image* image,
                                                size_t new_size) {
  if (!image || image != image_)
    return;

  info_->SetDecodedSize(new_size);
}

bool ImageResourceContent::ShouldPauseAnimation(const blink::Image* image) {
  if (!image || image != image_)
    return false;

  ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(this);

  for (const auto& it : finished_observers_) {
    if (it.key->WillRenderImage())
      return false;
  }

  for (const auto& it : observers_) {
    if (it.key->WillRenderImage())
      return false;
  }

  return true;
}

void ImageResourceContent::AnimationAdvanced(const blink::Image* image) {
  if (!image || image != image_)
    return;
  NotifyObservers(kDoNotNotifyFinish);
}

void ImageResourceContent::UpdateImageAnimationPolicy() {
  if (!image_)
    return;

  ImageAnimationPolicy new_policy = kImageAnimationPolicyAllowed;
  {
    ProhibitAddRemoveObserverInScope prohibit_add_remove_observer_in_scope(
        this);
    for (const auto& it : finished_observers_) {
      if (it.key->GetImageAnimationPolicy(new_policy))
        break;
    }
    for (const auto& it : observers_) {
      if (it.key->GetImageAnimationPolicy(new_policy))
        break;
    }
  }

  image_->SetAnimationPolicy(new_policy);
}

void ImageResourceContent::ChangedInRect(const blink::Image* image,
                                         const IntRect& rect) {
  if (!image || image != image_)
    return;
  NotifyObservers(kDoNotNotifyFinish, &rect);
}

bool ImageResourceContent::IsAccessAllowed(SecurityOrigin* security_origin) {
  return info_->IsAccessAllowed(
      security_origin, GetImage()->CurrentFrameHasSingleSecurityOrigin()
                           ? ImageResourceInfo::kHasSingleSecurityOrigin
                           : ImageResourceInfo::kHasMultipleSecurityOrigin);
}

void ImageResourceContent::EmulateLoadStartedForInspector(
    ResourceFetcher* fetcher,
    const KURL& url,
    const AtomicString& initiator_name) {
  info_->EmulateLoadStartedForInspector(fetcher, url, initiator_name);
}

bool ImageResourceContent::IsLoaded() const {
  return GetContentStatus() > ResourceStatus::kPending;
}

bool ImageResourceContent::IsLoading() const {
  return GetContentStatus() == ResourceStatus::kPending;
}

bool ImageResourceContent::ErrorOccurred() const {
  return GetContentStatus() == ResourceStatus::kLoadError ||
         GetContentStatus() == ResourceStatus::kDecodeError;
}

bool ImageResourceContent::LoadFailedOrCanceled() const {
  return GetContentStatus() == ResourceStatus::kLoadError;
}

ResourceStatus ImageResourceContent::GetContentStatus() const {
  return content_status_;
}

// TODO(hiroshige): Consider removing the following methods, or stoping
// redirecting to ImageResource.
const KURL& ImageResourceContent::Url() const {
  return info_->Url();
}

bool ImageResourceContent::HasCacheControlNoStoreHeader() const {
  return info_->HasCacheControlNoStoreHeader();
}

float ImageResourceContent::DevicePixelRatioHeaderValue() const {
  return info_->DevicePixelRatioHeaderValue();
}

bool ImageResourceContent::HasDevicePixelRatioHeaderValue() const {
  return info_->HasDevicePixelRatioHeaderValue();
}

const ResourceResponse& ImageResourceContent::GetResponse() const {
  return info_->GetResponse();
}

const ResourceError& ImageResourceContent::GetResourceError() const {
  return info_->GetResourceError();
}

bool ImageResourceContent::IsCacheValidator() const {
  return info_->IsCacheValidator();
}

}  // namespace blink
