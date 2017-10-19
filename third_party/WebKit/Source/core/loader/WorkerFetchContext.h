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
class SubresourceFilter;
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
// class is used only when off-main-thread-fetch is enabled.
class WorkerFetchContext final : public BaseFetchContext {
 public:
  static WorkerFetchContext* Create(WorkerOrWorkletGlobalScope&);
  virtual ~WorkerFetchContext();

  // BaseFetchContext implementation:
  KURL GetSiteForCookies() const override;
  bool AllowScriptFromSource(const KURL&) const override;
  SubresourceFilter* GetSubresourceFilter() const override;
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
      WebURLRequest::FrameType,
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
  Optional<WebAddressSpace> GetAddressSpace() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  // FetchContext implementation:
  SecurityOrigin* GetSecurityOrigin() const override;
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest&,
      scoped_refptr<WebTaskRunner>) override;
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
                                  WebURLRequest::FrameType,
                                  WebURLRequest::RequestContext,
                                  Resource*,
                                  ResourceResponseType) override;
  void DispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              int dataLength) override;
  void DispatchDidReceiveEncodedData(unsigned long identifier,
                                     int encodedDataLength) override;
  void DispatchDidFinishLoading(unsigned long identifier,
                                double finishTime,
                                int64_t encodedDataLength,
                                int64_t decodedBodyLength) override;
  void DispatchDidFail(unsigned long identifier,
                       const ResourceError&,
                       int64_t encodedDataLength,
                       bool isInternalRequest) override;
  void AddResourceTiming(const ResourceTimingInfo&) override;
  void PopulateResourceRequest(Resource::Type,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;
  void SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&) override;
  scoped_refptr<WebTaskRunner> GetLoadingTaskRunner() override;

  virtual void Trace(blink::Visitor*);

 private:
  WorkerFetchContext(WorkerOrWorkletGlobalScope&,
                     std::unique_ptr<WebWorkerFetchContext>);

  Member<WorkerOrWorkletGlobalScope> global_scope_;
  std::unique_ptr<WebWorkerFetchContext> web_context_;
  Member<SubresourceFilter> subresource_filter_;
  Member<ResourceFetcher> resource_fetcher_;
  scoped_refptr<WebTaskRunner> loading_task_runner_;
};

}  // namespace blink

#endif  // WorkerFetchContext_h
