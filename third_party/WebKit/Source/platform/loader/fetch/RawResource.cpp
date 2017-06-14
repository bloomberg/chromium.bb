/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/loader/fetch/RawResource.h"

#include <memory>
#include "platform/HTTPNames.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"

namespace blink {

RawResource* RawResource::FetchSynchronously(FetchParameters& params,
                                             ResourceFetcher* fetcher) {
  params.MakeSynchronous();
  return ToRawResource(
      fetcher->RequestResource(params, RawResourceFactory(Resource::kRaw)));
}

RawResource* RawResource::FetchImport(FetchParameters& params,
                                      ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  params.SetRequestContext(WebURLRequest::kRequestContextImport);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(Resource::kImportResource)));
}

RawResource* RawResource::Fetch(FetchParameters& params,
                                ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  DCHECK_NE(params.GetResourceRequest().GetRequestContext(),
            WebURLRequest::kRequestContextUnspecified);
  return ToRawResource(
      fetcher->RequestResource(params, RawResourceFactory(Resource::kRaw)));
}

RawResource* RawResource::FetchMainResource(
    FetchParameters& params,
    ResourceFetcher* fetcher,
    const SubstituteData& substitute_data) {
  DCHECK_NE(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  DCHECK(params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextForm ||
         params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextFrame ||
         params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextHyperlink ||
         params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextIframe ||
         params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextInternal ||
         params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextLocation);

  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(Resource::kMainResource), substitute_data));
}

RawResource* RawResource::FetchMedia(FetchParameters& params,
                                     ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  DCHECK(params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextAudio ||
         params.GetResourceRequest().GetRequestContext() ==
             WebURLRequest::kRequestContextVideo);
  return ToRawResource(
      fetcher->RequestResource(params, RawResourceFactory(Resource::kMedia)));
}

RawResource* RawResource::FetchTextTrack(FetchParameters& params,
                                         ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  params.SetRequestContext(WebURLRequest::kRequestContextTrack);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(Resource::kTextTrack)));
}

RawResource* RawResource::FetchManifest(FetchParameters& params,
                                        ResourceFetcher* fetcher) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            WebURLRequest::kFrameTypeNone);
  DCHECK_EQ(params.GetResourceRequest().GetRequestContext(),
            WebURLRequest::kRequestContextManifest);
  return ToRawResource(fetcher->RequestResource(
      params, RawResourceFactory(Resource::kManifest)));
}

RawResource::RawResource(const ResourceRequest& resource_request,
                         Type type,
                         const ResourceLoaderOptions& options)
    : Resource(resource_request, type, options) {}

void RawResource::AppendData(const char* data, size_t length) {
  Resource::AppendData(data, length);

  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DataReceived(this, data, length);
}

void RawResource::DidAddClient(ResourceClient* c) {
  // CHECK()/RevalidationStartForbiddenScope are for
  // https://crbug.com/640960#c24.
  CHECK(!IsCacheValidator());
  if (!HasClient(c))
    return;
  DCHECK(RawResourceClient::IsExpectedType(c));
  RevalidationStartForbiddenScope revalidation_start_forbidden_scope(this);
  RawResourceClient* client = static_cast<RawResourceClient*>(c);
  for (const auto& redirect : RedirectChain()) {
    ResourceRequest request(redirect.request_);
    client->RedirectReceived(this, request, redirect.redirect_response_);
    if (!HasClient(c))
      return;
  }

  if (!GetResponse().IsNull())
    client->ResponseReceived(this, GetResponse(), nullptr);
  if (!HasClient(c))
    return;
  if (Data()) {
    Data()->ForEachSegment([this, &client](const char* segment,
                                           size_t segment_size,
                                           size_t segment_offset) -> bool {
      client->DataReceived(this, segment, segment_size);

      // Stop pushing data if the client removed itself.
      return HasClient(client);
    });
  }
  if (!HasClient(c))
    return;
  Resource::DidAddClient(client);
}

bool RawResource::WillFollowRedirect(
    const ResourceRequest& new_request,
    const ResourceResponse& redirect_response) {
  bool follow = Resource::WillFollowRedirect(new_request, redirect_response);
  // The base class method takes a const reference of a ResourceRequest and
  // returns bool just for allowing RawResource to reject redirect. It must
  // always return true.
  DCHECK(follow);

  DCHECK(!redirect_response.IsNull());
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next()) {
    if (!c->RedirectReceived(this, new_request, redirect_response))
      follow = false;
  }

  return follow;
}

void RawResource::WillNotFollowRedirect() {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->RedirectBlocked();
}

void RawResource::ResponseReceived(
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  if (response.WasFallbackRequiredByServiceWorker()) {
    // The ServiceWorker asked us to re-fetch the request. This resource must
    // not be reused.
    // Note: This logic is needed here because DocumentThreadableLoader handles
    // CORS independently from ResourceLoader. Fix it.
    GetMemoryCache()->Remove(this);
  }

  bool is_successful_revalidation =
      IsCacheValidator() && response.HttpStatusCode() == 304;
  Resource::ResponseReceived(response, nullptr);

  ResourceClientWalker<RawResourceClient> w(Clients());
  DCHECK(Clients().size() <= 1 || !handle);
  while (RawResourceClient* c = w.Next()) {
    // |handle| is cleared when passed, but it's not a problem because |handle|
    // is null when there are two or more clients, as asserted.
    c->ResponseReceived(this, this->GetResponse(), std::move(handle));
  }

  // If we successfully revalidated, we won't get appendData() calls. Forward
  // the data to clients now instead. Note: |m_data| can be null when no data is
  // appended to the original resource.
  if (is_successful_revalidation && Data()) {
    ResourceClientWalker<RawResourceClient> w(Clients());
    while (RawResourceClient* c = w.Next())
      c->DataReceived(this, Data()->Data(), Data()->size());
  }
}

