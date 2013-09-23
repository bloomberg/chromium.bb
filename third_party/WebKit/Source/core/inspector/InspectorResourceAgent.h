/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorResourceAgent_h
#define InspectorResourceAgent_h

#include "InspectorFrontend.h"
#include "bindings/v8/ScriptString.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"


namespace WTF {
class String;
}

namespace WebCore {

class Resource;
struct FetchInitiatorInfo;
class Document;
class DocumentLoader;
class FormData;
class Frame;
class HTTPHeaderMap;
class InspectorClient;
class InspectorFrontend;
class InspectorOverlay;
class InspectorPageAgent;
class InstrumentingAgents;
class JSONObject;
class KURL;
class NetworkResourcesData;
class Page;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class ThreadableLoaderClient;
class XHRReplayData;

struct WebSocketFrame;
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;

typedef String ErrorString;

class InspectorResourceAgent : public InspectorBaseAgent<InspectorResourceAgent>, public InspectorBackendDispatcher::NetworkCommandHandler {
public:
    static PassOwnPtr<InspectorResourceAgent> create(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorClient* client, InspectorCompositeState* state, InspectorOverlay* overlay)
    {
        return adoptPtr(new InspectorResourceAgent(instrumentingAgents, pageAgent, client, state, overlay));
    }

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    static PassRefPtr<InspectorResourceAgent> restore(Page*, InspectorCompositeState*, InspectorFrontend*);

    ~InspectorResourceAgent();

    // Called from instrumentation.
    void willSendRequest(unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, const FetchInitiatorInfo&);
    void markResourceAsCached(unsigned long identifier);
    void didReceiveResourceResponse(unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    void didFinishLoading(unsigned long identifier, DocumentLoader*, double monotonicFinishTime);
    void didReceiveCORSRedirectResponse(unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didFailLoading(unsigned long identifier, DocumentLoader*, const ResourceError&);
    void didCommitLoad(Frame*, DocumentLoader*);
    void scriptImported(unsigned long identifier, const String& sourceString);
    void didReceiveScriptResponse(unsigned long identifier);

    void documentThreadableLoaderStartedLoadingForClient(unsigned long identifier, ThreadableLoaderClient*);
    void willLoadXHR(ThreadableLoaderClient*, const String& method, const KURL&, bool async, PassRefPtr<FormData> body, const HTTPHeaderMap& headers, bool includeCrendentials);
    void didFailXHRLoading(ThreadableLoaderClient*);
    void didFinishXHRLoading(ThreadableLoaderClient*, unsigned long identifier, ScriptString sourceString, const String&, const String&, unsigned);
    void didReceiveXHRResponse(unsigned long identifier);

    void willDestroyResource(Resource*);

    void applyUserAgentOverride(String* userAgent);

    // FIXME: InspectorResourceAgent should not be aware of style recalculation.
    void willRecalculateStyle(Document*);
    void didRecalculateStyle();
    void didScheduleStyleRecalculation(Document*);

    void frameScheduledNavigation(Frame*, double);
    void frameClearedScheduledNavigation(Frame*);

    PassRefPtr<TypeBuilder::Network::Initiator> buildInitiatorObject(Document*, const FetchInitiatorInfo&);

    void didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const String&);
    void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest&);
    void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeResponse&);
    void didCloseWebSocket(Document*, unsigned long identifier);
    void didReceiveWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didSendWebSocketFrame(unsigned long identifier, const WebSocketFrame&);
    void didReceiveWebSocketFrameError(unsigned long identifier, const String&);

    // called from Internals for layout test purposes.
    void setResourcesDataSizeLimitsFromInternals(int maximumResourcesContentSize, int maximumSingleResourceContentSize);

    // Called from frontend
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void setUserAgentOverride(ErrorString*, const String& userAgent);
    virtual void setExtraHTTPHeaders(ErrorString*, const RefPtr<JSONObject>&);
    virtual void getResponseBody(ErrorString*, const String& requestId, String* content, bool* base64Encoded);

    virtual void replayXHR(ErrorString*, const String& requestId);

    virtual void canClearBrowserCache(ErrorString*, bool*);
    virtual void clearBrowserCache(ErrorString*);
    virtual void canClearBrowserCookies(ErrorString*, bool*);
    virtual void clearBrowserCookies(ErrorString*);
    virtual void setCacheDisabled(ErrorString*, bool cacheDisabled);

    virtual void loadResourceForFrontend(ErrorString*, const String& frameId, const String& url, const RefPtr<JSONObject>* requestHeaders, PassRefPtr<LoadResourceForFrontendCallback>);

    // Called from other agents.
    bool fetchResourceContent(Frame*, const KURL&, String* content, bool* base64Encoded);

private:
    InspectorResourceAgent(InstrumentingAgents*, InspectorPageAgent*, InspectorClient*, InspectorCompositeState*, InspectorOverlay*);

    void enable();

    InspectorPageAgent* m_pageAgent;
    InspectorClient* m_client;
    InspectorOverlay* m_overlay;
    InspectorFrontend::Network* m_frontend;
    String m_userAgentOverride;
    OwnPtr<NetworkResourcesData> m_resourcesData;

    typedef HashMap<ThreadableLoaderClient*, RefPtr<XHRReplayData> > PendingXHRReplayDataMap;
    PendingXHRReplayDataMap m_pendingXHRReplayData;

    typedef HashMap<String, RefPtr<TypeBuilder::Network::Initiator> > FrameNavigationInitiatorMap;
    FrameNavigationInitiatorMap m_frameNavigationInitiatorMap;

    // FIXME: InspectorResourceAgent should now be aware of style recalculation.
    RefPtr<TypeBuilder::Network::Initiator> m_styleRecalculationInitiator;
    bool m_isRecalculatingStyle;
};

} // namespace WebCore


#endif // !defined(InspectorResourceAgent_h)
