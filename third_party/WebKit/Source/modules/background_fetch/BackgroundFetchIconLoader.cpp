// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LiICENSE file.

#include "modules/background_fetch/BackgroundFetchIconLoader.h"

#include "core/dom/ExecutionContext.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/IconDefinition.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Threading.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebURLRequest.h"
#include "skia/ext/image_operations.h"

namespace blink {

namespace {

const unsigned long kIconFetchTimeoutInMs = 30000;

}  // namespace

BackgroundFetchIconLoader::BackgroundFetchIconLoader() = default;
BackgroundFetchIconLoader::~BackgroundFetchIconLoader() {
  // We should've called Stop() before the destructor is invoked.
  DCHECK(stopped_ || icon_callback_.is_null());
}

// TODO(nator): Add functionality to select which icon to load.
void BackgroundFetchIconLoader::Start(BackgroundFetchBridge* bridge,
                                      ExecutionContext* execution_context,
                                      HeapVector<IconDefinition> icons,
                                      IconCallback icon_callback) {
  DCHECK(!stopped_);
  DCHECK_GE(icons.size(), 1u);
  DCHECK(bridge);

  icons_ = std::move(icons);
  bridge->GetIconDisplaySize(
      WTF::Bind(&BackgroundFetchIconLoader::DidGetIconDisplaySizeIfSoLoadIcon,
                WrapWeakPersistent(this), WrapWeakPersistent(execution_context),
                std::move(icon_callback)));
}

void BackgroundFetchIconLoader::DidGetIconDisplaySizeIfSoLoadIcon(
    ExecutionContext* execution_context,
    IconCallback icon_callback,
    const WebSize& icon_display_size_pixels) {
  // TODO(nator): Pick the appropriate icon based on display size instead,
  // and resize it, if needed.
  if (icon_display_size_pixels.IsEmpty() || !icons_[0].hasSrc()) {
    std::move(icon_callback).Run(SkBitmap());
    return;
  }

  KURL first_icon_url = execution_context->CompleteURL(icons_[0].src());
  if (!first_icon_url.IsValid() || first_icon_url.IsEmpty()) {
    std::move(icon_callback).Run(SkBitmap());
    return;
  }

  icon_callback_ = std::move(icon_callback);

  ThreadableLoaderOptions threadable_loader_options;
  threadable_loader_options.timeout_milliseconds = kIconFetchTimeoutInMs;

  ResourceLoaderOptions resource_loader_options;
  if (execution_context->IsWorkerGlobalScope())
    resource_loader_options.request_initiator_context = kWorkerContext;

  ResourceRequest resource_request(first_icon_url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextImage);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetRequestorOrigin(execution_context->GetSecurityOrigin());

  threadable_loader_ = ThreadableLoader::Create(*execution_context, this,
                                                threadable_loader_options,
                                                resource_loader_options);

  threadable_loader_->Start(resource_request);
}

void BackgroundFetchIconLoader::Stop() {
  if (stopped_)
    return;

  stopped_ = true;
  if (threadable_loader_) {
    threadable_loader_->Cancel();
    threadable_loader_ = nullptr;
  }
}

void BackgroundFetchIconLoader::DidReceiveData(const char* data,
                                               unsigned length) {
  if (!data_)
    data_ = SharedBuffer::Create();
  data_->Append(data, length);
}

void BackgroundFetchIconLoader::DidFinishLoading(
    unsigned long resource_identifier,
    double finish_time) {
  if (stopped_)
    return;
  if (data_) {
    // Decode data.
    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
        data_, true /* data_complete*/, ImageDecoder::kAlphaPremultiplied,
        ColorBehavior::TransformToSRGB());
    if (decoder) {
      // the |ImageFrame*| is owned by the decoder.
      ImageFrame* image_frame = decoder->DecodeFrameBufferAtIndex(0);
      if (image_frame) {
        std::move(icon_callback_).Run(image_frame->Bitmap());
        return;
      }
    }
  }
  RunCallbackWithEmptyBitmap();
}

void BackgroundFetchIconLoader::DidFail(const ResourceError& error) {
  RunCallbackWithEmptyBitmap();
}

void BackgroundFetchIconLoader::DidFailRedirectCheck() {
  RunCallbackWithEmptyBitmap();
}

void BackgroundFetchIconLoader::RunCallbackWithEmptyBitmap() {
  // If this has been stopped it is not desirable to trigger further work,
  // there is a shutdown of some sort in progress.
  if (stopped_)
    return;

  std::move(icon_callback_).Run(SkBitmap());
}

}  // namespace blink
