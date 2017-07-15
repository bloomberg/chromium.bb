// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageResourceContent_h
#define ImageResourceContent_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSizeHash.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceStatus.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class FetchParameters;
class ImageResourceInfo;
class ImageResourceObserver;
class ResourceError;
class ResourceFetcher;
class ResourceResponse;
class SecurityOrigin;

// ImageResourceContent is a container that holds fetch result of
// an ImageResource in a decoded form.
// Classes that use the fetched images
// should hold onto this class and/or inherit ImageResourceObserver,
// instead of holding onto ImageResource or inheriting ResourceClient.
// https://docs.google.com/document/d/1O-fB83mrE0B_V8gzXNqHgmRLCvstTB4MMi3RnVLr8bE/edit?usp=sharing
// TODO(hiroshige): Make ImageResourceContent ResourceClient and remove the
// word 'observer' from ImageResource.
// TODO(hiroshige): Rename local variables of type ImageResourceContent to
// e.g. |imageContent|. Currently they have Resource-like names.
class CORE_EXPORT ImageResourceContent final
    : public GarbageCollectedFinalized<ImageResourceContent>,
      public ImageObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ImageResourceContent);

 public:
  // Used for loading.
  // Returned content will be associated immediately later with ImageResource.
  static ImageResourceContent* CreateNotStarted() {
    return new ImageResourceContent(nullptr);
  }

  // Creates ImageResourceContent from an already loaded image.
  static ImageResourceContent* CreateLoaded(PassRefPtr<blink::Image>);

  static ImageResourceContent* Fetch(FetchParameters&, ResourceFetcher*);

  ~ImageResourceContent() override;

  // Returns the nullImage() if the image is not available yet.
  blink::Image* GetImage();
  bool HasImage() const { return image_.Get(); }

  static std::pair<blink::Image*, float> BrokenImage(
      float
          device_scale_factor);  // Returns an image and the image's resolution
                                 // scale factor.

  bool UsesImageContainerSize() const;
  bool ImageHasRelativeSize() const;
  // The device pixel ratio we got from the server for this image, or 1.0.
  float DevicePixelRatioHeaderValue() const;
  bool HasDevicePixelRatioHeaderValue() const;

  enum SizeType {
    // Report the intrinsic size.
    kIntrinsicSize,

    // Report the intrinsic size corrected to account for image density.
    kIntrinsicCorrectedToDPR,
  };

  // This method takes a zoom multiplier that can be used to increase the
  // natural size of the image by the zoom.
  LayoutSize ImageSize(
      RespectImageOrientationEnum should_respect_image_orientation,
      float multiplier,
      SizeType = kIntrinsicSize);

  void UpdateImageAnimationPolicy();

  void AddObserver(ImageResourceObserver*);
  void RemoveObserver(ImageResourceObserver*);

  bool IsSizeAvailable() const {
    return size_available_ != Image::kSizeUnavailable;
  }

  DECLARE_TRACE();

  // Content status and deriving predicates.
  // https://docs.google.com/document/d/1O-fB83mrE0B_V8gzXNqHgmRLCvstTB4MMi3RnVLr8bE/edit#heading=h.6cyqmir0f30h
  // Normal transitions:
  //   kNotStarted -> kPending -> kCached|kLoadError|kDecodeError.
  // Additional transitions in multipart images:
  //   kCached -> kLoadError|kDecodeError.
  // Transitions due to revalidation:
  //   kCached -> kPending.
  // Transitions due to reload:
  //   kCached|kLoadError|kDecodeError -> kPending.
  //
  // ImageResourceContent::GetContentStatus() can be different from
  // ImageResource::GetStatus(). Use ImageResourceContent::GetContentStatus().
  ResourceStatus GetContentStatus() const;
  bool IsLoaded() const;
  bool IsLoading() const;
  bool ErrorOccurred() const;
  bool LoadFailedOrCanceled() const;

  // Redirecting methods to Resource.
  const KURL& Url() const;
  bool IsAccessAllowed(SecurityOrigin*);
  const ResourceResponse& GetResponse() const;
  const ResourceError& GetResourceError() const;

  // For FrameSerializer.
  bool HasCacheControlNoStoreHeader() const;

  void EmulateLoadStartedForInspector(ResourceFetcher*,
                                      const KURL&,
                                      const AtomicString& initiator_name);

  void SetNotRefetchableDataFromDiskCache() {
    is_refetchable_data_from_disk_cache_ = false;
  }

  // The following public methods should be called from ImageResource only.

  // updateImage() is the single control point of image content modification
  // from ImageResource that all image updates should call.
  // We clear and/or update images in this single method
  // (controlled by UpdateImageOption) rather than providing separate methods,
  // in order to centralize state changes and
  // not to expose the state inbetween to ImageResource.
  enum UpdateImageOption {
    // Updates the image (including placeholder and decode error handling
    // and notifying observers) if needed.
    kUpdateImage,

    // Clears the image and then updates the image if needed.
    kClearAndUpdateImage,

    // Clears the image and always notifies observers (without updating).
    kClearImageAndNotifyObservers,
  };
  enum class UpdateImageResult {
    kNoDecodeError,

    // Decode error occurred. Observers are not notified.
    // Only occurs when UpdateImage or ClearAndUpdateImage is specified.
    kShouldDecodeError,
  };
  WARN_UNUSED_RESULT UpdateImageResult UpdateImage(PassRefPtr<SharedBuffer>,
                                                   ResourceStatus,
                                                   UpdateImageOption,
                                                   bool all_data_received,
                                                   bool is_multipart);

  void NotifyStartLoad();
  void DestroyDecodedData();
  void DoResetAnimation();

  void SetImageResourceInfo(ImageResourceInfo*);

  ResourcePriority PriorityFromObservers() const;
  PassRefPtr<const SharedBuffer> ResourceBuffer() const;
  bool ShouldUpdateImageImmediately() const;
  bool HasObservers() const {
    return !observers_.IsEmpty() || !finished_observers_.IsEmpty();
  }
  bool IsRefetchableDataFromDiskCache() const {
    return is_refetchable_data_from_disk_cache_;
  }

 private:
  explicit ImageResourceContent(PassRefPtr<blink::Image> = nullptr);

  // ImageObserver
  void DecodedSizeChangedTo(const blink::Image*, size_t new_size) override;
  bool ShouldPauseAnimation(const blink::Image*) override;
  void AnimationAdvanced(const blink::Image*) override;
  void ChangedInRect(const blink::Image*, const IntRect&) override;
  void AsyncLoadCompleted(const blink::Image*) override;

  PassRefPtr<Image> CreateImage(bool is_multipart);
  void ClearImage();

  enum NotifyFinishOption { kShouldNotifyFinish, kDoNotNotifyFinish };

  // If not null, changeRect is the changed part of the image.
  void NotifyObservers(NotifyFinishOption,
                       const IntRect* change_rect = nullptr);
  void MarkObserverFinished(ImageResourceObserver*);
  void UpdateToLoadedContentStatus(ResourceStatus);

  class ProhibitAddRemoveObserverInScope : public AutoReset<bool> {
   public:
    ProhibitAddRemoveObserverInScope(const ImageResourceContent* content)
        : AutoReset(&content->is_add_remove_observer_prohibited_, true) {}
  };

  ResourceStatus content_status_ = ResourceStatus::kNotStarted;

  // Indicates if this resource's encoded image data can be purged and refetched
  // from disk cache to save memory usage. See crbug/664437.
  bool is_refetchable_data_from_disk_cache_;

  mutable bool is_add_remove_observer_prohibited_ = false;

  Image::SizeAvailability size_available_ = Image::kSizeUnavailable;

  Member<ImageResourceInfo> info_;

  RefPtr<blink::Image> image_;

  HashCountedSet<ImageResourceObserver*> observers_;
  HashCountedSet<ImageResourceObserver*> finished_observers_;

  bool is_update_image_being_called_ = false;
};

}  // namespace blink

#endif
