// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerFetchContext_h
#define WorkerFetchContext_h

#include <memory>
#include "core/CoreExport.h"
#include "core/loader/BaseFetchContext.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ResourceFetcher;
class WebTaskRunner;
class WebURLLoader;
class WebWorkerFetchContext;
class WorkerClients;
class WorkerOrWorkletGlobalScope;

CORE_EXPORT void ProvideWorkerFetchContextToWorker(
    WorkerClients*,
    std::unique_ptr<WebWorkerFetchContext>);

// The WorkerFetchContext is a FetchContext for workers (dedicated, shared and
// service workers) and threaded worklets (animation and audio worklets). This
// class is used only when off-main-thread-fetch is enabled, and is still under
// development.
// TODO(horo): Implement all methods of FetchContext. crbug.com/443374
class WorkerFetchContext final : public BaseFetchContext {
 public:
  static WorkerFetchContext* Create(WorkerOrWorkletGlobalScope&);
  virtual ~WorkerFetchContext();

  ResourceFetcher* GetResourceFetcher();
  KURL FirstPartyForCookies() const;
  RefPtr<WebTaskRunner> GetTaskRunner() { return loading_task_runner_; }

  // BaseFetchContext implementation:
  ContentSettingsClient* GetContentSettingsClient() const override;
  Settings* GetSettings() const override;
  SubresourceFilter* GetSubresourceFilter() const override;
  bool ShouldBlockRequestByInspector(const ResourceRequest&) const override;
  void DispatchDidBlockRequest(const ResourceRequest&,
                               const FetchInitiatorInfo&,
                               ResourceRequestBlockedReason) const override;
  bool ShouldBypassMainWorldCSP() const override;
  bool IsSVGImageChromeClient() const override;
  void CountUsage(WebFeature) const override;
  void CountDeprecation(WebFeature) const override;
  bool ShouldBlockFetchByMixedContentCheck(
      const ResourceRequest&,
      const KURL&,
      SecurityViolationReportingPolicy) const override;
  bool ShouldBlockFetchAsCredentialedSubresource(const ResourceRequest&,
                                                 const KURL&) const override;
  ReferrerPolicy GetReferrerPolicy() const override;
  String GetOutgoingReferrer() const override;
  const KURL& Url() const override;
  const SecurityOrigin* GetParentSecurityOrigin() const override;
  Optional<WebAddressSpace> GetAddressSpace() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  // FetchContext implementation:
  // TODO(horo): Implement more methods.
  SecurityOrigin* GetSecurityOrigin() const override;
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&) override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  bool IsControlledByServiceWorker() const override;

  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  void DispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceResponse&,
                                  WebURLRequest::FrameType,
                                  WebURLRequest::RequestContext,
                                  Resource*,
                                  ResourceResponseType) override;
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void PopulateResourceRequest(const KURL&,
                               Resource::Type,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               const ResourceLoaderOptions&,
                               SecurityViolationReportingPolicy,
                               ResourceRequest&) override;
  void SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  WorkerFetchContext(WorkerOrWorkletGlobalScope&,
                     std::unique_ptr<WebWorkerFetchContext>);

  Member<WorkerOrWorkletGlobalScope> global_scope_;
  std::unique_ptr<WebWorkerFetchContext> web_context_;
  Member<ResourceFetcher> resource_fetcher_;
  RefPtr<WebTaskRunner> loading_task_runner_;
};

}  // namespace blink

#endif  // WorkerFetchContext_h
