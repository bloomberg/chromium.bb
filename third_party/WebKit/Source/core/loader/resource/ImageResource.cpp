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

#include <stdint.h>
#include <v8.h>
#include <memory>

#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceInfo.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/StdLibExtras.h"
#include "public/platform/Platform.h"
#include "v8/include/v8.h"

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
  ImageResourceInfoImpl(ImageResource* resource) : resource_(resource) {
    DCHECK(resource_);
  }
  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(resource_);
    ImageResourceInfo::Trace(visitor);
  }

 private:
  const KURL& Url() const override { return resource_->Url(); }
  bool IsSchedulingReload() const override {
    return resource_->is_scheduling_reload_;
  }
  bool HasDevicePixelRatioHeaderValue() const override {
    return resource_->has_device_pixel_ratio_header_value_;
  }
  float DevicePixelRatioHeaderValue() const override {
    return resource_->device_pixel_ratio_header_value_;
  }
  const ResourceResponse& GetResponse() const override {
    return resource_->GetResponse();
  }
  bool ShouldShowPlaceholder() const override {
    return resource_->ShouldShowPlaceholder();
  }
  bool IsCacheValidator() const override {
    return resource_->IsCacheValidator();
  }
  bool SchedulingReloadOrShouldReloadBrokenPlaceholder() const override {
    return resource_->is_scheduling_reload_ ||
           resource_->ShouldReloadBrokenPlaceholder();
  }
  bool IsAccessAllowed(
      SecurityOrigin* security_origin,
      DoesCurrentFrameHaveSingleSecurityOrigin
          does_current_frame_has_single_security_origin) const override {
    return resource_->IsAccessAllowed(
        security_origin, does_current_frame_has_single_security_origin);
  }
  bool HasCacheControlNoStoreHeader() const override {
    return resource_->HasCacheControlNoStoreHeader();
  }
  const ResourceError& GetResourceError() const override {
    return resource_->GetResourceError();
  }

  void SetDecodedSize(size_t size) override { resource_->SetDecodedSize(size); }
  void WillAddClientOrObserver() override {
    resource_->WillAddClientOrObserver(Resource::kMarkAsReferenced);
  }
  void DidRemoveClientOrObserver() override {
    resource_->DidRemoveClientOrObserver();
  }
  void EmulateLoadStartedForInspector(
      ResourceFetcher* fetcher,
      const KURL& url,
      const AtomicString& initiator_name) override {
    fetcher->EmulateLoadStartedForInspector(resource_.Get(), url,
                                            WebURLRequest::kRequestContextImage,
                                            initiator_name);
  }

  const Member<ImageResource> resource_;
};

class ImageResource::ImageResourceFactory : public ResourceFactory {
  STACK_ALLOCATED();

 public:
  ImageResourceFactory(const FetchParameters& fetch_params)
      : ResourceFactory(Resource::kImage), fetch_params_(&fetch_params) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options,
                   const String&) const override {
    return new ImageResource(request, options,
                             ImageResourceContent::CreateNotStarted(),
                             fetch_params_->GetPlaceholderImageRequestType() ==
                                 FetchParameters::kAllowPlaceholder);
  }

 private:
  // Weak, unowned pointer. Must outlive |this|.
  const FetchParameters* fetch_params_;
};

ImageResource* ImageResource::Fetch(FetchParameters& params,
                                    ResourceFetcher* fetcher) {
  if (params.GetResourceRequest().GetRequestContext() ==
      WebURLRequest::kRequestContextUnspecified) {
    params.SetRequestContext(WebURLRequest::kRequestContextImage);
  }
  if (fetcher->Context().PageDismissalEventBeingDispatched()) {
    KURL request_url = params.GetResourceRequest().Url();
    if (request_url.IsValid()) {
      ResourceRequestBlockedReason block_reason = fetcher->Context().CanRequest(
          Resource::kImage, params.GetResourceRequest(), request_url,
          params.Options(),
          /* Don't send security violation reports for speculative preloads */
          params.IsSpeculativePreload()
              ? SecurityViolationReportingPolicy::kSuppressReporting
              : SecurityViolationReportingPolicy::kReport,
          params.GetOriginRestriction());
      if (block_reason == ResourceRequestBlockedReason::kNone)
        fetcher->Context().SendImagePing(request_url);
    }
    return nullptr;
  }

  return ToImageResource(
      fetcher->RequestResource(params, ImageResourceFactory(params)));
}

