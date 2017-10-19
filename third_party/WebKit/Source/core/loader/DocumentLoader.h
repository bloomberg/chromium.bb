/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DocumentLoader_h
#define DocumentLoader_h

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/WeakIdentifierMap.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/parser/ParserSynchronizationPolicy.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/fetch/SubstituteData.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebLoadingBehaviorFlag.h"

#include <memory>

namespace blink {

class ApplicationCacheHost;
class SubresourceFilter;
class ResourceFetcher;
class Document;
class DocumentParser;
class HistoryItem;
class LocalFrame;
class LocalFrameClient;
class FrameLoader;
class ResourceTimingInfo;
class SerializedScriptValue;
class WebServiceWorkerNetworkProvider;
struct ViewportDescriptionWrapper;

// The DocumentLoader fetches a main resource and handles the result.
class CORE_EXPORT DocumentLoader
    : public GarbageCollectedFinalized<DocumentLoader>,
      private RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentLoader);

 public:
  static DocumentLoader* Create(LocalFrame* frame,
                                const ResourceRequest& request,
                                const SubstituteData& data,
                                ClientRedirectPolicy client_redirect_policy) {
    DCHECK(frame);

    return new DocumentLoader(frame, request, data, client_redirect_policy);
  }
  ~DocumentLoader() override;

  LocalFrame* GetFrame() const { return frame_; }

  ResourceTimingInfo* GetNavigationTimingInfo() const;

  virtual void DetachFromFrame();

  unsigned long MainResourceIdentifier() const;

  void ReplaceDocumentWhileExecutingJavaScriptURL(
      const KURL&,
      Document* owner_document,
      bool should_reuse_default_view,
      const String& source);

  const AtomicString& MimeType() const;

  const ResourceRequest& OriginalRequest() const;

  const ResourceRequest& GetRequest() const;

  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }

  void SetSubresourceFilter(SubresourceFilter*);
  SubresourceFilter* GetSubresourceFilter() const {
    return subresource_filter_.Get();
  }

  const SubstituteData& GetSubstituteData() const { return substitute_data_; }

  const KURL& Url() const;
  const KURL& UnreachableURL() const;
  const KURL& UrlForHistory() const;

  void DidChangePerformanceTiming();
  void DidObserveLoadingBehavior(WebLoadingBehaviorFlag);
  void UpdateForSameDocumentNavigation(const KURL&,
                                       SameDocumentNavigationSource,
                                       scoped_refptr<SerializedScriptValue>,
                                       HistoryScrollRestorationType,
                                       FrameLoadType,
                                       Document*);
  const ResourceResponse& GetResponse() const { return response_; }
  bool IsClientRedirect() const { return is_client_redirect_; }
  void SetIsClientRedirect(bool is_client_redirect) {
    is_client_redirect_ = is_client_redirect;
  }
  bool ReplacesCurrentHistoryItem() const {
    return replaces_current_history_item_;
  }
  void SetReplacesCurrentHistoryItem(bool replaces_current_history_item) {
    replaces_current_history_item_ = replaces_current_history_item;
  }

  bool IsCommittedButEmpty() const {
    return state_ >= kCommitted && !data_received_;
  }

  // Without PlzNavigate, this is only false for a narrow window during
  // navigation start. For PlzNavigate, a navigation sent to the browser will
  // leave a dummy DocumentLoader in the NotStarted state until the navigation
  // is actually handled in the renderer.
  bool DidStart() const { return state_ != kNotStarted; }

  void MarkAsCommitted();
  void SetSentDidFinishLoad() { state_ = kSentDidFinishLoad; }
  bool SentDidFinishLoad() const { return state_ == kSentDidFinishLoad; }

  FrameLoadType LoadType() const { return load_type_; }
  void SetLoadType(FrameLoadType load_type) { load_type_ = load_type; }

  NavigationType GetNavigationType() const { return navigation_type_; }
  void SetNavigationType(NavigationType navigation_type) {
    navigation_type_ = navigation_type;
  }

  void SetItemForHistoryNavigation(HistoryItem* item) { history_item_ = item; }
  HistoryItem* GetHistoryItem() const { return history_item_; }

  void StartLoading();

  DocumentLoadTiming& GetTiming() { return document_load_timing_; }
  const DocumentLoadTiming& GetTiming() const { return document_load_timing_; }

  ApplicationCacheHost* GetApplicationCacheHost() const {
    return application_cache_host_.Get();
  }

  void ClearRedirectChain();
  void AppendRedirect(const KURL&);

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hints_preferences_;
  }

  struct InitialScrollState {
    DISALLOW_NEW();
    InitialScrollState()
        : was_scrolled_by_user(false), did_restore_from_history(false) {}

    bool was_scrolled_by_user;
    bool did_restore_from_history;
  };
  InitialScrollState& GetInitialScrollState() { return initial_scroll_state_; }

  void SetWasBlockedAfterCSP() { was_blocked_after_csp_ = true; }
  bool WasBlockedAfterCSP() { return was_blocked_after_csp_; }

  void DispatchLinkHeaderPreloads(ViewportDescriptionWrapper*,
                                  LinkLoader::MediaPreloadPolicy);

  Resource* StartPreload(Resource::Type, FetchParameters&);

  void SetServiceWorkerNetworkProvider(
      std::unique_ptr<WebServiceWorkerNetworkProvider>);

  // May return null before the first HTML tag is inserted by the
  // parser (before didCreateDataSource is called), after the document
  // is detached from frame, or in tests.
  WebServiceWorkerNetworkProvider* GetServiceWorkerNetworkProvider() {
    return service_worker_network_provider_.get();
  }

  std::unique_ptr<SourceLocation> CopySourceLocation() const;
  void SetSourceLocation(std::unique_ptr<SourceLocation>);

  void LoadFailed(const ResourceError&);

  virtual void Trace(blink::Visitor*);

 protected:
  DocumentLoader(LocalFrame*,
                 const ResourceRequest&,
                 const SubstituteData&,
                 ClientRedirectPolicy);

  static bool ShouldClearWindowName(const LocalFrame&,
                                    SecurityOrigin* previous_security_origin,
                                    const Document& new_document);

  Vector<KURL> redirect_chain_;

 private:
  // installNewDocument() does the work of creating a Document and
  // DocumentParser, as well as creating a new LocalDOMWindow if needed. It also
  // initalizes a bunch of state on the Document (e.g., the state based on
  // response headers).
  enum class InstallNewDocumentReason { kNavigation, kJavascriptURL };
  void InstallNewDocument(const KURL&,
                          Document* owner_document,
                          bool should_reuse_default_view,
                          const AtomicString& mime_type,
                          const AtomicString& encoding,
                          InstallNewDocumentReason,
                          ParserSynchronizationPolicy,
                          const KURL& overriding_url);
  void DidInstallNewDocument(Document*);
  void WillCommitNavigation();
  void DidCommitNavigation();

  void CommitNavigation(const AtomicString& mime_type,
                        const KURL& overriding_url = KURL());

  // Use these method only where it's guaranteed that |m_frame| hasn't been
  // cleared.
  FrameLoader& GetFrameLoader() const;
  LocalFrameClient& GetLocalFrameClient() const;

  void CommitData(const char* bytes, size_t length);
  void ClearMainResourceHandle();

  bool MaybeCreateArchive();

  void FinishedLoading(double finish_time);
  void CancelLoadAfterCSPDenied(const ResourceResponse&);

  enum class HistoryNavigationType {
    kDifferentDocument,
    kFragment,
    kHistoryApi
  };
  void SetHistoryItemStateForCommit(HistoryItem* old_item,
                                    FrameLoadType,
                                    HistoryNavigationType);

  // RawResourceClient implementation
  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) final;
  void ResponseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) final;
  void DataReceived(Resource*, const char* data, size_t length) final;

  // ResourceClient implementation
  void NotifyFinished(Resource*) final;
  String DebugName() const override { return "DocumentLoader"; }

  void ProcessData(const char* data, size_t length);

  bool MaybeLoadEmpty();

  bool IsRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

  bool ShouldContinueForResponse() const;

  Member<LocalFrame> frame_;
  Member<ResourceFetcher> fetcher_;

  Member<RawResource> main_resource_;
  Member<HistoryItem> history_item_;

  // The parser that was created when the current Document was installed.
  // document.open() may create a new parser at a later point, but this
  // will not be updated.
  Member<DocumentParser> parser_;

  Member<SubresourceFilter> subresource_filter_;

  // A reference to actual request used to create the data source.
  // The only part of this request that should change is the url, and
  // that only in the case of a same-document navigation.
  ResourceRequest original_request_;

  SubstituteData substitute_data_;

  // The 'working' request. It may be mutated
  // several times from the original request to include additional
  // headers, cookie information, canonicalization and redirects.
  ResourceRequest request_;

  ResourceResponse response_;

  FrameLoadType load_type_;

  bool is_client_redirect_;
  bool replaces_current_history_item_;
  bool data_received_;

  NavigationType navigation_type_;

  DocumentLoadTiming document_load_timing_;

  double time_of_last_data_received_;

  Member<ApplicationCacheHost> application_cache_host_;

  std::unique_ptr<WebServiceWorkerNetworkProvider>
      service_worker_network_provider_;

  Member<ContentSecurityPolicy> content_security_policy_;
  ClientHintsPreferences client_hints_preferences_;
  InitialScrollState initial_scroll_state_;

  bool was_blocked_after_csp_;

  static bool ShouldPersistUserGestureValue(
      const SecurityOrigin* previous_security_origin,
      const SecurityOrigin* new_security_origin);
  static bool CheckOriginIsHttpOrHttps(const SecurityOrigin*);

  // PlzNavigate: set when committing a navigation. The data has originally been
  // captured when the navigation was sent to the browser process, and it is
  // sent back at commit time.
  std::unique_ptr<SourceLocation> source_location_;

  enum State { kNotStarted, kProvisional, kCommitted, kSentDidFinishLoad };
  State state_;

  // Used to protect against reentrancy into dataReceived().
  bool in_data_received_;
  scoped_refptr<SharedBuffer> data_buffer_;
};

DECLARE_WEAK_IDENTIFIER_MAP(DocumentLoader);

}  // namespace blink

#endif  // DocumentLoader_h