void RawResource::SetSerializedCachedMetadata(const char* data, size_t size) {
  Resource::SetSerializedCachedMetadata(data, size);
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->SetSerializedCachedMetadata(this, data, size);
}

void RawResource::DidSendData(unsigned long long bytes_sent,
                              unsigned long long total_bytes_to_be_sent) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DataSent(this, bytes_sent, total_bytes_to_be_sent);
}

void RawResource::DidDownloadData(int data_length) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DataDownloaded(this, data_length);
}

void RawResource::ReportResourceTimingToClients(
    const ResourceTimingInfo& info) {
  ResourceClientWalker<RawResourceClient> w(Clients());
  while (RawResourceClient* c = w.Next())
    c->DidReceiveResourceTiming(this, info);
}

void RawResource::SetDefersLoading(bool defers) {
  if (Loader())
    Loader()->SetDefersLoading(defers);
}

static bool ShouldIgnoreHeaderForCacheReuse(AtomicString header_name) {
  // FIXME: This list of headers that don't affect cache policy almost certainly
  // isn't complete.
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, headers,
      ({"Cache-Control", "If-Modified-Since", "If-None-Match", "Origin",
        "Pragma", "Purpose", "Referer", "User-Agent",
        HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id}));
  return headers.Contains(header_name);
}

static bool IsCacheableHTTPMethod(const AtomicString& method) {
  // Per http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html#sec13.10,
  // these methods always invalidate the cache entry.
  return method != "POST" && method != "PUT" && method != "DELETE";
}

bool RawResource::CanReuse(const FetchParameters& new_fetch_parameters) const {
  const ResourceRequest& new_request =
      new_fetch_parameters.GetResourceRequest();

  if (GetDataBufferingPolicy() == kDoNotBufferData)
    return false;

  if (!IsCacheableHTTPMethod(GetResourceRequest().HttpMethod()))
    return false;
  if (GetResourceRequest().HttpMethod() != new_request.HttpMethod())
    return false;

  if (GetResourceRequest().HttpBody() != new_request.HttpBody())
    return false;

  if (GetResourceRequest().AllowStoredCredentials() !=
      new_request.AllowStoredCredentials())
    return false;

  // Ensure most headers match the existing headers before continuing. Note that
  // the list of ignored headers includes some headers explicitly related to
  // caching. A more detailed check of caching policy will be performed later,
  // this is simply a list of headers that we might permit to be different and
  // still reuse the existing Resource.
  const HTTPHeaderMap& new_headers = new_request.HttpHeaderFields();
  const HTTPHeaderMap& old_headers = GetResourceRequest().HttpHeaderFields();

  for (const auto& header : new_headers) {
    AtomicString header_name = header.key;
    if (!ShouldIgnoreHeaderForCacheReuse(header_name) &&
        header.value != old_headers.Get(header_name))
      return false;
  }

  for (const auto& header : old_headers) {
    AtomicString header_name = header.key;
    if (!ShouldIgnoreHeaderForCacheReuse(header_name) &&
        header.value != new_headers.Get(header_name))
      return false;
  }

  return true;
}

RawResourceClientStateChecker::RawResourceClientStateChecker()
    : state_(kNotAddedAsClient) {}

RawResourceClientStateChecker::~RawResourceClientStateChecker() {}

NEVER_INLINE void RawResourceClientStateChecker::WillAddClient() {
  SECURITY_CHECK(state_ == kNotAddedAsClient);
  state_ = kStarted;
}

NEVER_INLINE void RawResourceClientStateChecker::WillRemoveClient() {
  SECURITY_CHECK(state_ != kNotAddedAsClient);
  state_ = kNotAddedAsClient;
}

NEVER_INLINE void RawResourceClientStateChecker::RedirectReceived() {
  SECURITY_CHECK(state_ == kStarted);
}

NEVER_INLINE void RawResourceClientStateChecker::RedirectBlocked() {
  SECURITY_CHECK(state_ == kStarted);
  state_ = kRedirectBlocked;
}

NEVER_INLINE void RawResourceClientStateChecker::DataSent() {
  SECURITY_CHECK(state_ == kStarted);
}

NEVER_INLINE void RawResourceClientStateChecker::ResponseReceived() {
  SECURITY_CHECK(state_ == kStarted);
  state_ = kResponseReceived;
}

NEVER_INLINE void RawResourceClientStateChecker::SetSerializedCachedMetadata() {
  SECURITY_CHECK(state_ == kResponseReceived);
  state_ = kSetSerializedCachedMetadata;
}

NEVER_INLINE void RawResourceClientStateChecker::DataReceived() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kSetSerializedCachedMetadata ||
                 state_ == kDataReceived);
  state_ = kDataReceived;
}

NEVER_INLINE void RawResourceClientStateChecker::DataDownloaded() {
  SECURITY_CHECK(state_ == kResponseReceived ||
                 state_ == kSetSerializedCachedMetadata ||
                 state_ == kDataDownloaded);
  state_ = kDataDownloaded;
}

NEVER_INLINE void RawResourceClientStateChecker::NotifyFinished(
    Resource* resource) {
  SECURITY_CHECK(state_ != kNotAddedAsClient);
  SECURITY_CHECK(state_ != kNotifyFinished);
  SECURITY_CHECK(resource->ErrorOccurred() ||
                 (state_ == kResponseReceived ||
                  state_ == kSetSerializedCachedMetadata ||
                  state_ == kDataReceived || state_ == kDataDownloaded));
  state_ = kNotifyFinished;
}

}  // namespace blink
