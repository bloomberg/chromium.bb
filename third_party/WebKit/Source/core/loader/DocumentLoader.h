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
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentWriter.h"
#include "core/loader/NavigationAction.h"
#include "core/loader/SubstituteData.h"
#include "core/platform/Timer.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"

namespace WTF {
class SchedulePair;
}

namespace WebCore {
    class ApplicationCacheHost;
    class ArchiveResource;
    class ArchiveResourceCollection;
    class ResourceFetcher;
    class ContentFilter;
    class FormState;
    class Frame;
    class FrameLoader;
    class MHTMLArchive;
    class Page;
    class ResourceLoader;
    class SharedBuffer;

    class DocumentLoader : public RefCounted<DocumentLoader>, private RawResourceClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static PassRefPtr<DocumentLoader> create(const ResourceRequest& request, const SubstituteData& data)
        {
            return adoptRef(new DocumentLoader(request, data));
        }
        virtual ~DocumentLoader();

        void setFrame(Frame*);
        Frame* frame() const { return m_frame; }

        void detachFromFrame();

        FrameLoader* frameLoader() const;

        unsigned long mainResourceIdentifier() const;

        void replaceDocument(const String& source, Document*);
        DocumentWriter* beginWriting(const String& mimeType, const String& encoding, const KURL& = KURL());
        void endWriting(DocumentWriter*);

        String mimeType() const;

        const ResourceRequest& originalRequest() const;
        const ResourceRequest& originalRequestCopy() const;

        const ResourceRequest& request() const;
        ResourceRequest& request();

        ResourceFetcher* fetcher() const { return m_fetcher.get(); }

        const SubstituteData& substituteData() const { return m_substituteData; }

        // FIXME: This is the same as requestURL(). We should remove one of them.
        const KURL& url() const;
        const KURL& unreachableURL() const;

        const KURL& originalURL() const;
        const KURL& requestURL() const;
        const String& responseMIMEType() const;

        void replaceRequestURLForSameDocumentNavigation(const KURL&);
        void stopLoading();
        void setCommitted(bool committed) { m_committed = committed; }
        bool isCommitted() const { return m_committed; }
        bool isLoading() const;
        const ResourceResponse& response() const { return m_response; }
        const ResourceError& mainDocumentError() const { return m_mainDocumentError; }
        bool isClientRedirect() const { return m_isClientRedirect; }
        void setIsClientRedirect(bool isClientRedirect) { m_isClientRedirect = isClientRedirect; }
        bool replacesCurrentHistoryItem() const { return m_replacesCurrentHistoryItem; }
        void setReplacesCurrentHistoryItem(bool replacesCurrentHistoryItem) { m_replacesCurrentHistoryItem = replacesCurrentHistoryItem; }
        bool isLoadingInAPISense() const;
        const String& overrideEncoding() const { return m_overrideEncoding; }

        bool scheduleArchiveLoad(Resource*, const ResourceRequest&);
        void cancelPendingSubstituteLoad(ResourceLoader*);

        enum PolicyCheckLoadType {
            PolicyCheckStandard,
            PolicyCheckFragment
        };
        bool shouldContinueForNavigationPolicy(const ResourceRequest&, PolicyCheckLoadType);
        const NavigationAction& triggeringAction() const { return m_triggeringAction; }
        void setTriggeringAction(const NavigationAction& action) { m_triggeringAction = action; }

        void setOverrideEncoding(const String& encoding) { m_overrideEncoding = encoding; }

        void setDefersLoading(bool);

        void startLoadingMainResource();
        void cancelMainResourceLoad(const ResourceError&);

        bool isLoadingMainResource() const { return m_loadingMainResource; }

        void stopLoadingSubresources();

        void subresourceLoaderFinishedLoadingOnePart(ResourceLoader*);

        void setDeferMainResourceDataLoad(bool defer) { m_deferMainResourceDataLoad = defer; }

