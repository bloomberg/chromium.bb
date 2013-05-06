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

#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentWriter.h"
#include "core/loader/NavigationAction.h"
#include "core/loader/ResourceLoaderOptions.h"
#include "core/loader/SubstituteData.h"
#include "core/loader/cache/CachedRawResource.h"
#include "core/loader/cache/CachedResourceHandle.h"
#include "core/platform/Timer.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/platform/network/ResourceResponse.h"
#include "core/platform/text/StringWithDirection.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WTF {
class SchedulePair;
}

namespace WebCore {
    class ApplicationCacheHost;
    class ArchiveResource;
    class ArchiveResourceCollection;
    class CachedResourceLoader;
    class ContentFilter;
    class FormState;
    class Frame;
    class FrameLoader;
    class MHTMLArchive;
    class Page;
    class ResourceLoader;
    class SharedBuffer;
    class SubstituteResource;

    typedef HashSet<RefPtr<ResourceLoader> > ResourceLoaderSet;

    class DocumentLoader : public RefCounted<DocumentLoader>, private CachedRawResourceClient {
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
        ResourceLoader* mainResourceLoader() const;
        PassRefPtr<SharedBuffer> mainResourceData() const;
        
        DocumentWriter* writer() const { return &m_writer; }

        const ResourceRequest& originalRequest() const;
        const ResourceRequest& originalRequestCopy() const;

        const ResourceRequest& request() const;
        ResourceRequest& request();

        CachedResourceLoader* cachedResourceLoader() const { return m_cachedResourceLoader.get(); }

        const SubstituteData& substituteData() const { return m_substituteData; }

        // FIXME: This is the same as requestURL(). We should remove one of them.
        const KURL& url() const;
        const KURL& unreachableURL() const;

        const KURL& originalURL() const;
        const KURL& requestURL() const;
        const String& responseMIMEType() const;

        void replaceRequestURLForSameDocumentNavigation(const KURL&);
        bool isStopping() const { return m_isStopping; }
        void stopLoading();
        void setCommitted(bool committed) { m_committed = committed; }
        bool isCommitted() const { return m_committed; }
        bool isLoading() const;
        const ResourceResponse& response() const { return m_response; }
        const ResourceError& mainDocumentError() const { return m_mainDocumentError; }
        bool isClientRedirect() const { return m_isClientRedirect; }
        void setIsClientRedirect(bool isClientRedirect) { m_isClientRedirect = isClientRedirect; }
        void handledOnloadEvents();
        bool wasOnloadHandled() { return m_wasOnloadHandled; }
        bool isLoadingInAPISense() const;
        void setTitle(const StringWithDirection&);
        const String& overrideEncoding() const { return m_overrideEncoding; }

        bool scheduleArchiveLoad(ResourceLoader*, const ResourceRequest&);

#ifndef NDEBUG
        bool isSubstituteLoadPending(ResourceLoader*) const;
#endif
        void cancelPendingSubstituteLoad(ResourceLoader*);

        bool shouldContinueForNavigationPolicy(const ResourceRequest&);
        const NavigationAction& triggeringAction() const { return m_triggeringAction; }
        void setTriggeringAction(const NavigationAction& action) { m_triggeringAction = action; }

        void setOverrideEncoding(const String& encoding) { m_overrideEncoding = encoding; }
        const StringWithDirection& title() const { return m_pageTitle; }

        KURL urlForHistory() const;
        
        void setDefersLoading(bool);
        void setMainResourceDataBufferingPolicy(DataBufferingPolicy);

        void startLoadingMainResource();
        void cancelMainResourceLoad(const ResourceError&);

        bool isLoadingMainResource() const { return m_loadingMainResource; }
        bool isLoadingMultipartContent() const { return m_isLoadingMultipartContent; }

        void stopLoadingSubresources();
        void addResourceLoader(ResourceLoader*);
        void removeResourceLoader(ResourceLoader*);

        void subresourceLoaderFinishedLoadingOnePart(ResourceLoader*);

        void setDeferMainResourceDataLoad(bool defer) { m_deferMainResourceDataLoad = defer; }
        
        void didTellClientAboutLoad(const String& url)
        { 
            // Don't include data urls here, as if a lot of data is loaded
            // that way, we hold on to the (large) url string for too long.
            if (protocolIs(url, "data"))
                return;
            if (!url.isEmpty())
                m_resourcesClientKnowsAbout.add(url);
        }
        bool haveToldClientAboutLoad(const String& url) { return m_resourcesClientKnowsAbout.contains(url); }
        void recordMemoryCacheLoadForFutureClientNotification(const String& url);
        void takeMemoryCacheLoadsForClientNotification(Vector<String>& loads);

        DocumentLoadTiming* timing() { return &m_documentLoadTiming; }
        void resetTiming() { m_documentLoadTiming = DocumentLoadTiming(); }

