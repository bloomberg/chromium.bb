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

#include "core/CoreExport.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/WeakIdentifierMap.h"
#include "core/frame/FrameTypes.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentWriter.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/LinkLoader.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/SubstituteData.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "public/platform/WebLoadingBehaviorFlag.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"

#include <memory>

namespace blink {

class ApplicationCacheHost;
class SubresourceFilter;
class ResourceFetcher;
class DocumentInit;
class LocalFrame;
class LocalFrameClient;
class FrameLoader;
class ResourceTimingInfo;
struct ViewportDescriptionWrapper;

class CORE_EXPORT DocumentLoader
    : public GarbageCollectedFinalized<DocumentLoader>,
      private RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DocumentLoader);

 public:
  static DocumentLoader* create(LocalFrame* frame,
                                const ResourceRequest& request,
                                const SubstituteData& data,
                                ClientRedirectPolicy clientRedirectPolicy) {
    DCHECK(frame);

    return new DocumentLoader(frame, request, data, clientRedirectPolicy);
  }
  ~DocumentLoader() override;

  LocalFrame* frame() const { return m_frame; }

  ResourceTimingInfo* getNavigationTimingInfo() const;

  virtual void detachFromFrame();

  unsigned long mainResourceIdentifier() const;

  void replaceDocumentWhileExecutingJavaScriptURL(const DocumentInit&,
                                                  const String& source);

  const AtomicString& mimeType() const;

  const ResourceRequest& originalRequest() const;

  const ResourceRequest& getRequest() const;

  ResourceFetcher* fetcher() const { return m_fetcher.get(); }

  void setSubresourceFilter(SubresourceFilter*);
  SubresourceFilter* subresourceFilter() const {
    return m_subresourceFilter.get();
  }

  const SubstituteData& substituteData() const { return m_substituteData; }

  const KURL& url() const;
  const KURL& unreachableURL() const;
  const KURL& urlForHistory() const;

  const AtomicString& responseMIMEType() const;

  void didChangePerformanceTiming();
  void didObserveLoadingBehavior(WebLoadingBehaviorFlag);
  void updateForSameDocumentNavigation(const KURL&,
                                       SameDocumentNavigationSource);
  const ResourceResponse& response() const { return m_response; }
  bool isClientRedirect() const { return m_isClientRedirect; }
  void setIsClientRedirect(bool isClientRedirect) {
    m_isClientRedirect = isClientRedirect;
  }
  bool replacesCurrentHistoryItem() const {
    return m_replacesCurrentHistoryItem;
  }
  void setReplacesCurrentHistoryItem(bool replacesCurrentHistoryItem) {
    m_replacesCurrentHistoryItem = replacesCurrentHistoryItem;
  }

  bool isCommittedButEmpty() const {
    return m_state >= Committed && !m_dataReceived;
  }

  void setSentDidFinishLoad() { m_state = SentDidFinishLoad; }
  bool sentDidFinishLoad() const { return m_state == SentDidFinishLoad; }

  FrameLoadType loadType() const { return m_loadType; }
  void setLoadType(FrameLoadType loadType) { m_loadType = loadType; }

  NavigationType getNavigationType() const { return m_navigationType; }
  void setNavigationType(NavigationType navigationType) {
    m_navigationType = navigationType;
  }

  void startLoadingMainResource();

  DocumentLoadTiming& timing() { return m_documentLoadTiming; }
  const DocumentLoadTiming& timing() const { return m_documentLoadTiming; }

  ApplicationCacheHost* applicationCacheHost() const {
    return m_applicationCacheHost.get();
  }

  void clearRedirectChain();
  void appendRedirect(const KURL&);

  ContentSecurityPolicy* releaseContentSecurityPolicy() {
    return m_contentSecurityPolicy.release();
  }

  ClientHintsPreferences& clientHintsPreferences() {
    return m_clientHintsPreferences;
  }

  struct InitialScrollState {
    DISALLOW_NEW();
    InitialScrollState()
        : wasScrolledByUser(false), didRestoreFromHistory(false) {}

    bool wasScrolledByUser;
    bool didRestoreFromHistory;
  };
  InitialScrollState& initialScrollState() { return m_initialScrollState; }

  void setWasBlockedAfterCSP() { m_wasBlockedAfterCSP = true; }
  bool wasBlockedAfterCSP() { return m_wasBlockedAfterCSP; }

  void dispatchLinkHeaderPreloads(ViewportDescriptionWrapper*,
                                  LinkLoader::MediaPreloadPolicy);

  Resource* startPreload(Resource::Type, FetchRequest&);

  DECLARE_VIRTUAL_TRACE();

 protected:
  DocumentLoader(LocalFrame*,
                 const ResourceRequest&,
                 const SubstituteData&,
                 ClientRedirectPolicy);

  Vector<KURL> m_redirectChain;

 private:
  static DocumentWriter* createWriterFor(const DocumentInit&,
                                         const AtomicString& mimeType,
                                         const AtomicString& encoding,
                                         bool dispatchWindowObjectAvailable,
                                         ParserSynchronizationPolicy,
                                         const KURL& overridingURL = KURL());

  void ensureWriter(const AtomicString& mimeType,
                    const KURL& overridingURL = KURL());
  void endWriting();

  // Use these method only where it's guaranteed that |m_frame| hasn't been
  // cleared.
  FrameLoader& frameLoader() const;
  LocalFrameClient& localFrameClient() const;

  void commitIfReady();
  void commitData(const char* bytes, size_t length);
  void clearMainResourceHandle();

  bool maybeCreateArchive();

  void finishedLoading(double finishTime);
  void cancelLoadAfterCSPDenied(const ResourceResponse&);

  // RawResourceClient implementation
  bool redirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) final;
  void responseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) final;
  void dataReceived(Resource*, const char* data, size_t length) final;

  // ResourceClient implementation
  void notifyFinished(Resource*) final;
  String debugName() const override { return "DocumentLoader"; }

  void processData(const char* data, size_t length);

  bool maybeLoadEmpty();

  bool isRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

  bool shouldContinueForResponse() const;

  Member<LocalFrame> m_frame;
  Member<ResourceFetcher> m_fetcher;

  Member<RawResource> m_mainResource;

  Member<DocumentWriter> m_writer;

  Member<SubresourceFilter> m_subresourceFilter;

  // A reference to actual request used to create the data source.
  // The only part of this request that should change is the url, and
  // that only in the case of a same-document navigation.
  ResourceRequest m_originalRequest;

  SubstituteData m_substituteData;

  // The 'working' request. It may be mutated
  // several times from the original request to include additional
  // headers, cookie information, canonicalization and redirects.
  ResourceRequest m_request;

  ResourceResponse m_response;

  FrameLoadType m_loadType;

  bool m_isClientRedirect;
  bool m_replacesCurrentHistoryItem;
  bool m_dataReceived;

  NavigationType m_navigationType;

  DocumentLoadTiming m_documentLoadTiming;

  double m_timeOfLastDataReceived;

  Member<ApplicationCacheHost> m_applicationCacheHost;

  Member<ContentSecurityPolicy> m_contentSecurityPolicy;
  ClientHintsPreferences m_clientHintsPreferences;
  InitialScrollState m_initialScrollState;

  bool m_wasBlockedAfterCSP;

  enum State {
    NotStarted,
    Provisional,
    Committed,
    SentDidFinishLoad
  };
  State m_state;

  // Used to protect against reentrancy into dataReceived().
  bool m_inDataReceived;
  RefPtr<SharedBuffer> m_dataBuffer;
};

DECLARE_WEAK_IDENTIFIER_MAP(DocumentLoader);

}  // namespace blink

#endif  // DocumentLoader_h