        DocumentLoadTiming* timing() { return &m_documentLoadTiming; }
        void resetTiming() { m_documentLoadTiming = DocumentLoadTiming(); }

        ApplicationCacheHost* applicationCacheHost() const { return m_applicationCacheHost.get(); }

        void checkLoadComplete();

        bool isRedirect() const { return m_redirectChain.size() > 1; }
        void clearRedirectChain();
        void appendRedirect(const KURL&);

    protected:
        DocumentLoader(const ResourceRequest&, const SubstituteData&);

        bool m_deferMainResourceDataLoad;
        Vector<KURL> m_redirectChain;

    private:
        static PassRefPtr<DocumentWriter> createWriterFor(Frame*, const Document* ownerDocument, const KURL&, const String& mimeType, const String& encoding, bool userChosen, bool dispatch);

        void ensureWriter();
        void ensureWriter(const String& mimeType, const KURL& overridingURL = KURL());

        Document* document() const;

        void setRequest(const ResourceRequest&);

        void commitIfReady();
        void commitData(const char* bytes, size_t length);
        void setMainDocumentError(const ResourceError&);
        void clearMainResourceLoader();
        ResourceLoader* mainResourceLoader() const;
        void clearMainResourceHandle();
        PassRefPtr<SharedBuffer> mainResourceData() const;

        void createArchive();
        void clearArchiveResources();

        void prepareSubframeArchiveLoadIfNeeded();
        void addAllArchiveResources(MHTMLArchive*);

        void willSendRequest(ResourceRequest&, const ResourceResponse&);
        void finishedLoading(double finishTime);
        void mainReceivedError(const ResourceError&);
        virtual void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) OVERRIDE;
        virtual void responseReceived(Resource*, const ResourceResponse&) OVERRIDE;
        virtual void dataReceived(Resource*, const char* data, int length) OVERRIDE;
        virtual void notifyFinished(Resource*) OVERRIDE;

        bool maybeLoadEmpty();

        bool isRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

        bool shouldContinueForResponse() const;

        typedef Timer<DocumentLoader> DocumentLoaderTimer;

        void handleSubstituteDataLoadSoon();
        void handleSubstituteDataLoadNow(DocumentLoaderTimer*);
        void startDataLoadTimer();

        Frame* m_frame;
        RefPtr<ResourceFetcher> m_fetcher;

        ResourcePtr<RawResource> m_mainResource;

        RefPtr<DocumentWriter> m_writer;

        // A reference to actual request used to create the data source.
        // This should only be used by the resourceLoadDelegate's
        // identifierForInitialRequest:fromDatasource: method. It is
        // not guaranteed to remain unchanged, as requests are mutable.
        ResourceRequest m_originalRequest;

        SubstituteData m_substituteData;

        // A copy of the original request used to create the data source.
        // We have to copy the request because requests are mutable.
        ResourceRequest m_originalRequestCopy;

        // The 'working' request. It may be mutated
        // several times from the original request to include additional
        // headers, cookie information, canonicalization and redirects.
        ResourceRequest m_request;

        ResourceResponse m_response;

        ResourceError m_mainDocumentError;

        bool m_committed;
        bool m_isClientRedirect;
        bool m_replacesCurrentHistoryItem;

        String m_overrideEncoding;

        // The action that triggered loading - we keep this around for the
        // benefit of the various policy handlers.
        NavigationAction m_triggeringAction;

        OwnPtr<ArchiveResourceCollection> m_archiveResourceCollection;
        RefPtr<MHTMLArchive> m_archive;

        bool m_loadingMainResource;
        DocumentLoadTiming m_documentLoadTiming;

        double m_timeOfLastDataReceived;
        unsigned long m_identifierForLoadWithoutResourceLoader;

        DocumentLoaderTimer m_dataLoadTimer;

        friend class ApplicationCacheHost;  // for substitute resource delivery
        OwnPtr<ApplicationCacheHost> m_applicationCacheHost;
    };
}

#endif // DocumentLoader_h
