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

#ifndef FrameFetchContext_h
#define FrameFetchContext_h

#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/BaseFetchContext.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/wtf/Forward.h"

namespace blink {

class ClientHintsPreferences;
class ContentSettingsClient;
class Document;
class DocumentLoader;
class LocalFrame;
class LocalFrameClient;
class ResourceError;
class ResourceResponse;
class Settings;
struct WebEnabledClientHints;
class WebTaskRunner;

class CORE_EXPORT FrameFetchContext final : public BaseFetchContext {
 public:
  static ResourceFetcher* CreateFetcherFromDocumentLoader(
      DocumentLoader* loader) {
    return CreateFetcher(loader, nullptr);
  }
  // Used for creating a FrameFetchContext for an imported Document.
  // |document_loader_| will be set to nullptr.
  static ResourceFetcher* CreateFetcherFromDocument(Document* document) {
    return CreateFetcher(nullptr, document);
  }

  static void ProvideDocumentToContext(FetchContext&, Document*);

  ~FrameFetchContext() override;

  bool IsFrameFetchContext() { return true; }

  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  WebCachePolicy ResourceRequestCachePolicy(
      const ResourceRequest&,
      Resource::Type,
      FetchParameters::DeferOption) const override;
  void DispatchDidChangeResourcePriority(unsigned long identifier,
                                         ResourceLoadPriority,
                                         int intra_priority_value) override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  void DispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirect_response,
      Resource::Type,
      const FetchInitiatorInfo& = FetchInitiatorInfo()) override;
  void DispatchDidLoadResourceFromMemoryCache(unsigned long identifier,
                                              const ResourceRequest&,
                                              const ResourceResponse&) override;
  void DispatchDidReceiveResponse(unsigned long identifier,
                                  const ResourceResponse&,
                                  WebURLRequest::FrameType,
                                  WebURLRequest::RequestContext,
                                  Resource*,
                                  ResourceResponseType) override;
  void DispatchDidReceiveData(unsigned long identifier,
                              const char* data,
                              int data_length) override;
  void DispatchDidReceiveEncodedData(unsigned long identifier,
                                     int encoded_data_length) override;
  void DispatchDidDownloadData(unsigned long identifier,
                               int data_length,
                               int encoded_data_length) override;
  void DispatchDidFinishLoading(unsigned long identifier,
                                double finish_time,
                                int64_t encoded_data_length,
                                int64_t decoded_body_length) override;
  void DispatchDidFail(unsigned long identifier,
                       const ResourceError&,
                       int64_t encoded_data_length,
                       bool is_internal_request) override;

  bool ShouldLoadNewResource(Resource::Type) const override;
  void RecordLoadingActivity(unsigned long identifier,
                             const ResourceRequest&,
                             Resource::Type,
                             const AtomicString& fetch_initiator_name) override;
  void DidLoadResource(Resource*) override;

  void AddResourceTiming(const ResourceTimingInfo&) override;
  bool AllowImage(bool images_enabled, const KURL&) const override;
  bool IsControlledByServiceWorker() const override;
  int64_t ServiceWorkerID() const override;
  int ApplicationCacheHostID() const override;

  bool IsMainFrame() const override;
  bool DefersLoading() const override;
  bool IsLoadComplete() const override;
  bool PageDismissalEventBeingDispatched() const override;
  bool UpdateTimingInfoForIFrameNavigation(ResourceTimingInfo*) override;
  void SendImagePing(const KURL&) override;

  SecurityOrigin* GetSecurityOrigin() const override;

  void PopulateResourceRequest(Resource::Type,
                               const ClientHintsPreferences&,
                               const FetchParameters::ResourceWidth&,
                               ResourceRequest&) override;
  void SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&) override;

  // Exposed for testing.
  void ModifyRequestForCSP(ResourceRequest&);
  void AddClientHintsIfNecessary(const ClientHintsPreferences&,
                                 const FetchParameters::ResourceWidth&,
                                 ResourceRequest&);

  MHTMLArchive* Archive() const override;

  std::unique_ptr<WebURLLoader> CreateURLLoader(const ResourceRequest&,
                                                WebTaskRunner*) override;

  bool IsDetached() const override { return frozen_state_; }

  FetchContext* Detach() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  struct FrozenState;

  static ResourceFetcher* CreateFetcher(DocumentLoader*, Document*);

  FrameFetchContext(DocumentLoader*, Document*);

  // Convenient accessors below can be used to transparently access the
  // relevant document loader or frame in either cases without null-checks.
  //
  // TODO(kinuko): Remove constness, these return non-const members.
  DocumentLoader* MasterDocumentLoader() const;
  LocalFrame* GetFrame() const;
  LocalFrameClient* GetLocalFrameClient() const;
  LocalFrame* FrameOfImportsController() const;

  // FetchContext overrides:
  WebFrameScheduler* GetFrameScheduler() override;
  RefPtr<WebTaskRunner> GetLoadingTaskRunner() override;

  // BaseFetchContext overrides:
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

  ReferrerPolicy GetReferrerPolicy() const override;
  String GetOutgoingReferrer() const override;
  const KURL& Url() const override;
  const SecurityOrigin* GetParentSecurityOrigin() const override;
  Optional<WebAddressSpace> GetAddressSpace() const override;
  const ContentSecurityPolicy* GetContentSecurityPolicy() const override;
  void AddConsoleMessage(ConsoleMessage*) const override;

  ContentSettingsClient* GetContentSettingsClient() const;
  Settings* GetSettings() const;
  String GetUserAgent() const;
  RefPtr<SecurityOrigin> GetRequestorOrigin();
  RefPtr<SecurityOrigin> GetRequestorOriginForFrameLoading();
  ClientHintsPreferences GetClientHintsPreferences() const;
  float GetDevicePixelRatio() const;
  bool ShouldSendClientHint(mojom::WebClientHintsType,
                            const ClientHintsPreferences&,
                            const WebEnabledClientHints&) const;
  // Checks if the origin requested persisting the client hints, and notifies
  // the |ContentSettingsClient| with the list of client hints and the
  // persistence duration.
  void ParseAndPersistClientHints(const ResourceResponse&);

  Member<DocumentLoader> document_loader_;
  Member<Document> document_;

  // Non-null only when detached.
  Member<const FrozenState> frozen_state_;
};

}  // namespace blink

#endif
