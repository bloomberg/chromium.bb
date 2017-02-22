// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageResourceContent_h
#define ImageResourceContent_h

#include "core/CoreExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSizeHash.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/loader/fetch/ResourceStatus.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashCountedSet.h"
#include "wtf/HashMap.h"
#include <memory>

namespace blink {

class FetchRequest;
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
  static ImageResourceContent* create(
      PassRefPtr<blink::Image> image = nullptr) {
    return new ImageResourceContent(std::move(image));
  }
  static ImageResourceContent* fetch(FetchRequest&, ResourceFetcher*);

  // Returns the nullImage() if the image is not available yet.
  blink::Image* getImage();
  bool hasImage() const { return m_image.get(); }

  static std::pair<blink::Image*, float> brokenImage(
      float deviceScaleFactor);  // Returns an image and the image's resolution
                                 // scale factor.

  bool usesImageContainerSize() const;
  bool imageHasRelativeSize() const;
  // The device pixel ratio we got from the server for this image, or 1.0.
  float devicePixelRatioHeaderValue() const;
  bool hasDevicePixelRatioHeaderValue() const;

  enum SizeType {
    // Report the intrinsic size.
    IntrinsicSize,

    // Report the intrinsic size corrected to account for image density.
    IntrinsicCorrectedToDPR,
  };

  // This method takes a zoom multiplier that can be used to increase the
  // natural size of the image by the zoom.
  LayoutSize imageSize(
      RespectImageOrientationEnum shouldRespectImageOrientation,
      float multiplier,
      SizeType = IntrinsicSize);

  void updateImageAnimationPolicy();

  void addObserver(ImageResourceObserver*);
  void removeObserver(ImageResourceObserver*);

  bool isSizeAvailable() const {
    return m_sizeAvailable == Image::SizeAvailable;
  }

  DECLARE_TRACE();

  // Redirecting methods to Resource.
  const KURL& url() const;
  bool isAccessAllowed(SecurityOrigin*);
  const ResourceResponse& response() const;
  bool isLoaded() const;
  bool isLoading() const;
  bool errorOccurred() const;
  bool loadFailedOrCanceled() const;
  ResourceStatus getStatus() const;
  const ResourceError& resourceError() const;

  // For FrameSerializer.
  bool hasCacheControlNoStoreHeader() const;

  void emulateLoadStartedForInspector(ResourceFetcher*,
                                      const KURL&,
                                      const AtomicString& initiatorName);

  void setNotRefetchableDataFromDiskCache() {
    m_isRefetchableDataFromDiskCache = false;
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
    UpdateImage,

    // Clears the image and then updates the image if needed.
    ClearAndUpdateImage,

    // Clears the image and always notifies observers (without updating).
    ClearImageAndNotifyObservers,
  };
  enum class UpdateImageResult {
    NoDecodeError,

    // Decode error occurred. Observers are not notified.
    // Only occurs when UpdateImage or ClearAndUpdateImage is specified.
    ShouldDecodeError,
  };
  WARN_UNUSED_RESULT UpdateImageResult updateImage(PassRefPtr<SharedBuffer>,
                                                   UpdateImageOption,
                                                   bool allDataReceived);

  void destroyDecodedData();
  void doResetAnimation();

  void setImageResourceInfo(ImageResourceInfo*);

  ResourcePriority priorityFromObservers() const;
  PassRefPtr<const SharedBuffer> resourceBuffer() const;
  bool shouldUpdateImageImmediately() const;
  bool hasObservers() const {
    return !m_observers.isEmpty() || !m_finishedObservers.isEmpty();
  }
  bool isRefetchableDataFromDiskCache() const {
    return m_isRefetchableDataFromDiskCache;
  }

 private:
  explicit ImageResourceContent(PassRefPtr<blink::Image> = nullptr);

  // ImageObserver
  void decodedSizeChangedTo(const blink::Image*, size_t newSize) override;
  bool shouldPauseAnimation(const blink::Image*) override;
  void animationAdvanced(const blink::Image*) override;
  void changedInRect(const blink::Image*, const IntRect&) override;

  PassRefPtr<Image> createImage();
  void clearImage();

  enum NotifyFinishOption { ShouldNotifyFinish, DoNotNotifyFinish };

  // If not null, changeRect is the changed part of the image.
  void notifyObservers(NotifyFinishOption, const IntRect* changeRect = nullptr);
  void markObserverFinished(ImageResourceObserver*);

  Member<ImageResourceInfo> m_info;

  RefPtr<blink::Image> m_image;

  HashCountedSet<ImageResourceObserver*> m_observers;
  HashCountedSet<ImageResourceObserver*> m_finishedObservers;

  Image::SizeAvailability m_sizeAvailable = Image::SizeUnavailable;

  // Indicates if this resource's encoded image data can be purged and refetched
  // from disk cache to save memory usage. See crbug/664437.
  bool m_isRefetchableDataFromDiskCache;

#if DCHECK_IS_ON()
  bool m_isUpdateImageBeingCalled = false;
#endif
};

}  // namespace blink

#endif
