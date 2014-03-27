/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#ifndef WebFrameClient_h
#define WebFrameClient_h

#include "WebDOMMessageEvent.h"
#include "WebDataSource.h"
#include "WebFrame.h"
#include "WebIconURL.h"
#include "WebNavigationPolicy.h"
#include "WebNavigationType.h"
#include "WebSecurityOrigin.h"
#include "WebTextDirection.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebFileSystem.h"
#include "public/platform/WebFileSystemType.h"
#include "public/platform/WebStorageQuotaCallbacks.h"
#include "public/platform/WebStorageQuotaType.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include <v8.h>

namespace blink {

class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebCachedURLRequest;
class WebContentDecryptionModule;
class WebCookieJar;
class WebDataSource;
class WebDOMEvent;
class WebFormElement;
class WebInputEvent;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebServiceWorkerProvider;
class WebServiceWorkerProviderClient;
class WebNode;
class WebPlugin;
class WebRTCPeerConnectionHandler;
class WebSharedWorker;
class WebSharedWorkerClient;
class WebSocketStreamHandle;
class WebString;
class WebURL;
class WebURLLoader;
class WebURLResponse;
class WebWorkerPermissionClientProxy;
struct WebContextMenuData;
struct WebPluginParams;
struct WebRect;
struct WebSize;
struct WebURLError;

class WebFrameClient {
public:
    // Factory methods -----------------------------------------------------

    // May return null.
    virtual WebPlugin* createPlugin(WebLocalFrame*, const WebPluginParams&) { return 0; }

    // May return null.
    virtual WebMediaPlayer* createMediaPlayer(WebLocalFrame*, const WebURL&, WebMediaPlayerClient*) { return 0; }

    // May return null.
    virtual WebContentDecryptionModule* createContentDecryptionModule(WebLocalFrame*, const WebSecurityOrigin&, const WebString& keySystem) { return 0; }

    // May return null.
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebLocalFrame*, WebApplicationCacheHostClient*) { return 0; }

    // May return null. Takes ownership of the client.
    // FIXME: Deprecate the second argument.
    virtual WebServiceWorkerProvider* createServiceWorkerProvider(WebLocalFrame*, WebServiceWorkerProviderClient*) { return 0; }
    virtual WebServiceWorkerProvider* createServiceWorkerProvider(WebLocalFrame* frame) { return createServiceWorkerProvider(frame, 0); }

    // May return null.
    virtual WebWorkerPermissionClientProxy* createWorkerPermissionClientProxy(WebLocalFrame*) { return 0; }


    // Services ------------------------------------------------------------

    // A frame specific cookie jar.  May return null, in which case
    // WebKitPlatformSupport::cookieJar() will be called to access cookies.
    virtual WebCookieJar* cookieJar(WebLocalFrame*) { return 0; }


    // General notifications -----------------------------------------------

    // Indicates that another page has accessed the DOM of the initial empty
    // document of a main frame. After this, it is no longer safe to show a
    // pending navigation's URL, because a URL spoof is possible.
    virtual void didAccessInitialDocument(WebLocalFrame*) { }

    // A child frame was created in this frame. This is called when the frame
    // is created and initialized. Takes the name of the new frame, the parent
    // frame and returns a new WebFrame. The WebFrame is considered in-use
    // until frameDetached() is called on it.
    // Note: If you override this, you should almost certainly be overriding
    // frameDetached().
    virtual WebFrame* createChildFrame(WebLocalFrame* parent, const WebString& frameName) { return 0; }

    // This frame set its opener to null, disowning it.
    // See http://html.spec.whatwg.org/#dom-opener.
    virtual void didDisownOpener(WebLocalFrame*) { }

    // This frame has been detached from the view, but has not been closed yet.
    virtual void frameDetached(WebFrame*) { }

    // This frame has become focused..
    virtual void frameFocused() { }