bool ImageResource::CanReuse(const FetchParameters& params) const {
  // If the image is a placeholder, but this fetch doesn't allow a
  // placeholder, then do not reuse this resource.
  if (params.GetPlaceholderImageRequestType() !=
          FetchParameters::kAllowPlaceholder &&
      placeholder_option_ != PlaceholderOption::kDoNotReloadPlaceholder)
    return false;
  return true;
}

bool ImageResource::CanUseCacheValidator() const {
  // Disable revalidation while ImageResourceContent is still waiting for
  // SVG load completion.
  // TODO(hiroshige): Clean up revalidation-related dependencies.
  if (!GetContent()->IsLoaded())
    return false;

  return Resource::CanUseCacheValidator();
}

ImageResource* ImageResource::Create(const ResourceRequest& request) {
  return new ImageResource(request, ResourceLoaderOptions(),
                           ImageResourceContent::CreateNotStarted(), false);
}

ImageResource::ImageResource(const ResourceRequest& resource_request,
                             const ResourceLoaderOptions& options,
                             ImageResourceContent* content,
                             bool is_placeholder)
    : Resource(resource_request, kImage, options),
      content_(content),
      device_pixel_ratio_header_value_(1.0),
      has_device_pixel_ratio_header_value_(false),
      is_scheduling_reload_(false),
      placeholder_option_(
          is_placeholder ? PlaceholderOption::kShowAndReloadPlaceholderAlways
                         : PlaceholderOption::kDoNotReloadPlaceholder),
      flush_timer_(this, &ImageResource::FlushImageIfNeeded) {
  DCHECK(GetContent());
  RESOURCE_LOADING_DVLOG(1) << "new ImageResource(ResourceRequest) " << this;
  GetContent()->SetImageResourceInfo(new ImageResourceInfoImpl(this));
}

ImageResource::~ImageResource() {
  RESOURCE_LOADING_DVLOG(1) << "~ImageResource " << this;
}

DEFINE_TRACE(ImageResource) {
  visitor->Trace(multipart_parser_);
  visitor->Trace(content_);
  Resource::Trace(visitor);
  MultipartImageResourceParser::Client::Trace(visitor);
}

void ImageResource::CheckNotify() {
  // Don't notify clients of completion if this ImageResource is
  // about to be reloaded.
  if (is_scheduling_reload_ || ShouldReloadBrokenPlaceholder())
    return;

  Resource::CheckNotify();
}

bool ImageResource::HasClientsOrObservers() const {
  return Resource::HasClientsOrObservers() || GetContent()->HasObservers();
}

void ImageResource::DidAddClient(ResourceClient* client) {
  DCHECK((multipart_parser_ && IsLoading()) || !Data() ||
         GetContent()->HasImage());

  // Don't notify observers and clients of completion if this ImageResource is
  // about to be reloaded.
  if (is_scheduling_reload_ || ShouldReloadBrokenPlaceholder())
    return;

  Resource::DidAddClient(client);
}

void ImageResource::DestroyDecodedDataForFailedRevalidation() {
  // Clears the image, as we must create a new image for the failed
  // revalidation response.
  UpdateImage(nullptr, ImageResourceContent::kClearAndUpdateImage, false);
  SetDecodedSize(0);
}

void ImageResource::DestroyDecodedDataIfPossible() {
  GetContent()->DestroyDecodedData();
  if (GetContent()->HasImage() && !IsPreloaded() &&
      GetContent()->IsRefetchableDataFromDiskCache()) {
    UMA_HISTOGRAM_MEMORY_KB("Memory.Renderer.EstimatedDroppableEncodedSize",
                            EncodedSize() / 1024);
  }
}

void ImageResource::AllClientsAndObserversRemoved() {
  CHECK(!GetContent()->HasImage() || !ErrorOccurred());
  // If possible, delay the resetting until back at the event loop. Doing so
  // after a conservative GC prevents resetAnimation() from upsetting ongoing
  // animation updates (crbug.com/613709)
  if (!ThreadHeap::WillObjectBeLazilySwept(this)) {
    Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, WTF::Bind(&ImageResourceContent::DoResetAnimation,
                                   WrapWeakPersistent(GetContent())));
  } else {
    GetContent()->DoResetAnimation();
  }
  if (multipart_parser_)
    multipart_parser_->Cancel();
  Resource::AllClientsAndObserversRemoved();
}

