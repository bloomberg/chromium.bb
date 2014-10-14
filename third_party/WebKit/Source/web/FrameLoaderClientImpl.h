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
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class WebLocalFrameImpl;
class WebPluginContainerImpl;
class WebPluginLoadObserver;

class FrameLoaderClientImpl final : public FrameLoaderClient {
public:
    explicit FrameLoaderClientImpl(WebLocalFrameImpl* webFrame);
    virtual ~FrameLoaderClientImpl();

    WebLocalFrameImpl* webFrame() const { return m_webFrame; }

    // FrameLoaderClient ----------------------------------------------

    // Notifies the WebView delegate that the JS window object has been cleared,
    // giving it a chance to bind native objects to the window before script
    // parsing begins.
    virtual void dispatchDidClearWindowObjectInMainWorld() override;
    virtual void documentElementAvailable() override;

    virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId) override;
    virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId) override;

    // Returns true if we should allow the given V8 extension to be added to
    // the script context at the currently loading page and given extension group.
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) override;

    virtual bool hasWebView() const override;
    virtual Frame* opener() const override;
    virtual void setOpener(Frame*) override;
    virtual Frame* parent() const override;
    virtual Frame* top() const override;
    virtual Frame* previousSibling() const override;
    virtual Frame* nextSibling() const override;
    virtual Frame* firstChild() const override;
    virtual Frame* lastChild() const override;
    virtual void detachedFromParent() override;
    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse) override;
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&) override;
    virtual void dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority, int intraPriorityValue) override;
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier) override;
    virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&) override;
    virtual void dispatchDidHandleOnloadEvents() override;
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    virtual void dispatchDidNavigateWithinPage(HistoryItem*, HistoryCommitType) override;
    virtual void dispatchWillClose() override;
    virtual void dispatchDidStartProvisionalLoad(bool isTransitionNavigation) override;
    virtual void dispatchDidReceiveTitle(const String&) override;
    virtual void dispatchDidChangeIcons(IconType) override;
    virtual void dispatchDidCommitLoad(LocalFrame*, HistoryItem*, HistoryCommitType) override;
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) override;
    virtual void dispatchDidFailLoad(const ResourceError&) override;
    virtual void dispatchDidFinishDocumentLoad() override;
    virtual void dispatchDidFinishLoad() override;
    virtual void dispatchDidFirstVisuallyNonEmptyLayout() override;

    virtual void dispatchDidChangeThemeColor() override;
    virtual NavigationPolicy decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationPolicy, bool isTransitionNavigation) override;
    virtual void dispatchAddNavigationTransitionData(const String& allowedDestinationOrigin, const String& selector, const String& markup) override;
    virtual void dispatchWillRequestResource(FetchRequest*) override;
    virtual void dispatchWillSendSubmitEvent(HTMLFormElement*) override;
    virtual void dispatchWillSubmitForm(HTMLFormElement*) override;
    virtual void didStartLoading(LoadStartType) override;
    virtual void didStopLoading() override;
    virtual void progressEstimateChanged(double progressEstimate) override;
    virtual void loadURLExternally(const ResourceRequest&, NavigationPolicy, const String& suggestedName = String()) override;
    virtual bool navigateBackForward(int offset) const override;
    virtual void didAccessInitialDocument() override;
    virtual void didDisplayInsecureContent() override;
    virtual void didRunInsecureContent(SecurityOrigin*, const KURL& insecureURL) override;
    virtual void didDetectXSS(const KURL&, bool didBlockEntirePage) override;
    virtual void didDispatchPingLoader(const KURL&) override;
    virtual void selectorMatchChanged(const Vector<String>& addedSelectors, const Vector<String>& removedSelectors) override;
    virtual PassRefPtr<DocumentLoader> createDocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&) override;
    virtual WTF::String userAgent(const KURL&) override;
    virtual WTF::String doNotTrackValue() override;
    virtual void transitionToCommittedForNewPage() override;
    virtual PassRefPtrWillBeRawPtr<LocalFrame> createFrame(const KURL&, const WTF::AtomicString& name, HTMLFrameOwnerElement*) override;
    virtual bool canCreatePluginWithoutRenderer(const String& mimeType) const;
    virtual PassOwnPtrWillBeRawPtr<PluginPlaceholder> createPluginPlaceholder(
        Document&, const KURL&,
        const Vector<String>& paramNames, const Vector<String>& paramValues,
        const String& mimeType, bool loadManually) override;
    virtual PassRefPtrWillBeRawPtr<Widget> createPlugin(
        HTMLPlugInElement*, const KURL&,
        const Vector<WTF::String>&, const Vector<WTF::String>&,
        const WTF::String&, bool loadManually, DetachedPluginPolicy) override;
    virtual PassRefPtrWillBeRawPtr<Widget> createJavaAppletWidget(
        HTMLAppletElement*,
        const KURL& /* base_url */,
        const Vector<WTF::String>& paramNames,
        const Vector<WTF::String>& paramValues) override;
    virtual ObjectContentType objectContentType(
        const KURL&, const WTF::String& mimeType, bool shouldPreferPlugInsForImages) override;
    virtual void didChangeScrollOffset() override;
    virtual void didUpdateCurrentHistoryItem() override;
    virtual void didRemoveAllPendingStylesheet() override;
    virtual bool allowScript(bool enabledPerSettings) override;
    virtual bool allowScriptFromSource(bool enabledPerSettings, const KURL& scriptURL) override;
    virtual bool allowPlugins(bool enabledPerSettings) override;
    virtual bool allowImage(bool enabledPerSettings, const KURL& imageURL) override;
    virtual bool allowMedia(const KURL& mediaURL) override;
    virtual bool allowDisplayingInsecureContent(bool enabledPerSettings, SecurityOrigin*, const KURL&) override;
    virtual bool allowRunningInsecureContent(bool enabledPerSettings, SecurityOrigin*, const KURL&) override;
    virtual void didNotAllowScript() override;
    virtual void didNotAllowPlugins() override;

    virtual WebCookieJar* cookieJar() const override;
    virtual bool willCheckAndDispatchMessageEvent(SecurityOrigin* target, MessageEvent*, LocalFrame* sourceFrame) const override;
    virtual void didChangeName(const String&) override;

    virtual void dispatchWillOpenWebSocket(WebSocketHandle*) override;

    virtual void dispatchWillStartUsingPeerConnectionHandler(WebRTCPeerConnectionHandler*) override;

    virtual void didRequestAutocomplete(HTMLFormElement*) override;

    virtual bool allowWebGL(bool enabledPerSettings) override;
    virtual void didLoseWebGLContext(int arbRobustnessContextLostReason) override;

    virtual void dispatchWillInsertBody() override;

    virtual PassOwnPtr<WebServiceWorkerProvider> createServiceWorkerProvider() override;
    virtual bool isControlledByServiceWorker(DocumentLoader&) override;
    virtual SharedWorkerRepositoryClient* sharedWorkerRepositoryClient() override;

    virtual PassOwnPtr<WebApplicationCacheHost> createApplicationCacheHost(WebApplicationCacheHostClient*) override;

    virtual void didStopAllLoaders() override;

    virtual void dispatchDidChangeManifest() override;

private:
    virtual bool isFrameLoaderClientImpl() const override { return true; }

    PassOwnPtr<WebPluginLoadObserver> pluginLoadObserver(DocumentLoader*);

    // The WebFrame that owns this object and manages its lifetime. Therefore,
    // the web frame object is guaranteed to exist.
    WebLocalFrameImpl* m_webFrame;
};

DEFINE_TYPE_CASTS(FrameLoaderClientImpl, FrameLoaderClient, client, client->isFrameLoaderClientImpl(), client.isFrameLoaderClientImpl());

} // namespace blink

#endif