        // The WebKit layer calls this function when it's ready for the data to
        // actually be added to the document.
        void commitData(const char* bytes, size_t length);

        ApplicationCacheHost* applicationCacheHost() const { return m_applicationCacheHost.get(); }

        virtual void reportMemoryUsage(MemoryObjectInfo*) const;
        void checkLoadComplete();

    protected:
        DocumentLoader(const ResourceRequest&, const SubstituteData&);

        bool m_deferMainResourceDataLoad;

    private:

        // The URL of the document resulting from this DocumentLoader.
        KURL documentURL() const;
        Document* document() const;

        void setRequest(const ResourceRequest&);

        void commitIfReady();
        void setMainDocumentError(const ResourceError&);
        void commitLoad(const char*, int);
        void clearMainResourceLoader();

        void setupForReplace();
        void maybeFinishLoadingMultipartContent();
        
        bool maybeCreateArchive();
        void clearArchiveResources();

        void prepareSubframeArchiveLoadIfNeeded();
        void addAllArchiveResources(MHTMLArchive*);
        ArchiveResource* archiveResourceForURL(const KURL&) const;

        void willSendRequest(ResourceRequest&, const ResourceResponse&);
        void finishedLoading(double finishTime);
        void mainReceivedError(const ResourceError&);
        virtual void redirectReceived(CachedResource*, ResourceRequest&, const ResourceResponse&) OVERRIDE;
        virtual void responseReceived(CachedResource*, const ResourceResponse&) OVERRIDE;
        virtual void dataReceived(CachedResource*, const char* data, int length) OVERRIDE;
        virtual void notifyFinished(CachedResource*) OVERRIDE;

        bool maybeLoadEmpty();

        bool isMultipartReplacingLoad() const;
        bool isPostOrRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

        bool shouldContinueForResponse() const;
        void stopLoadingForPolicyChange();
        ResourceError interruptedForPolicyChangeError() const;

        typedef Timer<DocumentLoader> DocumentLoaderTimer;

        void handleSubstituteDataLoadSoon();
        void handleSubstituteDataLoadNow(DocumentLoaderTimer*);
        void startDataLoadTimer();

        void deliverSubstituteResourcesAfterDelay();
        void substituteResourceDeliveryTimerFired(Timer<DocumentLoader>*);
                
        Frame* m_frame;
        RefPtr<CachedResourceLoader> m_cachedResourceLoader;

        CachedResourceHandle<CachedRawResource> m_mainResource;
        ResourceLoaderSet m_resourceLoaders;
        ResourceLoaderSet m_multipartResourceLoaders;
        
        mutable DocumentWriter m_writer;

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

        bool m_originalSubstituteDataWasValid;
        bool m_committed;
        bool m_isStopping;
        bool m_gotFirstByte;
        bool m_isClientRedirect;
        bool m_isLoadingMultipartContent;

        // FIXME: Document::m_processingLoadEvent and DocumentLoader::m_wasOnloadHandled are roughly the same
        // and should be merged.
        bool m_wasOnloadHandled;

        StringWithDirection m_pageTitle;

        String m_overrideEncoding;

        // The action that triggered loading - we keep this around for the
        // benefit of the various policy handlers.
        NavigationAction m_triggeringAction;
        
        typedef HashMap<RefPtr<ResourceLoader>, RefPtr<SubstituteResource> > SubstituteResourceMap;
        SubstituteResourceMap m_pendingSubstituteResources;
        Timer<DocumentLoader> m_substituteResourceDeliveryTimer;

        OwnPtr<ArchiveResourceCollection> m_archiveResourceCollection;
        RefPtr<MHTMLArchive> m_archive;

        HashSet<String> m_resourcesClientKnowsAbout;
        Vector<String> m_resourcesLoadedFromMemoryCacheForClientNotification;

        bool m_loadingMainResource;
        DocumentLoadTiming m_documentLoadTiming;

        double m_timeOfLastDataReceived;
        unsigned long m_identifierForLoadWithoutResourceLoader;

        DocumentLoaderTimer m_dataLoadTimer;

        friend class ApplicationCacheHost;  // for substitute resource delivery
        OwnPtr<ApplicationCacheHost> m_applicationCacheHost;
    };

    inline void DocumentLoader::recordMemoryCacheLoadForFutureClientNotification(const String& url)
    {
        m_resourcesLoadedFromMemoryCacheForClientNotification.append(url);
    }

    inline void DocumentLoader::takeMemoryCacheLoadsForClientNotification(Vector<String>& loadsSet)
    {
        loadsSet.swap(m_resourcesLoadedFromMemoryCacheForClientNotification);
        m_resourcesLoadedFromMemoryCacheForClientNotification.clear();
    }

}

#endif // DocumentLoader_h