PassRefPtr<const SharedBuffer> ImageResource::ResourceBuffer() const {
  if (Data())
    return Data();
  return GetContent()->ResourceBuffer();
}

void ImageResource::AppendData(const char* data, size_t length) {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(length);
  if (multipart_parser_) {
    multipart_parser_->AppendData(data, length);
  } else {
    Resource::AppendData(data, length);

    // Update the image immediately if needed.
    if (GetContent()->ShouldUpdateImageImmediately()) {
      UpdateImage(this->Data(), ImageResourceContent::kUpdateImage, false);
      return;
    }

    // For other cases, only update at |kFlushDelaySeconds| intervals. This
    // throttles how frequently we update |m_image| and how frequently we
    // inform the clients which causes an invalidation of this image. In other
    // words, we only invalidate this image every |kFlushDelaySeconds| seconds
    // while loading.
    if (!flush_timer_.IsActive()) {
      double now = WTF::MonotonicallyIncreasingTime();
      if (!last_flush_time_)
        last_flush_time_ = now;

      DCHECK_LE(last_flush_time_, now);
      double flush_delay = last_flush_time_ - now + kFlushDelaySeconds;
      if (flush_delay < 0.)
        flush_delay = 0.;
      flush_timer_.StartOneShot(flush_delay, BLINK_FROM_HERE);
    }
  }
}

void ImageResource::FlushImageIfNeeded(TimerBase*) {
  // We might have already loaded the image fully, in which case we don't need
  // to call |updateImage()|.
  if (IsLoading()) {
    last_flush_time_ = WTF::MonotonicallyIncreasingTime();
    UpdateImage(this->Data(), ImageResourceContent::kUpdateImage, false);
  }
}

void ImageResource::DecodeError(bool all_data_received) {
  size_t size = EncodedSize();

  ClearData();
  SetEncodedSize(0);
  if (!ErrorOccurred())
    SetStatus(ResourceStatus::kDecodeError);

  // Finishes loading if needed, and notifies observers.
  if (!all_data_received && Loader()) {
    // Observers are notified via ImageResource::finish().
    // TODO(hiroshige): Do not call didFinishLoading() directly.
    Loader()->DidFinishLoading(MonotonicallyIncreasingTime(), size, size, size);
  } else {
    auto result = GetContent()->UpdateImage(
        nullptr, GetStatus(),
        ImageResourceContent::kClearImageAndNotifyObservers, all_data_received);
    DCHECK_EQ(result, ImageResourceContent::UpdateImageResult::kNoDecodeError);
  }

  GetMemoryCache()->Remove(this);
}

void ImageResource::UpdateImageAndClearBuffer() {
  UpdateImage(Data(), ImageResourceContent::kClearAndUpdateImage, true);
  ClearData();
}

void ImageResource::NotifyStartLoad() {
  CHECK_EQ(GetStatus(), ResourceStatus::kPending);
  GetContent()->NotifyStartLoad();
}

void ImageResource::Finish(double load_finish_time) {
  if (multipart_parser_) {
    multipart_parser_->Finish();
    if (Data())
      UpdateImageAndClearBuffer();
  } else {
    UpdateImage(Data(), ImageResourceContent::kUpdateImage, true);
    // As encoded image data can be created from m_image  (see
    // ImageResource::resourceBuffer(), we don't have to keep m_data. Let's
    // clear this. As for the lifetimes of m_image and m_data, see this
    // document:
    // https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
    ClearData();
  }
  Resource::Finish(load_finish_time);
}

void ImageResource::FinishAsError(const ResourceError& error) {
  if (multipart_parser_)
    multipart_parser_->Cancel();
  // TODO(hiroshige): Move setEncodedSize() call to Resource::error() if it
  // is really needed, or remove it otherwise.
  SetEncodedSize(0);
  Resource::FinishAsError(error);
  UpdateImage(nullptr, ImageResourceContent::kClearImageAndNotifyObservers,
              true);
}

// Determines if |response| likely contains the entire resource for the purposes
// of determining whether or not to show a placeholder, e.g. if the server
// responded with a full 200 response or if the full image is smaller than the
// requested range.
static bool IsEntireResource(const ResourceResponse& response) {
  if (response.HttpStatusCode() != 206)
    return true;

  int64_t first_byte_position = -1, last_byte_position = -1,
          instance_length = -1;
  return ParseContentRangeHeaderFor206(
             response.HttpHeaderField("Content-Range"), &first_byte_position,
             &last_byte_position, &instance_length) &&
         first_byte_position == 0 && last_byte_position + 1 == instance_length;
}

