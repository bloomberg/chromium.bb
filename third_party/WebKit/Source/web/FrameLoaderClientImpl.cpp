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

#include "config.h"
#include "FrameLoaderClientImpl.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SharedWorkerRepositoryClientImpl.h"
#include "WebAutofillClient.h"
#include "WebCachedURLRequest.h"
#include "WebDOMEvent.h"
#include "WebDataSourceImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebDocument.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebNode.h"
#include "WebPermissionClient.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPluginLoadObserver.h"
#include "WebPluginParams.h"
#include "WebSecurityOrigin.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFullscreen.h"
#include "core/events/MessageEvent.h"
#include "core/events/MouseEvent.h"
#include "core/dom/WheelController.h"
#include "core/html/HTMLAppletElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/rendering/HitTestResult.h"
#include "modules/device_orientation/DeviceMotionController.h"
#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/gamepad/NavigatorGamepad.h"
#include "modules/screen_orientation/ScreenOrientationController.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/UserGestureIndicator.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/SocketStreamHandleInternal.h"
#include "platform/plugins/PluginData.h"
#include "public/platform/Platform.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebMimeRegistry.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebServiceWorkerProvider.h"
#include "public/platform/WebServiceWorkerProviderClient.h"
#include "public/platform/WebSocketStreamHandle.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

using namespace WebCore;

namespace blink {

FrameLoaderClientImpl::FrameLoaderClientImpl(WebFrameImpl* frame)
    : m_webFrame(frame)
{
}

FrameLoaderClientImpl::~FrameLoaderClientImpl()
{
}

void FrameLoaderClientImpl::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld* world)
{
    if (m_webFrame->client()) {
        m_webFrame->client()->didClearWindowObject(m_webFrame, world->worldId());
        Document* document = m_webFrame->frame()->document();
        if (document) {
            WheelController::from(*document);
            if (RuntimeEnabledFeatures::deviceMotionEnabled())
                DeviceMotionController::from(*document);
            if (RuntimeEnabledFeatures::deviceOrientationEnabled())
                DeviceOrientationController::from(*document);
            if (RuntimeEnabledFeatures::screenOrientationEnabled())
                ScreenOrientationController::from(*document);
            if (RuntimeEnabledFeatures::gamepadEnabled())
                NavigatorGamepad::from(*document);
        }
    }
}

void FrameLoaderClientImpl::documentElementAvailable()
{
    if (m_webFrame->client())
        m_webFrame->client()->didCreateDocumentElement(m_webFrame);
}

void FrameLoaderClientImpl::didCreateScriptContext(v8::Handle<v8::Context> context, int extensionGroup, int worldId)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview->devToolsAgentPrivate())
        webview->devToolsAgentPrivate()->didCreateScriptContext(m_webFrame, worldId);
    if (m_webFrame->client())
        m_webFrame->client()->didCreateScriptContext(m_webFrame, context, extensionGroup, worldId);
}

void FrameLoaderClientImpl::willReleaseScriptContext(v8::Handle<v8::Context> context, int worldId)
{
    if (m_webFrame->client())
        m_webFrame->client()->willReleaseScriptContext(m_webFrame, context, worldId);
}

bool FrameLoaderClientImpl::allowScriptExtension(const String& extensionName,
                                                 int extensionGroup,
                                                 int worldId)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowScriptExtension(m_webFrame, extensionName, extensionGroup, worldId);

    return true;
}

void FrameLoaderClientImpl::didChangeScrollOffset()
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeScrollOffset(m_webFrame);
}

void FrameLoaderClientImpl::didUpdateCurrentHistoryItem()
{
    if (m_webFrame->client())
        m_webFrame->client()->didUpdateCurrentHistoryItem(m_webFrame);
}

bool FrameLoaderClientImpl::allowScript(bool enabledPerSettings)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowScript(m_webFrame, enabledPerSettings);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowScriptFromSource(bool enabledPerSettings, const KURL& scriptURL)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowScriptFromSource(m_webFrame, enabledPerSettings, scriptURL);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowPlugins(bool enabledPerSettings)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowPlugins(m_webFrame, enabledPerSettings);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowImage(bool enabledPerSettings, const KURL& imageURL)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowImage(m_webFrame, enabledPerSettings, imageURL);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowDisplayingInsecureContent(bool enabledPerSettings, SecurityOrigin* context, const KURL& url)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowDisplayingInsecureContent(m_webFrame, enabledPerSettings, WebSecurityOrigin(context), WebURL(url));

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowRunningInsecureContent(bool enabledPerSettings, SecurityOrigin* context, const KURL& url)
{
    if (m_webFrame->permissionClient())
        return m_webFrame->permissionClient()->allowRunningInsecureContent(m_webFrame, enabledPerSettings, WebSecurityOrigin(context), WebURL(url));

    return enabledPerSettings;
}

