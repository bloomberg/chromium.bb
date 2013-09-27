/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef FrameLoaderClientImpl_h
#define FrameLoaderClientImpl_h

#include "core/loader/FrameLoaderClient.h"
#include "weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace WebKit {

class WebFrameImpl;
class WebPluginContainerImpl;
class WebPluginLoadObserver;

class FrameLoaderClientImpl : public WebCore::FrameLoaderClient {
public:
    FrameLoaderClientImpl(WebFrameImpl* webFrame);
    ~FrameLoaderClientImpl();

    WebFrameImpl* webFrame() const { return m_webFrame; }

    // WebCore::FrameLoaderClient ----------------------------------------------

    virtual void frameLoaderDestroyed();

    // Notifies the WebView delegate that the JS window object has been cleared,
    // giving it a chance to bind native objects to the window before script
    // parsing begins.
    virtual void dispatchDidClearWindowObjectInWorld(WebCore::DOMWrapperWorld*);
    virtual void documentElementAvailable();

    // Script in the page tried to allocate too much memory.
    virtual void didExhaustMemoryAvailableForScript();

    virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId);
    virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId);

    // Returns true if we should allow the given V8 extension to be added to
    // the script context at the currently loading page and given extension group.
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId);

    virtual bool hasWebView() const;
    virtual bool hasFrameView() const;
    virtual void detachedFromParent();
    virtual void dispatchWillRequestAfterPreconnect(WebCore::ResourceRequest&);
    virtual void dispatchWillSendRequest(WebCore::DocumentLoader*, unsigned long identifier, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    virtual void dispatchDidReceiveResponse(WebCore::DocumentLoader*, unsigned long identifier, const WebCore::ResourceResponse&);
    virtual void dispatchDidChangeResourcePriority(unsigned long identifier, WebCore::ResourceLoadPriority);
    virtual void dispatchDidFinishLoading(WebCore::DocumentLoader*, unsigned long identifier);
    virtual void dispatchDidLoadResourceFromMemoryCache(const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
    virtual void dispatchDidHandleOnloadEvents();
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad();
    virtual void dispatchDidNavigateWithinPage();
    virtual void dispatchWillClose();
    virtual void dispatchDidStartProvisionalLoad();
    virtual void dispatchDidReceiveTitle(const String&);
    virtual void dispatchDidChangeIcons(WebCore::IconType);
    virtual void dispatchDidCommitLoad();
    virtual void dispatchDidFailProvisionalLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFailLoad(const WebCore::ResourceError&);
    virtual void dispatchDidFinishDocumentLoad();
    virtual void dispatchDidFinishLoad();
    virtual void dispatchDidFirstVisuallyNonEmptyLayout() OVERRIDE;
    virtual WebCore::NavigationPolicy decidePolicyForNavigation(const WebCore::ResourceRequest&, WebCore::DocumentLoader*, WebCore::NavigationPolicy);
    virtual void dispatchWillRequestResource(WebCore::FetchRequest*);
    virtual void dispatchWillSendSubmitEvent(PassRefPtr<WebCore::FormState>);
    virtual void dispatchWillSubmitForm(PassRefPtr<WebCore::FormState>);
    virtual void postProgressStartedNotification();
    virtual void postProgressEstimateChangedNotification();
    virtual void postProgressFinishedNotification();
    virtual void loadURLExternally(const WebCore::ResourceRequest&, WebCore::NavigationPolicy, const String& suggestedName = String());
    virtual void navigateBackForward(int offset) const;
    virtual void didAccessInitialDocument();
    virtual void didDisownOpener();
    virtual void didDisplayInsecureContent();
    virtual void didRunInsecureContent(WebCore::SecurityOrigin*, const WebCore::KURL& insecureURL);
    virtual void didDetectXSS(const WebCore::KURL&, bool didBlockEntirePage);
    virtual void didDispatchPingLoader(const WebCore::KURL&);
    virtual PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(
        const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    virtual WTF::String userAgent(const WebCore::KURL&);
    virtual WTF::String doNotTrackValue();
    virtual void transitionToCommittedForNewPage();
    virtual PassRefPtr<WebCore::Frame> createFrame(const WebCore::KURL&, const WTF::String& name, const WTF::String& referrer, WebCore::HTMLFrameOwnerElement*);
    virtual PassRefPtr<WebCore::Widget> createPlugin(
        const WebCore::IntSize&, WebCore::HTMLPlugInElement*, const WebCore::KURL&,
        const Vector<WTF::String>&, const Vector<WTF::String>&,
        const WTF::String&, bool loadManually);
    virtual PassRefPtr<WebCore::Widget> createJavaAppletWidget(
        const WebCore::IntSize&,
        WebCore::HTMLAppletElement*,
        const WebCore::KURL& /* base_url */,
        const Vector<WTF::String>& paramNames,
        const Vector<WTF::String>& paramValues);
    virtual WebCore::ObjectContentType objectContentType(
        const WebCore::KURL&, const WTF::String& mimeType, bool shouldPreferPlugInsForImages);
    virtual void didChangeScrollOffset();
    virtual bool allowScript(bool enabledPerSettings);
    virtual bool allowScriptFromSource(bool enabledPerSettings, const WebCore::KURL& scriptURL);
    virtual bool allowPlugins(bool enabledPerSettings);
    virtual bool allowImage(bool enabledPerSettings, const WebCore::KURL& imageURL);
    virtual bool allowDisplayingInsecureContent(bool enabledPerSettings, WebCore::SecurityOrigin*, const WebCore::KURL&);
    virtual bool allowRunningInsecureContent(bool enabledPerSettings, WebCore::SecurityOrigin*, const WebCore::KURL&);
    virtual void didNotAllowScript();
    virtual void didNotAllowPlugins();

    virtual WebCookieJar* cookieJar() const;
    virtual bool willCheckAndDispatchMessageEvent(WebCore::SecurityOrigin* target, WebCore::MessageEvent*) const;
    virtual void didChangeName(const String&);

    virtual void dispatchWillOpenSocketStream(WebCore::SocketStreamHandle*) OVERRIDE;

    virtual void dispatchWillStartUsingPeerConnectionHandler(WebCore::RTCPeerConnectionHandler*) OVERRIDE;

    virtual void didRequestAutocomplete(PassRefPtr<WebCore::FormState>) OVERRIDE;

    virtual bool allowWebGL(bool enabledPerSettings) OVERRIDE;
    virtual void didLoseWebGLContext(int arbRobustnessContextLostReason) OVERRIDE;

    virtual void dispatchWillInsertBody() OVERRIDE;

    virtual WebServiceWorkerRegistry* serviceWorkerRegistry() OVERRIDE;

    virtual void didStopAllLoaders() OVERRIDE;

private:
    PassOwnPtr<WebPluginLoadObserver> pluginLoadObserver();

    // The WebFrame that owns this object and manages its lifetime. Therefore,
    // the web frame object is guaranteed to exist.
    WebFrameImpl* m_webFrame;
};

} // namespace WebKit

#endif
