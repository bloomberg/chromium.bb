// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CoreProbeSink;
class Resource;
class SubresourceFilter;
class WebWorkerFetchContext;
class WorkerContentSettingsClient;
class WorkerSettings;
class WorkerOrWorkletGlobalScope;
enum class ResourceType : uint8_t;

// The WorkerFetchContext is a FetchContext for workers (dedicated, shared and
// service workers) and threaded worklets (animation and audio worklets).
//
// Separate WorkerFetchContext objects (and separate ResourceFetcher objects)
// are used for each of insideSettings fetch and outsideSettings fetches.
// For more details, see core/workers/README.md.
class WorkerFetchContext final : public BaseFetchContext {
 public:
  WorkerFetchContext(WorkerOrWorkletGlobalScope&,
                     scoped_refptr<WebWorkerFetchContext>,
                     SubresourceFilter*,
                     ContentSecurityPolicy&);
  ~WorkerFetchContext() override;

  // BaseFetchContext implementation:
  KURL GetSiteForCookies() const override;
  scoped_refptr<const SecurityOrigin> GetTopFrameOrigin() const override;

  SubresourceFilter* GetSubresourceFilter() const override;
  PreviewsResourceLoadingHints* GetPreviewsResourceLoadingHints()
      const override;
  bool AllowScriptFromSource(const KURL&) const override;
  bool ShouldBlockRequestByInspector(const KURL&) const override;
  void DispatchDidBlockRequest(const ResourceRequest&,
                               const FetchInitiatorInfo&,
                               ResourceRequestBlockedReason,
                               ResourceType) const override;
  bool ShouldBypassMainWorldCSP() const override;
  bool IsSVGImageChromeClient() const override;
  void CountUsage(WebFeature) const override;
  void CountDeprecation(WebFeature) const override;
  bool ShouldBlockWebSocketByMixedContentCheck(const KURL&) const override;
  std::unique_ptr<WebSocketHandshakeThrottle> CreateWebSocketHandshakeThrottle()
      override;
  bool ShouldBlockFetchByMixedContentCheck(
      mojom::RequestContextType,
      network::mojom::RequestContextFrameType,
      ResourceRequest::RedirectStatus,
      const KURL&,
      SecurityViolationReportingPolicy) const override;
  bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                 const KURL&) const override;
  const KURL& Url() const override;
  const SecurityOrigin* GetParentSecurityOrigin() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  // FetchContext implementation:
  void PrepareRequest(ResourceRequest&,
                      const FetchInitiatorInfo&,
                      WebScopedVirtualTimePauser&,
                      ResourceType) override;
  void AddAdditionalRequestHeaders(ResourceRequest&) override;
  void DispatchWillSendRequest(unsigned long,
                               const ResourceRequest&,
                               const ResourceResponse&,
                               ResourceType,
                               const FetchInitiatorInfo&) override;
  void DispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceRequest&,
                                  const ResourceResponse&,
                                  Resource*,
                                  ResourceResponseType) override;
  void DispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              uint64_t data_length) override;
  void DispatchDidReceiveEncodedData(unsigned long identifier,
                                     size_t encoded_data_length) override;
  void DispatchDidFinishLoading(unsigned long identifier,
                                TimeTicks finish_time,
                                int64_t encoded_data_length,
                                int64_t decoded_body_length,
                                bool should_report_corb_blocking,
                                ResourceResponseType) override;
  void DispatchDidFail(const KURL&,
                       unsigned long identifier,
                       const ResourceError&,
                       int64_t encoded_data_length,
                       bool isInternalRequest) override;
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void PopulateResourceRequest(ResourceType,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;

  SecurityContext& GetSecurityContext() const;
  WorkerSettings* GetWorkerSettings() const;
  WorkerContentSettingsClient* GetWorkerContentSettingsClient() const;
  WebWorkerFetchContext* GetWebWorkerFetchContext() const {
    return web_context_.get();
  }

  void Trace(blink::Visitor*) override;

 private:
  void SetFirstPartyCookie(ResourceRequest&);

  CoreProbeSink* Probe() const;

  const Member<WorkerOrWorkletGlobalScope> global_scope_;

  const scoped_refptr<WebWorkerFetchContext> web_context_;
  Member<SubresourceFilter> subresource_filter_;

  // In case of insideSettings fetch (=subresource fetch), this is
  // WorkerGlobalScope::GetContentSecurityPolicy().
  // In case of outsideSettings fetch (=off-the-main-thread top-level script
  // fetch), this is a ContentSecurityPolicy different from
  // WorkerGlobalScope::GetContentSecurityPolicy(), not bound to
  // WorkerGlobalScope and owned by this WorkerFetchContext.
  const Member<ContentSecurityPolicy> content_security_policy_;

  // The value of |save_data_enabled_| is read once per frame from
  // NetworkStateNotifier, which is guarded by a mutex lock, and cached locally
  // here for performance.
  const bool save_data_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_