void FrameLoaderClientImpl::didNotAllowScript()
{
    if (m_webFrame->permissionClient())
        m_webFrame->permissionClient()->didNotAllowScript(m_webFrame);
}

void FrameLoaderClientImpl::didNotAllowPlugins()
{
    if (m_webFrame->permissionClient())
        m_webFrame->permissionClient()->didNotAllowPlugins(m_webFrame);

}

bool FrameLoaderClientImpl::hasWebView() const
{
    return m_webFrame->viewImpl();
}

Frame* FrameLoaderClientImpl::opener() const
{
    WebFrameImpl* opener = toWebFrameImpl(m_webFrame->opener());
    return opener ? opener->frame() : 0;
}

void FrameLoaderClientImpl::setOpener(Frame* opener)
{
    // FIXME: Temporary hack to stage converting locations that really should be Frame.
    m_webFrame->setOpener(WebFrameImpl::fromFrame(toLocalFrame(opener)));
}

Frame* FrameLoaderClientImpl::parent() const
{
    WebFrameImpl* frame = toWebFrameImpl(m_webFrame->parent());
    return frame ? frame->frame() : 0;
}

Frame* FrameLoaderClientImpl::top() const
{
    WebFrameImpl* frame = toWebFrameImpl(m_webFrame->top());
    return frame ? frame->frame() : 0;
}

Frame* FrameLoaderClientImpl::previousSibling() const
{
    WebFrameImpl* frame = toWebFrameImpl(m_webFrame->previousSibling());
    return frame ? frame->frame() : 0;
}

Frame* FrameLoaderClientImpl::nextSibling() const
{
    WebFrameImpl* frame = toWebFrameImpl(m_webFrame->nextSibling());
    return frame ? frame->frame() : 0;
}

Frame* FrameLoaderClientImpl::firstChild() const
{
    WebFrameImpl* frame = toWebFrameImpl(m_webFrame->firstChild());
    return frame ? frame->frame() : 0;
}

Frame* FrameLoaderClientImpl::lastChild() const
{
    WebFrameImpl* frame = toWebFrameImpl(m_webFrame->lastChild());
    return frame ? frame->frame() : 0;
}

void FrameLoaderClientImpl::detachedFromParent()
{
    // Alert the client that the frame is being detached. This is the last
    // chance we have to communicate with the client.
    RefPtr<WebFrameImpl> protector(m_webFrame);

    WebFrameClient* client = m_webFrame->client();
    if (!client)
        return;

    m_webFrame->willDetachParent();

    // Signal that no further communication with WebFrameClient should take
    // place at this point since we are no longer associated with the Page.
    m_webFrame->setClient(0);

    client->frameDetached(m_webFrame);
    // Clear our reference to WebCore::LocalFrame at the very end, in case the client
    // refers to it.
    m_webFrame->setWebCoreFrame(nullptr);
}

void FrameLoaderClientImpl::dispatchWillRequestAfterPreconnect(ResourceRequest& request)
{
    if (m_webFrame->client()) {
        WrappedResourceRequest webreq(request);
        m_webFrame->client()->willRequestAfterPreconnect(m_webFrame, webreq);
    }
}

void FrameLoaderClientImpl::dispatchWillSendRequest(
    DocumentLoader* loader, unsigned long identifier, ResourceRequest& request,
    const ResourceResponse& redirectResponse)
{
    // Give the WebFrameClient a crack at the request.
    if (m_webFrame->client()) {
        WrappedResourceRequest webreq(request);
        WrappedResourceResponse webresp(redirectResponse);
        m_webFrame->client()->willSendRequest(
            m_webFrame, identifier, webreq, webresp);
    }
}

void FrameLoaderClientImpl::dispatchDidReceiveResponse(DocumentLoader* loader,
                                                       unsigned long identifier,
                                                       const ResourceResponse& response)
{
    if (m_webFrame->client()) {
        WrappedResourceResponse webresp(response);
        m_webFrame->client()->didReceiveResponse(m_webFrame, identifier, webresp);
    }
}

