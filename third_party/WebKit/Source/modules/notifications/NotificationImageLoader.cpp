// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationImageLoader.h"

#include <memory>
#include "core/dom/ExecutionContext.h"
#include "platform/Histogram.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoadPriority.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/notifications/WebNotificationConstants.h"
#include "skia/ext/image_operations.h"

#define NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, type_name, value, max) \
  case NotificationImageLoader::Type::k##type_name: {                         \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram,                     \
                                    metric##type_name##Histogram,             \
                                    ("Notifications." #metric "." #type_name, \
                                     1 /* min */, max, 50 /* buckets */));    \
    metric##type_name##Histogram.Count(value);                                \
    break;                                                                    \
  }

#define NOTIFICATION_HISTOGRAM_COUNTS(metric, type, value, max)            \
  switch (type) {                                                          \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, Image, value, max)      \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, Icon, value, max)       \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, Badge, value, max)      \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, ActionIcon, value, max) \
  }

namespace {

// 99.9% of all images were fetched successfully in 90 seconds.
const unsigned long kImageFetchTimeoutInMs = 90000;

}  // namespace

namespace blink {

NotificationImageLoader::NotificationImageLoader(Type type)
    : type_(type), stopped_(false), start_time_(0.0) {}

NotificationImageLoader::~NotificationImageLoader() = default;

// static
SkBitmap NotificationImageLoader::ScaleDownIfNeeded(const SkBitmap& image,
                                                    Type type) {
  int max_width_px = 0, max_height_px = 0;
  switch (type) {
    case Type::kImage:
      max_width_px = kWebNotificationMaxImageWidthPx;
      max_height_px = kWebNotificationMaxImageHeightPx;
      break;
    case Type::kIcon:
      max_width_px = kWebNotificationMaxIconSizePx;
      max_height_px = kWebNotificationMaxIconSizePx;
      break;
    case Type::kBadge:
      max_width_px = kWebNotificationMaxBadgeSizePx;
      max_height_px = kWebNotificationMaxBadgeSizePx;
      break;
    case Type::kActionIcon:
      max_width_px = kWebNotificationMaxActionIconSizePx;
      max_height_px = kWebNotificationMaxActionIconSizePx;
      break;
  }
  DCHECK_GT(max_width_px, 0);
  DCHECK_GT(max_height_px, 0);
  // TODO(peter): Explore doing the scaling on a background thread.
  if (image.width() > max_width_px || image.height() > max_height_px) {
    double scale =
        std::min(static_cast<double>(max_width_px) / image.width(),
                 static_cast<double>(max_height_px) / image.height());
    double start_time = CurrentTimeTicksInMilliseconds();
    // TODO(peter): Try using RESIZE_BETTER for large images.
    SkBitmap scaled_image =
        skia::ImageOperations::Resize(image, skia::ImageOperations::RESIZE_BEST,
                                      std::lround(scale * image.width()),
                                      std::lround(scale * image.height()));
    NOTIFICATION_HISTOGRAM_COUNTS(LoadScaleDownTime, type,
                                  CurrentTimeTicksInMilliseconds() - start_time,
                                  1000 * 10 /* 10 seconds max */);
    return scaled_image;
  }
  return image;
}

void NotificationImageLoader::Start(ExecutionContext* execution_context,
                                    const KURL& url,
                                    ImageCallback image_callback) {
  DCHECK(!stopped_);

  start_time_ = CurrentTimeTicksInMilliseconds();
  image_callback_ = std::move(image_callback);

  ThreadableLoaderOptions threadable_loader_options;
  threadable_loader_options.timeout_milliseconds = kImageFetchTimeoutInMs;

  // TODO(mvanouwerkerk): Add an entry for notifications to
  // FetchInitiatorTypeNames and use it.
  ResourceLoaderOptions resource_loader_options;
  if (execution_context->IsWorkerGlobalScope())
    resource_loader_options.request_initiator_context = kWorkerContext;

  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetRequestorOrigin(execution_context->GetSecurityOrigin());

  threadable_loader_ = ThreadableLoader::Create(*execution_context, this,
                                                threadable_loader_options,
                                                resource_loader_options);
  threadable_loader_->Start(resource_request);
}

void NotificationImageLoader::Stop() {
  if (stopped_)
    return;

  stopped_ = true;
  if (threadable_loader_) {
    threadable_loader_->Cancel();
    threadable_loader_ = nullptr;
  }
}

void NotificationImageLoader::DidReceiveData(const char* data,
                                             unsigned length) {
  if (!data_)
    data_ = SharedBuffer::Create();
  data_->Append(data, length);
}

void NotificationImageLoader::DidFinishLoading(
    unsigned long resource_identifier,
    double finish_time) {
  // If this has been stopped it is not desirable to trigger further work,
  // there is a shutdown of some sort in progress.
  if (stopped_)
    return;

  NOTIFICATION_HISTOGRAM_COUNTS(LoadFinishTime, type_,
                                CurrentTimeTicksInMilliseconds() - start_time_,
                                1000 * 60 * 60 /* 1 hour max */);

  if (data_) {
    NOTIFICATION_HISTOGRAM_COUNTS(LoadFileSize, type_, data_->size(),
                                  10000000 /* ~10mb max */);

    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
        data_, true /* dataComplete */, ImageDecoder::kAlphaPremultiplied,
        ColorBehavior::TransformToSRGB());
    if (decoder) {
      // The |ImageFrame*| is owned by the decoder.
      ImageFrame* image_frame = decoder->DecodeFrameBufferAtIndex(0);
      if (image_frame) {
        std::move(image_callback_).Run(image_frame->Bitmap());
        return;
      }
    }
  }
  RunCallbackWithEmptyBitmap();
}

void NotificationImageLoader::DidFail(const ResourceError& error) {
  NOTIFICATION_HISTOGRAM_COUNTS(LoadFailTime, type_,
                                CurrentTimeTicksInMilliseconds() - start_time_,
                                1000 * 60 * 60 /* 1 hour max */);

  RunCallbackWithEmptyBitmap();
}

void NotificationImageLoader::DidFailRedirectCheck() {
  RunCallbackWithEmptyBitmap();
}

void NotificationImageLoader::RunCallbackWithEmptyBitmap() {
  // If this has been stopped it is not desirable to trigger further work,
  // there is a shutdown of some sort in progress.
  if (stopped_)
    return;

  std::move(image_callback_).Run(SkBitmap());
}

}  // namespace blink
