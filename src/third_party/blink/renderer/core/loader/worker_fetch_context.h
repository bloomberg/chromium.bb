// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CoreProbeSink;
class SubresourceFilter;
class WebWorkerFetchContext;
class WorkerResourceTimingNotifier;
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
  WorkerFetchContext(const DetachableResourceFetcherProperties&,
                     WorkerOrWorkletGlobalScope&,
                     scoped_refptr<WebWorkerFetchContext>,
                     SubresourceFilter*,
                     ContentSecurityPolicy&,
                     WorkerResourceTimingNotifier&);
  ~WorkerFetchContext() override;

  // BaseFetchContext implementation:
  net::SiteForCookies GetSiteForCookies() const override;
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
      mojom::blink::RequestContextType request_context,
      ResourceRequest::RedirectStatus redirect_status,
      const KURL& url,
      ReportingDisposition reporting_disposition,
      const base::Optional<String>& devtools_id) const override;
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
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void PopulateResourceRequest(ResourceType,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;
  mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>
  TakePendingWorkerTimingReceiver(int request_id) override;

  WorkerSettings* GetWorkerSettings() const;
  WebWorkerFetchContext* GetWebWorkerFetchContext() const {
    return web_context_.get();
  }

  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const KURL& url) const;

  void Trace(Visitor*) override;

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

  const CrossThreadPersistent<WorkerResourceTimingNotifier>
      resource_timing_notifier_;

  // The value of |save_data_enabled_| is read once per frame from
  // NetworkStateNotifier, which is guarded by a mutex lock, and cached locally
  // here for performance.
  const bool save_data_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WORKER_FETCH_CONTEXT_H_