void FrameLoaderClientImpl::dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority priority, int intraPriorityValue)
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeResourcePriority(m_webFrame, identifier, static_cast<blink::WebURLRequest::Priority>(priority), intraPriorityValue);
}

// Called when a particular resource load completes
void FrameLoaderClientImpl::dispatchDidFinishLoading(DocumentLoader* loader,
                                                    unsigned long identifier)
{
    if (m_webFrame->client())
        m_webFrame->client()->didFinishResourceLoad(m_webFrame, identifier);
}

void FrameLoaderClientImpl::dispatchDidFinishDocumentLoad()
{
    if (m_webFrame->client())
        m_webFrame->client()->didFinishDocumentLoad(m_webFrame);
}

void FrameLoaderClientImpl::dispatchDidLoadResourceFromMemoryCache(const ResourceRequest& request, const ResourceResponse& response)
{
    if (m_webFrame->client())
        m_webFrame->client()->didLoadResourceFromMemoryCache(m_webFrame, WrappedResourceRequest(request), WrappedResourceResponse(response));
}

void FrameLoaderClientImpl::dispatchDidHandleOnloadEvents()
{
    if (m_webFrame->client())
        m_webFrame->client()->didHandleOnloadEvents(m_webFrame);
}

void FrameLoaderClientImpl::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    if (m_webFrame->client())
        m_webFrame->client()->didReceiveServerRedirectForProvisionalLoad(m_webFrame);
    m_webFrame->frame()->page()->historyController().removeChildrenForRedirect(m_webFrame->frame());
}

void FrameLoaderClientImpl::dispatchDidNavigateWithinPage(HistoryItem* item, HistoryCommitType commitType)
{
    bool shouldCreateHistoryEntry = commitType == StandardCommit;
    if (shouldCreateHistoryEntry)
        m_webFrame->frame()->page()->historyController().updateBackForwardListForFragmentScroll(m_webFrame->frame(), item);
    m_webFrame->viewImpl()->didCommitLoad(shouldCreateHistoryEntry, true);
    if (m_webFrame->client())
        m_webFrame->client()->didNavigateWithinPage(m_webFrame, shouldCreateHistoryEntry);
}

void FrameLoaderClientImpl::dispatchWillClose()
{
    if (m_webFrame->client())
        m_webFrame->client()->willClose(m_webFrame);
}

void FrameLoaderClientImpl::dispatchDidStartProvisionalLoad()
{
    if (m_webFrame->client())
        m_webFrame->client()->didStartProvisionalLoad(m_webFrame);
}

void FrameLoaderClientImpl::dispatchDidReceiveTitle(const String& title)
{
    if (m_webFrame->client())
        m_webFrame->client()->didReceiveTitle(m_webFrame, title, WebTextDirectionLeftToRight);
}

void FrameLoaderClientImpl::dispatchDidChangeIcons(WebCore::IconType type)
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeIcon(m_webFrame, static_cast<WebIconURL::Type>(type));
}

void FrameLoaderClientImpl::dispatchDidCommitLoad(LocalFrame* frame, HistoryItem* item, HistoryCommitType commitType)
{
    m_webFrame->frame()->page()->historyController().updateForCommit(frame, item, commitType);
    m_webFrame->viewImpl()->didCommitLoad(commitType == StandardCommit, false);
    if (m_webFrame->client())
        m_webFrame->client()->didCommitProvisionalLoad(m_webFrame, commitType == StandardCommit);
}

void FrameLoaderClientImpl::dispatchDidFailProvisionalLoad(
    const ResourceError& error)
{
    OwnPtr<WebPluginLoadObserver> observer = pluginLoadObserver(m_webFrame->frame()->loader().provisionalDocumentLoader());
    m_webFrame->didFail(error, true);
    if (observer)
        observer->didFailLoading(error);
}

void FrameLoaderClientImpl::dispatchDidFailLoad(const ResourceError& error)
{
    OwnPtr<WebPluginLoadObserver> observer = pluginLoadObserver(m_webFrame->frame()->loader().documentLoader());
    m_webFrame->didFail(error, false);
    if (observer)
        observer->didFailLoading(error);

    // Don't clear the redirect chain, this will happen in the middle of client
    // redirects, and we need the context. The chain will be cleared when the
    // provisional load succeeds or fails, not the "real" one.
}

