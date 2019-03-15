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

#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"

#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/platform_probe_sink.h"
#include "third_party/blink/renderer/platform/probe/platform_trace_events_agent.h"

namespace blink {

namespace {

class NullFetchContext final : public FetchContext {
 public:
  NullFetchContext() = default;

  void CountUsage(mojom::WebFeature) const override {}
  void CountDeprecation(mojom::WebFeature) const override {}
};

}  // namespace

FetchContext& FetchContext::NullInstance() {
  return *MakeGarbageCollected<NullFetchContext>();
}

FetchContext::FetchContext()
    : platform_probe_sink_(MakeGarbageCollected<PlatformProbeSink>()) {
  platform_probe_sink_->AddPlatformTraceEvents(
      MakeGarbageCollected<PlatformTraceEventsAgent>());
}

void FetchContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(platform_probe_sink_);
  visitor->Trace(resource_fetcher_properties_);
}

void FetchContext::DispatchDidChangeResourcePriority(unsigned long,
                                                     ResourceLoadPriority,
                                                     int) {}

void FetchContext::AddAdditionalRequestHeaders(ResourceRequest&) {}

mojom::FetchCacheMode FetchContext::ResourceRequestCachePolicy(
    const ResourceRequest&,
    ResourceType,
    FetchParameters::DeferOption defer) const {
  return mojom::FetchCacheMode::kDefault;
}

void FetchContext::PrepareRequest(ResourceRequest&,
                                  const FetchInitiatorInfo&,
                                  WebScopedVirtualTimePauser&,
                                  ResourceType) {}

void FetchContext::DispatchWillSendRequest(unsigned long,
                                           const ResourceRequest&,
                                           const ResourceResponse&,
                                           ResourceType,
                                           const FetchInitiatorInfo&) {}

void FetchContext::DispatchDidReceiveResponse(unsigned long,
                                              const ResourceRequest&,
                                              const ResourceResponse&,
                                              Resource*,
                                              ResourceResponseType) {}

void FetchContext::DispatchDidReceiveData(unsigned long,
                                          const char*,
                                          uint64_t) {}

void FetchContext::DispatchDidReceiveEncodedData(unsigned long, size_t) {}

void FetchContext::DispatchDidDownloadToBlob(unsigned long identifier,
                                             BlobDataHandle*) {}

void FetchContext::DispatchDidFinishLoading(unsigned long,
                                            TimeTicks,
                                            int64_t,
                                            int64_t,
                                            bool,
                                            ResourceResponseType) {}

void FetchContext::DispatchDidFail(const KURL&,
                                   unsigned long,
                                   const ResourceError&,
                                   int64_t,
                                   bool) {}

bool FetchContext::ShouldLoadNewResource(ResourceType type) const {
  return !GetResourceFetcherProperties().ShouldBlockLoadingSubResource();
}

void FetchContext::RecordLoadingActivity(
    const ResourceRequest&,
    ResourceType,
    const AtomicString& fetch_initiator_name) {}

void FetchContext::DidObserveLoadingBehavior(WebLoadingBehaviorFlag) {}

void FetchContext::AddResourceTiming(const ResourceTimingInfo&) {}

void FetchContext::PopulateResourceRequest(
    ResourceType,
    const ClientHintsPreferences&,
    const FetchParameters::ResourceWidth&,
    ResourceRequest&) {}

}  // namespace blink