void ImageResource::ResponseReceived(
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  DCHECK(!multipart_parser_);
  // If there's no boundary, just handle the request normally.
  if (response.IsMultipart() && !response.MultipartBoundary().IsEmpty()) {
    multipart_parser_ = new MultipartImageResourceParser(
        response, response.MultipartBoundary(), this);
  }
  Resource::ResponseReceived(response, std::move(handle));
  if (RuntimeEnabledFeatures::clientHintsEnabled()) {
    device_pixel_ratio_header_value_ =
        this->GetResponse()
            .HttpHeaderField(HTTPNames::Content_DPR)
            .ToFloat(&has_device_pixel_ratio_header_value_);
    if (!has_device_pixel_ratio_header_value_ ||
        device_pixel_ratio_header_value_ <= 0.0) {
      device_pixel_ratio_header_value_ = 1.0;
      has_device_pixel_ratio_header_value_ = false;
    }
  }

  if (placeholder_option_ ==
          PlaceholderOption::kShowAndReloadPlaceholderAlways &&
      IsEntireResource(this->GetResponse())) {
    if (this->GetResponse().HttpStatusCode() < 400 ||
        this->GetResponse().HttpStatusCode() >= 600) {
      // Don't treat a complete and broken image as a placeholder if the
      // response code is something other than a 4xx or 5xx error.
      // This is done to prevent reissuing the request in cases like
      // "204 No Content" responses to tracking requests triggered by <img>
      // tags, and <img> tags used to preload non-image resources.
      placeholder_option_ = PlaceholderOption::kDoNotReloadPlaceholder;
    } else {
      placeholder_option_ = PlaceholderOption::kReloadPlaceholderOnDecodeError;
    }
  }
}