void FrameLoaderClientImpl::dispatchDidFinishLoad()
{
    OwnPtr<WebPluginLoadObserver> observer = pluginLoadObserver(m_webFrame->frame()->loader().documentLoader());

    if (m_webFrame->client())
        m_webFrame->client()->didFinishLoad(m_webFrame);

    if (observer)
        observer->didFinishLoading();

    // Don't clear the redirect chain, this will happen in the middle of client
    // redirects, and we need the context. The chain will be cleared when the
    // provisional load succeeds or fails, not the "real" one.
}

void FrameLoaderClientImpl::dispatchDidFirstVisuallyNonEmptyLayout()
{
    if (m_webFrame->client())
        m_webFrame->client()->didFirstVisuallyNonEmptyLayout(m_webFrame);
}

NavigationPolicy FrameLoaderClientImpl::decidePolicyForNavigation(const ResourceRequest& request, DocumentLoader* loader, NavigationPolicy policy)
{
    if (!m_webFrame->client())
        return NavigationPolicyIgnore;
    WebDataSourceImpl* ds = WebDataSourceImpl::fromDocumentLoader(loader);
    WebNavigationPolicy webPolicy = m_webFrame->client()->decidePolicyForNavigation(m_webFrame, ds->extraData(), WrappedResourceRequest(request),
        ds->navigationType(), static_cast<WebNavigationPolicy>(policy), ds->isRedirect());
    return static_cast<NavigationPolicy>(webPolicy);
}

void FrameLoaderClientImpl::dispatchWillRequestResource(FetchRequest* request)
{
    if (m_webFrame->client()) {
        WebCachedURLRequest urlRequest(request);
        m_webFrame->client()->willRequestResource(m_webFrame, urlRequest);
    }
}

void FrameLoaderClientImpl::dispatchWillSendSubmitEvent(HTMLFormElement* form)
{
    if (m_webFrame->client())
        m_webFrame->client()->willSendSubmitEvent(m_webFrame, WebFormElement(form));
}

void FrameLoaderClientImpl::dispatchWillSubmitForm(HTMLFormElement* form)
{
    if (m_webFrame->client())
        m_webFrame->client()->willSubmitForm(m_webFrame, WebFormElement(form));
}

void FrameLoaderClientImpl::didStartLoading(LoadStartType loadStartType)
{
    if (m_webFrame->client())
        m_webFrame->client()->didStartLoading(loadStartType == NavigationToDifferentDocument);
}

void FrameLoaderClientImpl::progressEstimateChanged(double progressEstimate)
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeLoadProgress(progressEstimate);
}

void FrameLoaderClientImpl::didStopLoading()
{
    if (m_webFrame->client())
        m_webFrame->client()->didStopLoading();
}

void FrameLoaderClientImpl::loadURLExternally(const ResourceRequest& request, NavigationPolicy policy, const String& suggestedName)
{
    if (m_webFrame->client()) {
        ASSERT(m_webFrame->frame()->document());
        DocumentFullscreen::webkitCancelFullScreen(*m_webFrame->frame()->document());
        WrappedResourceRequest webreq(request);
        m_webFrame->client()->loadURLExternally(
            m_webFrame, webreq, static_cast<WebNavigationPolicy>(policy), suggestedName);
    }
}

bool FrameLoaderClientImpl::navigateBackForward(int offset) const
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (!webview->client())
        return false;

    ASSERT(offset);
    offset = std::min(offset, webview->client()->historyForwardListCount());
    offset = std::max(offset, -webview->client()->historyBackListCount());
    if (!offset)
        return false;
    webview->client()->navigateBackForwardSoon(offset);
    return true;
}

void FrameLoaderClientImpl::didAccessInitialDocument()
{
    if (m_webFrame->client())
        m_webFrame->client()->didAccessInitialDocument(m_webFrame);
}

void FrameLoaderClientImpl::didDisplayInsecureContent()
{
    if (m_webFrame->client())
        m_webFrame->client()->didDisplayInsecureContent(m_webFrame);
}

void FrameLoaderClientImpl::didRunInsecureContent(SecurityOrigin* origin, const KURL& insecureURL)
{
    if (m_webFrame->client())
        m_webFrame->client()->didRunInsecureContent(m_webFrame, WebSecurityOrigin(origin), insecureURL);
}

void FrameLoaderClientImpl::didDetectXSS(const KURL& insecureURL, bool didBlockEntirePage)
{
    if (m_webFrame->client())
        m_webFrame->client()->didDetectXSS(m_webFrame, insecureURL, didBlockEntirePage);
}

