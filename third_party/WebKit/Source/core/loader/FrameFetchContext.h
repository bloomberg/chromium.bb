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
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "wtf/Forward.h"

namespace blink {

class ClientHintsPreferences;
class ContentSettingsClient;
class Document;
class DocumentLoader;
class LocalFrame;
class LocalFrameClient;
class ResourceError;
class ResourceResponse;

class CORE_EXPORT FrameFetchContext final : public FetchContext {
 public:
  static ResourceFetcher* CreateFetcherFromDocumentLoader(
      DocumentLoader* loader) {
    return ResourceFetcher::Create(new FrameFetchContext(loader, nullptr));
  }
  static ResourceFetcher* CreateFetcherFromDocument(Document* document) {
    return ResourceFetcher::Create(new FrameFetchContext(nullptr, document));
  }

  static void ProvideDocumentToContext(FetchContext& context,
                                       Document* document) {
    DCHECK(document);
    CHECK(context.IsLiveContext());
    static_cast<FrameFetchContext&>(context).document_ = document;
  }

  ~FrameFetchContext();

  bool IsLiveContext() { return true; }

  void AddAdditionalRequestHeaders(ResourceRequest&,
                                   FetchResourceType) override;
  WebCachePolicy ResourceRequestCachePolicy(
      ResourceRequest&,
      Resource::Type,
      FetchRequest::DeferOption) const override;
  void DispatchDidChangeResourcePriority(unsigned long identifier,
                                         ResourceLoadPriority,
                                         int intra_priority_value) override;
  void PrepareRequest(ResourceRequest&, RedirectType) override;
  void DispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest&,
      const ResourceResponse& redirect_response,
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
  ResourceRequestBlockedReason CanRequest(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchRequest::OriginRestriction) const override;
  ResourceRequestBlockedReason AllowResponse(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&) const override;

  bool IsControlledByServiceWorker() const override;
  int64_t ServiceWorkerID() const override;

  bool IsMainFrame() const override;
  bool DefersLoading() const override;
  bool IsLoadComplete() const override;
  bool PageDismissalEventBeingDispatched() const override;
  bool UpdateTimingInfoForIFrameNavigation(ResourceTimingInfo*) override;
  void SendImagePing(const KURL&) override;
  void AddConsoleMessage(const String&,
                         LogMessageType = kLogErrorMessage) const override;
  SecurityOrigin* GetSecurityOrigin() const override;

  void PopulateResourceRequest(Resource::Type,
                               const ClientHintsPreferences&,
                               const FetchRequest::ResourceWidth&,
                               ResourceRequest&) override;
  void SetFirstPartyCookieAndRequestorOrigin(ResourceRequest&) override;

  // Exposed for testing.
  void ModifyRequestForCSP(ResourceRequest&);
  void AddClientHintsIfNecessary(const ClientHintsPreferences&,
                                 const FetchRequest::ResourceWidth&,
                                 ResourceRequest&);

  MHTMLArchive* Archive() const override;

  ResourceLoadPriority ModifyPriorityForExperiments(
      ResourceLoadPriority) override;

  RefPtr<WebTaskRunner> LoadingTaskRunner() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  FrameFetchContext(DocumentLoader*, Document*);

  // m_documentLoader is null when loading resources from an HTML import
  // and in such cases we use the document loader of the importing frame.
  // Convenient accessors below can be used to transparently access the
  // relevant document loader or frame in either cases without null-checks.
  // TODO(kinuko): Remove constness, these return non-const members.
  DocumentLoader* MasterDocumentLoader() const;
  LocalFrame* GetFrame() const;
  LocalFrameClient* GetLocalFrameClient() const;

  ContentSettingsClient* GetContentSettingsClient() const;

  LocalFrame* FrameOfImportsController() const;

  void PrintAccessDeniedMessage(const KURL&) const;
  ResourceRequestBlockedReason CanRequestInternal(
      Resource::Type,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      FetchRequest::OriginRestriction,
      ResourceRequest::RedirectStatus) const;

  void AddCSPHeaderIfNecessary(Resource::Type, ResourceRequest&);

  // FIXME: Oilpan: Ideally this should just be a traced Member but that will
  // currently leak because ComputedStyle and its data are not on the heap.
  // See crbug.com/383860 for details.
  WeakMember<Document> document_;
  Member<DocumentLoader> document_loader_;
};

}  // namespace blink

#endif
