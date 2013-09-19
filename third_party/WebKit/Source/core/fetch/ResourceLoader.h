/*
 * Copyright (C) 2005, 2006, 2011 Apple Inc. All rights reserved.
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

#ifndef ResourceLoader_h
#define ResourceLoader_h

#include "core/fetch/ResourceLoaderOptions.h"
#include "core/platform/network/ResourceRequest.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class Resource;
class KURL;
class ResourceError;
class ResourceResponse;
class ResourceLoaderHost;

class ResourceLoader : public RefCounted<ResourceLoader>, protected WebKit::WebURLLoaderClient {
public:
    static PassRefPtr<ResourceLoader> create(ResourceLoaderHost*, Resource*, const ResourceRequest&, const ResourceLoaderOptions&);
    virtual ~ResourceLoader();

    void start();

    void cancel();
    void cancel(const ResourceError&);
    void cancelIfNotFinishing();

    Resource* cachedResource() { return m_resource; }
    const ResourceRequest& originalRequest() const { return m_originalRequest; }

    void setDefersLoading(bool);
    bool defersLoading() const { return m_defersLoading; }

    void releaseResources();

    void didChangePriority(ResourceLoadPriority);

    // WebURLLoaderClient
    virtual void willSendRequest(WebKit::WebURLLoader*, WebKit::WebURLRequest&, const WebKit::WebURLResponse& redirectResponse) OVERRIDE;
    virtual void didSendData(WebKit::WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent) OVERRIDE;
    virtual void didReceiveResponse(WebKit::WebURLLoader*, const WebKit::WebURLResponse&) OVERRIDE;
    virtual void didReceiveData(WebKit::WebURLLoader*, const char*, int, int encodedDataLength) OVERRIDE;
    virtual void didReceiveCachedMetadata(WebKit::WebURLLoader*, const char* data, int length) OVERRIDE;
    virtual void didFinishLoading(WebKit::WebURLLoader*, double finishTime) OVERRIDE;
    virtual void didFail(WebKit::WebURLLoader*, const WebKit::WebURLError&) OVERRIDE;
    virtual void didDownloadData(WebKit::WebURLLoader*, int, int) OVERRIDE;

    const KURL& url() const { return m_request.url(); }
    bool shouldSendResourceLoadCallbacks() const { return m_options.sendLoadCallbacks == SendCallbacks; }
    bool shouldSniffContent() const { return m_options.sniffContent == SniffContent; }
    bool isLoadedBy(ResourceLoaderHost*) const;

    bool reachedTerminalState() const { return m_state == Terminated; }
    const ResourceRequest& request() const { return m_request; }

private:
    ResourceLoader(ResourceLoaderHost*, Resource*, const ResourceLoaderOptions&);

    void init(const ResourceRequest&);
    void requestSynchronously();

    void didFinishLoadingOnePart(double finishTime);

    OwnPtr<WebKit::WebURLLoader> m_loader;
    RefPtr<ResourceLoaderHost> m_host;

    ResourceRequest m_request;
    ResourceRequest m_originalRequest; // Before redirects.

    bool m_notifiedLoadComplete;

    bool m_defersLoading;
    ResourceRequest m_deferredRequest;
    ResourceLoaderOptions m_options;

    enum ResourceLoaderState {
        Initialized,
        Finishing,
        Terminated
    };

    enum ConnectionState {
        ConnectionStateNew,
        ConnectionStateStarted,
        ConnectionStateReceivedResponse,
        ConnectionStateReceivingData,
        ConnectionStateFinishedLoading,
        ConnectionStateCanceled,
        ConnectionStateFailed,
    };

    class RequestCountTracker {
    public:
        RequestCountTracker(ResourceLoaderHost*, Resource*);
        ~RequestCountTracker();
    private:
        ResourceLoaderHost* m_host;
        Resource* m_resource;
    };

    Resource* m_resource;
    ResourceLoaderState m_state;

    // Used for sanity checking to make sure we don't experience illegal state
    // transitions.
    ConnectionState m_connectionState;

    OwnPtr<RequestCountTracker> m_requestCountTracker;
};

}

#endif