bool ImageResource::ShouldShowPlaceholder() const {
  switch (placeholder_option_) {
    case PlaceholderOption::kShowAndReloadPlaceholderAlways:
      return true;
    case PlaceholderOption::kReloadPlaceholderOnDecodeError:
    case PlaceholderOption::kDoNotReloadPlaceholder:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ImageResource::ShouldReloadBrokenPlaceholder() const {
  switch (placeholder_option_) {
    case PlaceholderOption::kShowAndReloadPlaceholderAlways:
      return ErrorOccurred();
    case PlaceholderOption::kReloadPlaceholderOnDecodeError:
      return GetStatus() == ResourceStatus::kDecodeError;
    case PlaceholderOption::kDoNotReloadPlaceholder:
      return false;
  }
  NOTREACHED();
  return false;
}

static bool IsLoFiImage(const ImageResource& resource) {
  if (resource.IsLoaded()) {
    return resource.GetResponse()
               .HttpHeaderField("chrome-proxy-content-transform")
               .Contains("empty-image") ||
           resource.GetResponse()
               .HttpHeaderField("chrome-proxy")
               .Contains("q=low");
  }
  return resource.GetResourceRequest().GetPreviewsState() &
         WebURLRequest::kServerLoFiOn;
}

void ImageResource::ReloadIfLoFiOrPlaceholderImage(
    ResourceFetcher* fetcher,
    ReloadLoFiOrPlaceholderPolicy policy) {
  if (policy == kReloadIfNeeded && !ShouldReloadBrokenPlaceholder())
    return;

  if (placeholder_option_ == PlaceholderOption::kDoNotReloadPlaceholder &&
      !IsLoFiImage(*this))
    return;

  // Prevent clients and observers from being notified of completion while the
  // reload is being scheduled, so that e.g. canceling an existing load in
  // progress doesn't cause clients and observers to be notified of completion
  // prematurely.
  DCHECK(!is_scheduling_reload_);
  is_scheduling_reload_ = true;

  SetCachePolicyBypassingCache();

  // The reloaded image should not use any previews transformations.
  WebURLRequest::PreviewsState previews_state_for_reload =
      WebURLRequest::kPreviewsNoTransform;

  if (policy == kReloadIfNeeded && (GetResourceRequest().GetPreviewsState() &
                                    WebURLRequest::kClientLoFiOn)) {
    // If the image attempted to use Client LoFi, but encountered a decoding
    // error and is being automatically reloaded, then also set the appropriate
    // PreviewsState bit for that. This allows the embedder to count the
    // bandwidth used for this reload against the data savings of the initial
    // response.
    previews_state_for_reload |= WebURLRequest::kClientLoFiAutoReload;
  }
  SetPreviewsState(previews_state_for_reload);

  if (placeholder_option_ != PlaceholderOption::kDoNotReloadPlaceholder)
    ClearRangeRequestHeader();
  placeholder_option_ = PlaceholderOption::kDoNotReloadPlaceholder;

  if (IsLoading()) {
    Loader()->Cancel();
    // Canceling the loader causes error() to be called, which in turn calls
    // clear() and notifyObservers(), so there's no need to call these again
    // here.
  } else {
    ClearData();
    SetEncodedSize(0);
    UpdateImage(nullptr, ImageResourceContent::kClearImageAndNotifyObservers,
                false);
  }

  SetStatus(ResourceStatus::kNotStarted);

  DCHECK(is_scheduling_reload_);
  is_scheduling_reload_ = false;

  fetcher->StartLoad(this);
}

void ImageResource::OnePartInMultipartReceived(
    const ResourceResponse& response) {
  DCHECK(multipart_parser_);

  SetResponse(response);
  if (multipart_parsing_state_ == MultipartParsingState::kWaitingForFirstPart) {
    // We have nothing to do because we don't have any data.
    multipart_parsing_state_ = MultipartParsingState::kParsingFirstPart;
    return;
  }
  UpdateImageAndClearBuffer();

  if (multipart_parsing_state_ == MultipartParsingState::kParsingFirstPart) {
    multipart_parsing_state_ = MultipartParsingState::kFinishedParsingFirstPart;
    // Notify finished when the first part ends.
    if (!ErrorOccurred())
      SetStatus(ResourceStatus::kCached);
    // We notify clients and observers of finish in checkNotify() and
    // updateImageAndClearBuffer(), respectively, and they will not be
    // notified again in Resource::finish()/error().
    CheckNotify();
    if (Loader())
      Loader()->DidFinishLoadingFirstPartInMultipart();
  }
}

void ImageResource::MultipartDataReceived(const char* bytes, size_t size) {
  DCHECK(multipart_parser_);
  Resource::AppendData(bytes, size);
}

bool ImageResource::IsAccessAllowed(
    SecurityOrigin* security_origin,
    ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin
        does_current_frame_has_single_security_origin) const {
  if (GetResponse().WasFetchedViaServiceWorker()) {
    return GetResponse().ServiceWorkerResponseType() !=
           kWebServiceWorkerResponseTypeOpaque;
  }
  if (does_current_frame_has_single_security_origin !=
      ImageResourceInfo::kHasSingleSecurityOrigin)
    return false;
  if (PassesAccessControlCheck(security_origin))
    return true;
  return !security_origin->TaintsCanvas(GetResponse().Url());
}

ImageResourceContent* ImageResource::GetContent() {
  return content_;
}

const ImageResourceContent* ImageResource::GetContent() const {
  return content_;
}

ResourcePriority ImageResource::PriorityFromObservers() {
  return GetContent()->PriorityFromObservers();
}

void ImageResource::UpdateImage(
    PassRefPtr<SharedBuffer> shared_buffer,
    ImageResourceContent::UpdateImageOption update_image_option,
    bool all_data_received) {
  auto result =
      GetContent()->UpdateImage(std::move(shared_buffer), GetStatus(),
                                update_image_option, all_data_received);
  if (result == ImageResourceContent::UpdateImageResult::kShouldDecodeError) {
    // In case of decode error, we call imageNotifyFinished() iff we don't
    // initiate reloading:
    // [(a): when this is in the middle of loading, or (b): otherwise]
    // 1. The updateImage() call above doesn't call notifyObservers().
    // 2. notifyObservers(ShouldNotifyFinish) is called
    //    (a) via updateImage() called in ImageResource::finish()
    //        called via didFinishLoading() called in decodeError(), or
    //    (b) via updateImage() called in decodeError().
    //    imageNotifyFinished() is called here iff we will not initiate
    //    reloading in Step 3 due to notifyObservers()'s
    //    schedulingReloadOrShouldReloadBrokenPlaceholder() check.
    // 3. reloadIfLoFiOrPlaceholderImage() is called via ResourceFetcher
    //    (a) via didFinishLoading() called in decodeError(), or
    //    (b) after returning ImageResource::updateImage().
    DecodeError(all_data_received);
  }
}

}  // namespace blink
