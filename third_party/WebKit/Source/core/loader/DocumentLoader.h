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

#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/fetch/ResourcePtr.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentWriter.h"
#include "core/loader/NavigationAction.h"
#include "core/loader/SubstituteData.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"

namespace blink {
class WebThreadedDataReceiver;
}

namespace blink {
    class ApplicationCacheHost;
    class ArchiveResourceCollection;
    class ResourceFetcher;
    class DocumentInit;
    class LocalFrame;
    class FrameLoader;
    class MHTMLArchive;
    class ResourceLoader;

    class DocumentLoader : public RefCounted<DocumentLoader>, private RawResourceClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static PassRefPtr<DocumentLoader> create(LocalFrame* frame, const ResourceRequest& request, const SubstituteData& data)
        {
            return adoptRef(new DocumentLoader(frame, request, data));
        }
        virtual ~DocumentLoader();

        LocalFrame* frame() const { return m_frame; }

        void detachFromFrame();

        unsigned long mainResourceIdentifier() const;

        void replaceDocumentWhileExecutingJavaScriptURL(const DocumentInit&, const String& source, Document*);

        const AtomicString& mimeType() const;

        void setUserChosenEncoding(const String& charset);

        const ResourceRequest& originalRequest() const;

        const ResourceRequest& request() const;

        ResourceFetcher* fetcher() const { return m_fetcher.get(); }

        const SubstituteData& substituteData() const { return m_substituteData; }

        const KURL& url() const;
        const KURL& unreachableURL() const;
        const KURL& urlForHistory() const;

        const AtomicString& responseMIMEType() const;

        void updateForSameDocumentNavigation(const KURL&, SameDocumentNavigationSource);
        void stopLoading();
        bool isCommitted() const { return m_committed; }
        bool isLoading() const;
        bool isLoadingMainResource() const { return m_loadingMainResource; }
        const ResourceResponse& response() const { return m_response; }
        const ResourceError& mainDocumentError() const { return m_mainDocumentError; }
        bool isClientRedirect() const { return m_isClientRedirect; }
        void setIsClientRedirect(bool isClientRedirect) { m_isClientRedirect = isClientRedirect; }
        bool replacesCurrentHistoryItem() const { return m_replacesCurrentHistoryItem; }
        void setReplacesCurrentHistoryItem(bool replacesCurrentHistoryItem) { m_replacesCurrentHistoryItem = replacesCurrentHistoryItem; }

        bool scheduleArchiveLoad(Resource*, const ResourceRequest&);

        bool shouldContinueForNavigationPolicy(const ResourceRequest&, ContentSecurityPolicyCheck shouldCheckMainWorldContentSecurityPolicy, bool isTransitionNavigation = false);
        const NavigationAction& triggeringAction() const { return m_triggeringAction; }
        void setTriggeringAction(const NavigationAction& action) { m_triggeringAction = action; }

        void setDefersLoading(bool);

        void startLoadingMainResource();
        void cancelMainResourceLoad(const ResourceError&);

        void attachThreadedDataReceiver(PassOwnPtr<blink::WebThreadedDataReceiver>);
        DocumentLoadTiming* timing() { return &m_documentLoadTiming; }

        ApplicationCacheHost* applicationCacheHost() const { return m_applicationCacheHost.get(); }

        bool isRedirect() const { return m_redirectChain.size() > 1; }
        void clearRedirectChain();
        void appendRedirect(const KURL&);

        PassRefPtr<ContentSecurityPolicy> releaseContentSecurityPolicy() { return m_contentSecurityPolicy.release(); }

    protected:
        DocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&);

        Vector<KURL> m_redirectChain;

    private:
        static PassRefPtrWillBeRawPtr<DocumentWriter> createWriterFor(const Document* ownerDocument, const DocumentInit&, const AtomicString& mimeType, const AtomicString& encoding, bool dispatch);

        void ensureWriter(const AtomicString& mimeType, const KURL& overridingURL = KURL());
        void endWriting(DocumentWriter*);

        Document* document() const;
        FrameLoader* frameLoader() const;

        void commitIfReady();
        void commitData(const char* bytes, size_t length);
        void setMainDocumentError(const ResourceError&);
        void clearMainResourceLoader();
        ResourceLoader* mainResourceLoader() const;
        void clearMainResourceHandle();

        bool maybeCreateArchive();

        void prepareSubframeArchiveLoadIfNeeded();
        void addAllArchiveResources(MHTMLArchive*);

        void willSendRequest(ResourceRequest&, const ResourceResponse&);
        void finishedLoading(double finishTime);
        void mainReceivedError(const ResourceError&);
        void cancelLoadAfterXFrameOptionsOrCSPDenied(const ResourceResponse&);
        virtual void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) override final;
        virtual void updateRequest(Resource*, const ResourceRequest&) override final;
        virtual void responseReceived(Resource*, const ResourceResponse&) override final;
        virtual void dataReceived(Resource*, const char* data, unsigned length) override final;
        virtual void notifyFinished(Resource*) override final;

        bool maybeLoadEmpty();

        bool isRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

        bool shouldContinueForResponse() const;

        LocalFrame* m_frame;
        RefPtrWillBePersistent<ResourceFetcher> m_fetcher;

        ResourcePtr<RawResource> m_mainResource;

        RefPtrWillBePersistent<DocumentWriter> m_writer;

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

        ResourceError m_mainDocumentError;

        bool m_committed;
        bool m_isClientRedirect;
        bool m_replacesCurrentHistoryItem;

        // The action that triggered loading - we keep this around for the
        // benefit of the various policy handlers.
        NavigationAction m_triggeringAction;

        OwnPtrWillBePersistent<ArchiveResourceCollection> m_archiveResourceCollection;
        RefPtrWillBePersistent<MHTMLArchive> m_archive;

        bool m_loadingMainResource;
        DocumentLoadTiming m_documentLoadTiming;

        double m_timeOfLastDataReceived;

        friend class ApplicationCacheHost;  // for substitute resource delivery
        OwnPtrWillBePersistent<ApplicationCacheHost> m_applicationCacheHost;

        RefPtr<ContentSecurityPolicy> m_contentSecurityPolicy;
    };
}

#endif // DocumentLoader_h