    // This frame is about to be closed. This is called after frameDetached,
    // when the document is being unloaded, due to new one committing.
    virtual void willClose(WebLocalFrame*) { }

    // This frame's name has changed.
    virtual void didChangeName(WebLocalFrame*, const WebString&) { }

    // Called when a watched CSS selector matches or stops matching.
    virtual void didMatchCSS(WebLocalFrame*, const WebVector<WebString>& newlyMatchingSelectors, const WebVector<WebString>& stoppedMatchingSelectors) { }

    // Load commands -------------------------------------------------------

    // The client should handle the navigation externally.
    virtual void loadURLExternally(
        WebLocalFrame*, const WebURLRequest&, WebNavigationPolicy) { }
    virtual void loadURLExternally(
        WebLocalFrame*, const WebURLRequest&, WebNavigationPolicy, const WebString& downloadName) { }


    // Navigational queries ------------------------------------------------

    // The client may choose to alter the navigation policy.  Otherwise,
    // defaultPolicy should just be returned.
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebLocalFrame*, WebDataSource::ExtraData*, const WebURLRequest&, WebNavigationType,
        WebNavigationPolicy defaultPolicy, bool isRedirect) { return defaultPolicy; }


    // Navigational notifications ------------------------------------------

    // A form submission has been requested, but the page's submit event handler
    // hasn't yet had a chance to run (and possibly alter/interrupt the submit.)
    virtual void willSendSubmitEvent(WebLocalFrame*, const WebFormElement&) { }

    // A form submission is about to occur.
    virtual void willSubmitForm(WebLocalFrame*, const WebFormElement&) { }

    // A datasource has been created for a new navigation.  The given
    // datasource will become the provisional datasource for the frame.
    virtual void didCreateDataSource(WebLocalFrame*, WebDataSource*) { }

    // A new provisional load has been started.
    virtual void didStartProvisionalLoad(WebLocalFrame*) { }

    // The provisional load was redirected via a HTTP 3xx response.
    virtual void didReceiveServerRedirectForProvisionalLoad(WebLocalFrame*) { }

    // The provisional load failed.
    virtual void didFailProvisionalLoad(WebLocalFrame*, const WebURLError&) { }

    // The provisional datasource is now committed.  The first part of the
    // response body has been received, and the encoding of the response
    // body is known.
    virtual void didCommitProvisionalLoad(WebLocalFrame*, bool isNewNavigation) { }

    // The window object for the frame has been cleared of any extra
    // properties that may have been set by script from the previously
    // loaded document.
    virtual void didClearWindowObject(WebLocalFrame* frame, int worldId) { }

    // The document element has been created.
    virtual void didCreateDocumentElement(WebLocalFrame*) { }

    // The page title is available.
    virtual void didReceiveTitle(WebLocalFrame* frame, const WebString& title, WebTextDirection direction) { }

    // The icon for the page have changed.
    virtual void didChangeIcon(WebLocalFrame*, WebIconURL::Type) { }

    // The frame's document finished loading.
    virtual void didFinishDocumentLoad(WebLocalFrame*) { }

    // The 'load' event was dispatched.
    virtual void didHandleOnloadEvents(WebLocalFrame*) { }

    // The frame's document or one of its subresources failed to load.
    virtual void didFailLoad(WebLocalFrame*, const WebURLError&) { }

    // The frame's document and all of its subresources succeeded to load.
    virtual void didFinishLoad(WebLocalFrame*) { }

    // The navigation resulted in no change to the documents within the page.
    // For example, the navigation may have just resulted in scrolling to a
    // named anchor or a PopState event may have been dispatched.
    virtual void didNavigateWithinPage(WebLocalFrame*, bool isNewNavigation) { }

    // Called upon update to scroll position, document state, and other
    // non-navigational events related to the data held by WebHistoryItem.
    // WARNING: This method may be called very frequently.
    virtual void didUpdateCurrentHistoryItem(WebLocalFrame*) { }

    // Editing -------------------------------------------------------------

    // These methods allow the client to intercept and overrule editing
    // operations.
    virtual void didChangeSelection(bool isSelectionEmpty) { }


    // UI ------------------------------------------------------------------

    // Shows a context menu with commands relevant to a specific element on
    // the given frame. Additional context data is supplied.
    virtual void showContextMenu(const WebContextMenuData&) { }

    // Called when the data attached to the currently displayed context menu is
    // invalidated. The context menu may be closed if possible.
    virtual void clearContextMenu() { }


    // Low-level resource notifications ------------------------------------

    // An element will request a resource.
    virtual void willRequestResource(WebLocalFrame*, const WebCachedURLRequest&) { }

    // The request is after preconnect is triggered.
    virtual void willRequestAfterPreconnect(WebLocalFrame*, WebURLRequest&) { }

    // A request is about to be sent out, and the client may modify it.  Request
    // is writable, and changes to the URL, for example, will change the request
    // made.  If this request is the result of a redirect, then redirectResponse
    // will be non-null and contain the response that triggered the redirect.
    virtual void willSendRequest(
        WebLocalFrame*, unsigned identifier, WebURLRequest&,
        const WebURLResponse& redirectResponse) { }

    // Response headers have been received for the resource request given
    // by identifier.
    virtual void didReceiveResponse(
        WebLocalFrame*, unsigned identifier, const WebURLResponse&) { }

    virtual void didChangeResourcePriority(
        WebLocalFrame*, unsigned identifier, const blink::WebURLRequest::Priority&) { }

    virtual void didChangeResourcePriority(
        WebFrame* webFrame, unsigned identifier, const blink::WebURLRequest::Priority& priority, int) { didChangeResourcePriority(webFrame, identifier, priority); }

    // The resource request given by identifier succeeded.
    virtual void didFinishResourceLoad(
        WebLocalFrame*, unsigned identifier) { }

    // The specified request was satified from WebCore's memory cache.
    virtual void didLoadResourceFromMemoryCache(
        WebLocalFrame*, const WebURLRequest&, const WebURLResponse&) { }

    // This frame has displayed inactive content (such as an image) from an
    // insecure source.  Inactive content cannot spread to other frames.
    virtual void didDisplayInsecureContent(WebLocalFrame*) { }

    // The indicated security origin has run active content (such as a
    // script) from an insecure source.  Note that the insecure content can
    // spread to other frames in the same origin.
    virtual void didRunInsecureContent(WebLocalFrame*, const WebSecurityOrigin&, const WebURL& insecureURL) { }

    // A reflected XSS was encountered in the page and suppressed.
    virtual void didDetectXSS(WebLocalFrame*, const WebURL&, bool didBlockEntirePage) { }

    // A PingLoader was created, and a request dispatched to a URL.
    virtual void didDispatchPingLoader(WebLocalFrame*, const WebURL&) { }

    // The loaders in this frame have been stopped.
    virtual void didAbortLoading(WebLocalFrame*) { }

    // Script notifications ------------------------------------------------

    // Notifies that a new script context has been created for this frame.
    // This is similar to didClearWindowObject but only called once per
    // frame context.
    virtual void didCreateScriptContext(WebLocalFrame*, v8::Handle<v8::Context>, int extensionGroup, int worldId) { }

    // WebKit is about to release its reference to a v8 context for a frame.
    virtual void willReleaseScriptContext(WebLocalFrame*, v8::Handle<v8::Context>, int worldId) { }

    // Geometry notifications ----------------------------------------------

    // The frame's document finished the initial non-empty layout of a page.
    virtual void didFirstVisuallyNonEmptyLayout(WebLocalFrame*) { }

    // The size of the content area changed.
    virtual void didChangeContentsSize(WebLocalFrame*, const WebSize&) { }

    // The main frame scrolled.
    virtual void didChangeScrollOffset(WebLocalFrame*) { }

    // If the frame is loading an HTML document, this will be called to
    // notify that the <body> will be attached soon.
    virtual void willInsertBody(WebLocalFrame*) { }

    // Find-in-page notifications ------------------------------------------

    // Notifies how many matches have been found so far, for a given
    // identifier.  |finalUpdate| specifies whether this is the last update
    // (all frames have completed scoping).
    virtual void reportFindInPageMatchCount(
        int identifier, int count, bool finalUpdate) { }

    // Notifies what tick-mark rect is currently selected.   The given
    // identifier lets the client know which request this message belongs
    // to, so that it can choose to ignore the message if it has moved on
    // to other things.  The selection rect is expected to have coordinates
    // relative to the top left corner of the web page area and represent
    // where on the screen the selection rect is currently located.
    virtual void reportFindInPageSelection(
        int identifier, int activeMatchOrdinal, const WebRect& selection) { }

    // Quota ---------------------------------------------------------

    // Requests a new quota size for the origin's storage.
    // |newQuotaInBytes| indicates how much storage space (in bytes) the
    // caller expects to need.
    // WebStorageQuotaCallbacks::didGrantStorageQuota will be called when
    // a new quota is granted. WebStorageQuotaCallbacks::didFail
    // is called with an error code otherwise.
    // Note that the requesting quota size may not always be granted and
    // a smaller amount of quota than requested might be returned.
    virtual void requestStorageQuota(
        WebLocalFrame*, WebStorageQuotaType,
        unsigned long long newQuotaInBytes,
        WebStorageQuotaCallbacks) { }

    // WebSocket -----------------------------------------------------

    // A WebSocket object is going to open new stream connection.
    virtual void willOpenSocketStream(WebSocketStreamHandle*) { }

    // MediaStream -----------------------------------------------------

    // A new WebRTCPeerConnectionHandler is created.
    virtual void willStartUsingPeerConnectionHandler(WebLocalFrame*, WebRTCPeerConnectionHandler*) { }

    // Messages ------------------------------------------------------

    // Notifies the embedder that a postMessage was issued on this frame, and
    // gives the embedder a chance to handle it instead of WebKit. Returns true
    // if the embedder handled it.
    virtual bool willCheckAndDispatchMessageEvent(
        WebLocalFrame* sourceFrame,
        WebFrame* targetFrame,
        WebSecurityOrigin target,
        WebDOMMessageEvent event) { return false; }

    // Asks the embedder if a specific user agent should be used for the given
    // URL. Non-empty strings indicate an override should be used. Otherwise,
    // Platform::current()->userAgent() will be called to provide one.
    virtual WebString userAgentOverride(WebLocalFrame*, const WebURL& url) { return WebString(); }

    // Asks the embedder what value the network stack will send for the DNT
    // header. An empty string indicates that no DNT header will be send.
    virtual WebString doNotTrackValue(WebLocalFrame*) { return WebString(); }

    // WebGL ------------------------------------------------------

    // Asks the embedder whether WebGL is allowed for the given WebFrame.
    // This call is placed here instead of WebPermissionClient because this
    // class is implemented in content/, and putting it here avoids adding
    // more public content/ APIs.
    virtual bool allowWebGL(WebLocalFrame*, bool defaultValue) { return defaultValue; }

    // Notifies the client that a WebGL context was lost on this page with the
    // given reason (one of the GL_ARB_robustness status codes; see
    // Extensions3D.h in WebCore/platform/graphics).
    virtual void didLoseWebGLContext(WebLocalFrame*, int) { }

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    virtual void forwardInputEvent(const WebInputEvent*) { }

    // Send initial drawing parameters to a child frame that is being rendered out of process.
    virtual void initializeChildFrame(const WebRect& frameRect, float scaleFactor) { }

protected:
    ~WebFrameClient() { }
};

} // namespace blink

#endif
