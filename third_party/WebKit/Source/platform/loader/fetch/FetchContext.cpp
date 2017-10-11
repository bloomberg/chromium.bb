/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/loader/fetch/FetchContext.h"

#include "platform/PlatformProbeSink.h"
#include "platform/probe/PlatformTraceEventsAgent.h"
#include "public/platform/WebCachePolicy.h"

namespace blink {

FetchContext& FetchContext::NullInstance() {
  DEFINE_STATIC_LOCAL(FetchContext, instance, (new FetchContext));
  return instance;
}

FetchContext::FetchContext() : platform_probe_sink_(new PlatformProbeSink) {
  platform_probe_sink_->addPlatformTraceEventsAgent(
      new PlatformTraceEventsAgent);
}

DEFINE_TRACE(FetchContext) {
  visitor->Trace(platform_probe_sink_);
}

void FetchContext::DispatchDidChangeResourcePriority(unsigned long,
                                                     ResourceLoadPriority,
                                                     int) {}

void FetchContext::AddAdditionalRequestHeaders(ResourceRequest&,
                                               FetchResourceType) {}

WebCachePolicy FetchContext::ResourceRequestCachePolicy(
    const ResourceRequest&,
    Resource::Type,
    FetchParameters::DeferOption defer) const {
  return WebCachePolicy::kUseProtocolCachePolicy;
}

void FetchContext::PrepareRequest(ResourceRequest&, RedirectType) {}

void FetchContext::DispatchWillSendRequest(unsigned long,
                                           ResourceRequest&,
                                           const ResourceResponse&,
                                           Resource::Type,
                                           const FetchInitiatorInfo&) {}

void FetchContext::DispatchDidLoadResourceFromMemoryCache(
    unsigned long,
    const ResourceRequest&,
    const ResourceResponse&) {}

void FetchContext::DispatchDidReceiveResponse(unsigned long,
                                              const ResourceResponse&,
                                              WebURLRequest::FrameType,
                                              WebURLRequest::RequestContext,
                                              Resource*,
                                              ResourceResponseType) {}

void FetchContext::DispatchDidReceiveData(unsigned long, const char*, int) {}

void FetchContext::DispatchDidReceiveEncodedData(unsigned long, int) {}

void FetchContext::DispatchDidDownloadData(unsigned long, int, int) {}

void FetchContext::DispatchDidFinishLoading(unsigned long,
                                            double,
                                            int64_t,
                                            int64_t) {}

void FetchContext::DispatchDidFail(unsigned long,
                                   const ResourceError&,
                                   int64_t,
                                   bool) {}

void FetchContext::RecordLoadingActivity(
    unsigned long,
    const ResourceRequest&,
    Resource::Type,
    const AtomicString& fetch_initiator_name) {}

void FetchContext::DidLoadResource(Resource*) {}

void FetchContext::AddResourceTiming(const ResourceTimingInfo&) {}

void FetchContext::SendImagePing(const KURL&) {}

void FetchContext::AddWarningConsoleMessage(const String&, LogSource) const {}

void FetchContext::AddErrorConsoleMessage(const String&, LogSource) const {}

void FetchContext::PopulateResourceRequest(
    Resource::Type,
    const ClientHintsPreferences&,
    const FetchParameters::ResourceWidth&,
    ResourceRequest&) {}

void FetchContext::SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&) {}

}  // namespace blink
