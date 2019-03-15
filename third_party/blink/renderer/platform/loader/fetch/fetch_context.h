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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_loading_behavior_flag.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

enum class ResourceType : uint8_t;
class ClientHintsPreferences;
class KURL;
class PlatformProbeSink;
class ResourceError;
class ResourceFetcherProperties;
class ResourceResponse;
class ResourceTimingInfo;
class WebScopedVirtualTimePauser;

// The FetchContext is an interface for performing context specific processing
// in response to events in the ResourceFetcher. The ResourceFetcher or its job
// class, ResourceLoader, may call the methods on a FetchContext.
//
// Any processing that depends on components outside platform/loader/fetch/
// should be implemented on a subclass of this interface, and then exposed to
// the ResourceFetcher via this interface.
class PLATFORM_EXPORT FetchContext
    : public GarbageCollectedFinalized<FetchContext> {
 public:
  FetchContext();

  static FetchContext& NullInstance();

  virtual ~FetchContext() = default;

  // Called from a ResourceFetcher constructor. This is called only once.
  // TODO(yhirano): Consider removing this.
  void Init(const ResourceFetcherProperties& properties) {
    DCHECK(!resource_fetcher_properties_);
    resource_fetcher_properties_ = &properties;
  }

  virtual void Trace(blink::Visitor*);

  virtual void AddAdditionalRequestHeaders(ResourceRequest&);

  // This function must not be called before |Init| is called.
  const ResourceFetcherProperties& GetResourceFetcherProperties() const {
    DCHECK(resource_fetcher_properties_);
    return *resource_fetcher_properties_;
  }

  // Returns the cache policy for the resource. ResourceRequest is not passed as
  // a const reference as a header needs to be added for doc.write blocking
  // intervention.
  virtual mojom::FetchCacheMode ResourceRequestCachePolicy(
      const ResourceRequest&,
      ResourceType,
      FetchParameters::DeferOption) const;

  virtual void DispatchDidChangeResourcePriority(unsigned long identifier,
                                                 ResourceLoadPriority,
                                                 int intra_priority_value);

  // This internally dispatches WebLocalFrameClient::WillSendRequest and hooks
  // request interceptors like ServiceWorker and ApplicationCache.
  // This may modify the request.
  // |virtual_time_pauser| is an output parameter. PrepareRequest may
  // create a new WebScopedVirtualTimePauser and set it to
  // |virtual_time_pauser|.
  // This is called on initial and every redirect request.
  virtual void PrepareRequest(ResourceRequest&,
                              const FetchInitiatorInfo&,
                              WebScopedVirtualTimePauser& virtual_time_pauser,
                              ResourceType);

  // The last callback before a request is actually sent to the browser process.
  // This is called on initial and every redirect request.
  virtual void DispatchWillSendRequest(
      unsigned long identifier,
      const ResourceRequest&,
      const ResourceResponse& redirect_response,
      ResourceType,
      const FetchInitiatorInfo& = FetchInitiatorInfo());
  enum class ResourceResponseType { kNotFromMemoryCache, kFromMemoryCache };
  // |request| and |resource| are provided separately because when it's from
  // the memory cache |request| and |resource->GetResourceRequest()| don't
  // match. |response| may not yet be set to |resource| when this function is
  // called.
  virtual void DispatchDidReceiveResponse(unsigned long identifier,
                                          const ResourceRequest& request,
                                          const ResourceResponse& response,
                                          Resource* resource,
                                          ResourceResponseType);
  virtual void DispatchDidReceiveData(unsigned long identifier,
                                      const char* data,
                                      uint64_t data_length);
  virtual void DispatchDidReceiveEncodedData(unsigned long identifier,
                                             size_t encoded_data_length);
  virtual void DispatchDidDownloadToBlob(unsigned long identifier,
                                         BlobDataHandle*);
  virtual void DispatchDidFinishLoading(unsigned long identifier,
                                        TimeTicks finish_time,
                                        int64_t encoded_data_length,
                                        int64_t decoded_body_length,
                                        bool should_report_corb_blocking,
                                        ResourceResponseType);
  virtual void DispatchDidFail(const KURL&,
                               unsigned long identifier,
                               const ResourceError&,
                               int64_t encoded_data_length,
                               bool is_internal_request);

  bool ShouldLoadNewResource(ResourceType) const;

  // Called when a resource load is first requested, which may not be when the
  // load actually begins.
  virtual void RecordLoadingActivity(const ResourceRequest&,
                                     ResourceType,
                                     const AtomicString& fetch_initiator_name);

  virtual void DidObserveLoadingBehavior(WebLoadingBehaviorFlag);

  virtual void AddResourceTiming(const ResourceTimingInfo&);
  virtual bool AllowImage(bool, const KURL&) const { return false; }
  virtual base::Optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus) const {
    return ResourceRequestBlockedReason::kOther;
  }
  virtual base::Optional<ResourceRequestBlockedReason> CheckCSPForRequest(
      mojom::RequestContextType,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus) const {
    return ResourceRequestBlockedReason::kOther;
  }

  virtual void CountUsage(mojom::WebFeature) const = 0;
  virtual void CountDeprecation(mojom::WebFeature) const = 0;

  // Populates the ResourceRequest using the given values and information
  // stored in the FetchContext implementation. Used by ResourceFetcher to
  // prepare a ResourceRequest instance at the start of resource loading.
  virtual void PopulateResourceRequest(ResourceType,
                                       const ClientHintsPreferences&,
                                       const FetchParameters::ResourceWidth&,
                                       ResourceRequest&);

  PlatformProbeSink* GetPlatformProbeSink() const {
    return platform_probe_sink_;
  }

  // Called when the underlying context is detached. Note that some
  // FetchContexts continue working after detached (e.g., for fetch() operations
  // with "keepalive" specified).
  // Returns a "detached" fetch context which cannot be null.
  virtual FetchContext* Detach() {
    DCHECK(resource_fetcher_properties_);
    auto* context = &NullInstance();
    context->Init(*resource_fetcher_properties_);
    return context;
  }

  // Returns the updated priority of the resource based on the experiments that
  // may be currently enabled.
  virtual ResourceLoadPriority ModifyPriorityForExperiments(
      ResourceLoadPriority priority) const {
    return priority;
  }

  // Determine if the request is on behalf of an advertisement. If so, return
  // true.
  virtual bool CalculateIfAdSubresource(const ResourceRequest& resource_request,
                                        ResourceType type) {
    return false;
  }

  // Called when IdlenessDetector emits its network idle signal.
  virtual void DispatchNetworkQuiet() {}

 private:
  Member<PlatformProbeSink> platform_probe_sink_;
  Member<const ResourceFetcherProperties> resource_fetcher_properties_;

  DISALLOW_COPY_AND_ASSIGN(FetchContext);
};

}  // namespace blink

#endif