void FrameLoaderClientImpl::didDispatchPingLoader(const KURL& url)
{
    if (m_webFrame->client())
        m_webFrame->client()->didDispatchPingLoader(m_webFrame, url);
}

void FrameLoaderClientImpl::selectorMatchChanged(const Vector<String>& addedSelectors, const Vector<String>& removedSelectors)
{
    if (WebFrameClient* client = m_webFrame->client())
        client->didMatchCSS(m_webFrame, WebVector<WebString>(addedSelectors), WebVector<WebString>(removedSelectors));
}

PassRefPtr<DocumentLoader> FrameLoaderClientImpl::createDocumentLoader(LocalFrame* frame, const ResourceRequest& request, const SubstituteData& data)
{
    RefPtr<WebDataSourceImpl> ds = WebDataSourceImpl::create(frame, request, data);
    if (m_webFrame->client())
        m_webFrame->client()->didCreateDataSource(m_webFrame, ds.get());
    return ds.release();
}

String FrameLoaderClientImpl::userAgent(const KURL& url)
{
    WebString override = m_webFrame->client()->userAgentOverride(m_webFrame, WebURL(url));
    if (!override.isEmpty())
        return override;

    return blink::Platform::current()->userAgent();
}

String FrameLoaderClientImpl::doNotTrackValue()
{
    WebString doNotTrack = m_webFrame->client()->doNotTrackValue(m_webFrame);
    if (!doNotTrack.isEmpty())
        return doNotTrack;
    return String();
}

// Called when the FrameLoader goes into a state in which a new page load
// will occur.
void FrameLoaderClientImpl::transitionToCommittedForNewPage()
{
    m_webFrame->createFrameView();
}

PassRefPtr<LocalFrame> FrameLoaderClientImpl::createFrame(
    const KURL& url,
    const AtomicString& name,
    const Referrer& referrer,
    HTMLFrameOwnerElement* ownerElement)
{
    FrameLoadRequest frameRequest(m_webFrame->frame()->document(),
        ResourceRequest(url, referrer), name);
    return m_webFrame->createChildFrame(frameRequest, ownerElement);
}

PassRefPtr<Widget> FrameLoaderClientImpl::createPlugin(
    HTMLPlugInElement* element,
    const KURL& url,
    const Vector<String>& paramNames,
    const Vector<String>& paramValues,
    const String& mimeType,
    bool loadManually,
    DetachedPluginPolicy policy)
{
    if (!m_webFrame->client())
        return nullptr;

    WebPluginParams params;
    params.url = url;
    params.mimeType = mimeType;
    params.attributeNames = paramNames;
    params.attributeValues = paramValues;
    params.loadManually = loadManually;

    WebPlugin* webPlugin = m_webFrame->client()->createPlugin(m_webFrame, params);
    if (!webPlugin)
        return nullptr;

    // The container takes ownership of the WebPlugin.
    RefPtr<WebPluginContainerImpl> container =
        WebPluginContainerImpl::create(element, webPlugin);

    if (!webPlugin->initialize(container.get()))
        return nullptr;

    if (policy != AllowDetachedPlugin && !element->renderer())
        return nullptr;

    return container;
}

PassRefPtr<Widget> FrameLoaderClientImpl::createJavaAppletWidget(
    HTMLAppletElement* element,
    const KURL& /* baseURL */,
    const Vector<String>& paramNames,
    const Vector<String>& paramValues)
{
    return createPlugin(element, KURL(), paramNames, paramValues,
        "application/x-java-applet", false, FailOnDetachedPlugin);
}

ObjectContentType FrameLoaderClientImpl::objectContentType(
    const KURL& url,
    const String& explicitMimeType,
    bool shouldPreferPlugInsForImages)
{
    // This code is based on Apple's implementation from
    // WebCoreSupport/WebFrameBridge.mm.

    String mimeType = explicitMimeType;
    if (mimeType.isEmpty()) {
        // Try to guess the MIME type based off the extension.
        String filename = url.lastPathComponent();
        int extensionPos = filename.reverseFind('.');
        if (extensionPos >= 0) {
            String extension = filename.substring(extensionPos + 1);
            mimeType = MIMETypeRegistry::getMIMETypeForExtension(extension);
            if (mimeType.isEmpty()) {
                // If there's no mimetype registered for the extension, check to see
                // if a plugin can handle the extension.
                mimeType = getPluginMimeTypeFromExtension(extension);
            }
        }

        if (mimeType.isEmpty())
            return ObjectContentFrame;
    }

    // If Chrome is started with the --disable-plugins switch, pluginData is 0.
    PluginData* pluginData = m_webFrame->frame()->page()->pluginData();
    bool plugInSupportsMIMEType = pluginData && pluginData->supportsMimeType(mimeType);

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return shouldPreferPlugInsForImages && plugInSupportsMIMEType ? ObjectContentNetscapePlugin : ObjectContentImage;

    if (plugInSupportsMIMEType)
        return ObjectContentNetscapePlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentFrame;

    return ObjectContentNone;
}

