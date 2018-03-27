// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerFetchContext_h
#define WorkerFetchContext_h

#include <memory>
#include "base/single_thread_task_runner.h"
#include "core/CoreExport.h"
#include "core/loader/BaseFetchContext.h"
#include "platform/wtf/Forward.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"

namespace blink {

class ResourceFetcher;
class SubresourceFilter;
class WebURLLoader;
class WebURLLoaderFactory;
class WebWorkerFetchContext;
class WorkerClients;
class WorkerOrWorkletGlobalScope;

CORE_EXPORT void ProvideWorkerFetchContextToWorker(
    WorkerClients*,
    std::unique_ptr<WebWorkerFetchContext>);

// The WorkerFetchContext is a FetchContext for workers (dedicated, shared and
// service workers) and threaded worklets (animation and audio worklets).
class WorkerFetchContext final : public BaseFetchContext {
 public:
  static WorkerFetchContext* Create(WorkerOrWorkletGlobalScope&);
  ~WorkerFetchContext() override;

  // BaseFetchContext implementation:
  KURL GetSiteForCookies() const override;
  SubresourceFilter* GetSubresourceFilter() const override;
  bool AllowScriptFromSource(const KURL&) const override;
  bool ShouldBlockRequestByInspector(const KURL&) const override;
  void DispatchDidBlockRequest(const ResourceRequest&,
                               const FetchInitiatorInfo&,
                               ResourceRequestBlockedReason,
                               Resource::Type) const override;
  bool ShouldBypassMainWorldCSP() const override;
  bool IsSVGImageChromeClient() const override;
  void CountUsage(WebFeature) const override;
  void CountDeprecation(WebFeature) const override;
  bool ShouldBlockFetchByMixedContentCheck(
      WebURLRequest::RequestContext,
      network::mojom::RequestContextFrameType,
      ResourceRequest::RedirectStatus,
      const KURL&,
      SecurityViolationReportingPolicy) const override;
  bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                 const KURL&) const override;
  bool ShouldLoadNewResource(Resource::Type) const override { return true; }
  ReferrerPolicy GetReferrerPolicy() const override;
  String GetOutgoingReferrer() const override;
  const KURL& Url() const override;
  const SecurityOrigin* GetParentSecurityOrigin() const override;
  Optional<mojom::IPAddressSpace> GetAddressSpace() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  // FetchContext implementation:
  const SecurityOrigin* GetSecurityOrigin() const override;
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      scoped_refptr<base::SingleThreadTaskRunner>,
      const ResourceLoaderOptions&) override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  bool IsControlledByServiceWorker() const override;
  int ApplicationCacheHostID() const override;
  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  void DispatchWillSendRequest(unsigned long,
                               ResourceRequest&,
                               const ResourceResponse&,
                               Resource::Type,
                               const FetchInitiatorInfo&) override;
  void DispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceResponse&,
                                  network::mojom::RequestContextFrameType,
                                  WebURLRequest::RequestContext,
                                  Resource*,
                                  ResourceResponseType) override;
  void DispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              int dataLength) override;
  void DispatchDidReceiveEncodedData(unsigned long identifier,
                                     int encoded_data_length) override;
  void DispatchDidFinishLoading(unsigned long identifier,
                                double finish_time,
                                int64_t encoded_data_length,
                                int64_t decoded_body_length,
                                bool blocked_cross_site_document) override;
  void DispatchDidFail(const KURL&,
                       unsigned long identifier,
                       const ResourceError&,
                       int64_t encoded_data_length,
                       bool isInternalRequest) override;
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void PopulateResourceRequest(Resource::Type,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner() override;

  void Trace(blink::Visitor*) override;

 private:
  WorkerFetchContext(WorkerOrWorkletGlobalScope&,
                     std::unique_ptr<WebWorkerFetchContext>);

  void SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&);

  Member<WorkerOrWorkletGlobalScope> global_scope_;
  std::unique_ptr<WebWorkerFetchContext> web_context_;
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;
  Member<SubresourceFilter> subresource_filter_;
  Member<ResourceFetcher> resource_fetcher_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;

  // The value of |save_data_enabled_| is read once per frame from
  // NetworkStateNotifier, which is guarded by a mutex lock, and cached locally
  // here for performance.
  const bool save_data_enabled_;
};

}  // namespace blink

#endif  // WorkerFetchContext_h