PassOwnPtr<WebPluginLoadObserver> FrameLoaderClientImpl::pluginLoadObserver(DocumentLoader* loader)
{
    return WebDataSourceImpl::fromDocumentLoader(loader)->releasePluginLoadObserver();
}

WebCookieJar* FrameLoaderClientImpl::cookieJar() const
{
    if (!m_webFrame->client())
        return 0;
    return m_webFrame->client()->cookieJar(m_webFrame);
}

bool FrameLoaderClientImpl::willCheckAndDispatchMessageEvent(
    SecurityOrigin* target, MessageEvent* event) const
{
    if (!m_webFrame->client())
        return false;

    WebFrame* source = 0;
    if (event && event->source() && event->source()->toDOMWindow() && event->source()->toDOMWindow()->document())
        source = WebFrameImpl::fromFrame(event->source()->toDOMWindow()->document()->frame());
    return m_webFrame->client()->willCheckAndDispatchMessageEvent(
        source, m_webFrame, WebSecurityOrigin(target), WebDOMMessageEvent(event));
}

void FrameLoaderClientImpl::didChangeName(const String& name)
{
    if (!m_webFrame->client())
        return;
    m_webFrame->client()->didChangeName(m_webFrame, name);
}

void FrameLoaderClientImpl::dispatchWillOpenSocketStream(SocketStreamHandle* handle)
{
    m_webFrame->client()->willOpenSocketStream(SocketStreamHandleInternal::toWebSocketStreamHandle(handle));
}

void FrameLoaderClientImpl::dispatchWillStartUsingPeerConnectionHandler(blink::WebRTCPeerConnectionHandler* handler)
{
    m_webFrame->client()->willStartUsingPeerConnectionHandler(webFrame(), handler);
}

void FrameLoaderClientImpl::didRequestAutocomplete(HTMLFormElement* form)
{
    if (m_webFrame->viewImpl() && m_webFrame->viewImpl()->autofillClient())
        m_webFrame->viewImpl()->autofillClient()->didRequestAutocomplete(m_webFrame, WebFormElement(form));
}

bool FrameLoaderClientImpl::allowWebGL(bool enabledPerSettings)
{
    if (m_webFrame->client())
        return m_webFrame->client()->allowWebGL(m_webFrame, enabledPerSettings);

    return enabledPerSettings;
}

void FrameLoaderClientImpl::didLoseWebGLContext(int arbRobustnessContextLostReason)
{
    if (m_webFrame->client())
        m_webFrame->client()->didLoseWebGLContext(m_webFrame, arbRobustnessContextLostReason);
}

void FrameLoaderClientImpl::dispatchWillInsertBody()
{
    if (m_webFrame->client())
        m_webFrame->client()->willInsertBody(m_webFrame);

    if (m_webFrame->viewImpl())
        m_webFrame->viewImpl()->willInsertBody(m_webFrame);
}

PassOwnPtr<WebServiceWorkerProvider> FrameLoaderClientImpl::createServiceWorkerProvider()
{
    if (!m_webFrame->client())
        return nullptr;
    return adoptPtr(m_webFrame->client()->createServiceWorkerProvider(m_webFrame));
}

SharedWorkerRepositoryClient* FrameLoaderClientImpl::sharedWorkerRepositoryClient()
{
    return m_webFrame->sharedWorkerRepositoryClient();
}

PassOwnPtr<WebApplicationCacheHost> FrameLoaderClientImpl::createApplicationCacheHost(WebApplicationCacheHostClient* client)
{
    if (!m_webFrame->client())
        return nullptr;
    return adoptPtr(m_webFrame->client()->createApplicationCacheHost(m_webFrame, client));
}

void FrameLoaderClientImpl::didStopAllLoaders()
{
    if (m_webFrame->client())
        m_webFrame->client()->didAbortLoading(m_webFrame);
}

} // namespace blink
