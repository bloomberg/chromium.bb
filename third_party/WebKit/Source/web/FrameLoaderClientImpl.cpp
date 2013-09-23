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
#include "core/events/MessageEvent.h"
#include "core/events/MouseEvent.h"
#include "core/dom/TouchController.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/WheelController.h"
#include "core/history/HistoryItem.h"
#include "core/html/HTMLAppletElement.h"
#include "core/html/HTMLFormElement.h" // needed by core/loader/FormState.h
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/WindowFeatures.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/chromium/support/WrappedResourceRequest.h"
#include "core/platform/chromium/support/WrappedResourceResponse.h"
#include "core/platform/mediastream/RTCPeerConnectionHandler.h"
#include "core/platform/network/HTTPParsers.h"
#include "core/platform/network/SocketStreamHandleInternal.h"
#include "core/plugins/PluginData.h"
#include "core/rendering/HitTestResult.h"
#include "modules/device_orientation/DeviceMotionController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMimeRegistry.h"
#include "public/platform/WebSocketStreamHandle.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

using namespace WebCore;

namespace WebKit {

FrameLoaderClientImpl::FrameLoaderClientImpl(WebFrameImpl* frame)
    : m_webFrame(frame)
{
}

FrameLoaderClientImpl::~FrameLoaderClientImpl()
{
}

void FrameLoaderClientImpl::frameLoaderDestroyed()
{
    // When the WebFrame was created, it had an extra reference given to it on
    // behalf of the Frame.  Since the WebFrame owns us, this extra ref also
    // serves to keep us alive until the FrameLoader is done with us.  The
    // FrameLoader calls this method when it's going away.  Therefore, we balance
    // out that extra reference, which may cause 'this' to be deleted.
    ASSERT(!m_webFrame->frame());
    m_webFrame->deref();
}

void FrameLoaderClientImpl::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*)
{
    if (m_webFrame->client()) {
        m_webFrame->client()->didClearWindowObject(m_webFrame);
        Document* document = m_webFrame->frame()->document();
        if (document) {
            TouchController::from(document);
            WheelController::from(document);
            if (RuntimeEnabledFeatures::deviceMotionEnabled())
                DeviceMotionController::from(document);
        }
    }
}

void FrameLoaderClientImpl::documentElementAvailable()
{
    if (m_webFrame->client())
        m_webFrame->client()->didCreateDocumentElement(m_webFrame);
}

void FrameLoaderClientImpl::didExhaustMemoryAvailableForScript()
{
    if (m_webFrame->client())
        m_webFrame->client()->didExhaustMemoryAvailableForScript(m_webFrame);
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
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowScriptExtension(m_webFrame, extensionName, extensionGroup, worldId);

    return true;
}

void FrameLoaderClientImpl::didChangeScrollOffset()
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeScrollOffset(m_webFrame);
}

bool FrameLoaderClientImpl::allowScript(bool enabledPerSettings)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowScript(m_webFrame, enabledPerSettings);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowScriptFromSource(bool enabledPerSettings, const KURL& scriptURL)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowScriptFromSource(m_webFrame, enabledPerSettings, scriptURL);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowPlugins(bool enabledPerSettings)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowPlugins(m_webFrame, enabledPerSettings);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowImage(bool enabledPerSettings, const KURL& imageURL)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowImage(m_webFrame, enabledPerSettings, imageURL);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowDisplayingInsecureContent(bool enabledPerSettings, SecurityOrigin* context, const KURL& url)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowDisplayingInsecureContent(m_webFrame, enabledPerSettings, WebSecurityOrigin(context), WebURL(url));

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowRunningInsecureContent(bool enabledPerSettings, SecurityOrigin* context, const KURL& url)
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        return webview->permissionClient()->allowRunningInsecureContent(m_webFrame, enabledPerSettings, WebSecurityOrigin(context), WebURL(url));

    return enabledPerSettings;
}

void FrameLoaderClientImpl::didNotAllowScript()
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        webview->permissionClient()->didNotAllowScript(m_webFrame);
}

void FrameLoaderClientImpl::didNotAllowPlugins()
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->permissionClient())
        webview->permissionClient()->didNotAllowPlugins(m_webFrame);

}

bool FrameLoaderClientImpl::hasWebView() const
{
    return m_webFrame->viewImpl();
}

bool FrameLoaderClientImpl::hasFrameView() const
{
    // The Mac port has this notion of a WebFrameView, which seems to be
    // some wrapper around an NSView.  Since our equivalent is HWND, I guess
    // we have a "frameview" whenever we have the toplevel HWND.
    return m_webFrame->viewImpl();
}

void FrameLoaderClientImpl::detachedFromParent()
{
    // Close down the proxy.  The purpose of this change is to make the
    // call to ScriptController::clearWindowShell a no-op when called from
    // Frame::pageDestroyed.  Without this change, this call to clearWindowShell
    // will cause a crash.  If you remove/modify this, just ensure that you can
    // go to a page and then navigate to a new page without getting any asserts
    // or crashes.
    m_webFrame->frame()->script()->clearForClose();

    // Alert the client that the frame is being detached. This is the last
    // chance we have to communicate with the client.
    if (m_webFrame->client())
        m_webFrame->client()->frameDetached(m_webFrame);

    // Stop communicating with the WebFrameClient at this point since we are no
    // longer associated with the Page.
    m_webFrame->setClient(0);
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
void FrameLoaderClientImpl::dispatchDidChangeResourcePriority(unsigned long identifier,
                                                              ResourceLoadPriority priority)
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeResourcePriority(m_webFrame, identifier, static_cast<WebKit::WebURLRequest::Priority>(priority));
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
}

void FrameLoaderClientImpl::dispatchDidNavigateWithinPage()
{
    bool isNewNavigation;
    m_webFrame->viewImpl()->didCommitLoad(&isNewNavigation, true);
    if (m_webFrame->client())
        m_webFrame->client()->didNavigateWithinPage(m_webFrame, isNewNavigation);
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

void FrameLoaderClientImpl::dispatchDidCommitLoad()
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    bool isNewNavigation;
    webview->didCommitLoad(&isNewNavigation, false);

    if (m_webFrame->client())
        m_webFrame->client()->didCommitProvisionalLoad(m_webFrame, isNewNavigation);
}

void FrameLoaderClientImpl::dispatchDidFailProvisionalLoad(
    const ResourceError& error)
{
    OwnPtr<WebPluginLoadObserver> observer = pluginLoadObserver();
    m_webFrame->didFail(error, true);
    if (observer)
        observer->didFailLoading(error);
}

void FrameLoaderClientImpl::dispatchDidFailLoad(const ResourceError& error)
{
    OwnPtr<WebPluginLoadObserver> observer = pluginLoadObserver();
    m_webFrame->didFail(error, false);
    if (observer)
        observer->didFailLoading(error);

    // Don't clear the redirect chain, this will happen in the middle of client
    // redirects, and we need the context. The chain will be cleared when the
    // provisional load succeeds or fails, not the "real" one.
}

void FrameLoaderClientImpl::dispatchDidFinishLoad()
{
    OwnPtr<WebPluginLoadObserver> observer = pluginLoadObserver();

    if (m_webFrame->client())
        m_webFrame->client()->didFinishLoad(m_webFrame);

    if (observer)
        observer->didFinishLoading();

    // Don't clear the redirect chain, this will happen in the middle of client
    // redirects, and we need the context. The chain will be cleared when the
    // provisional load succeeds or fails, not the "real" one.
}

void FrameLoaderClientImpl::dispatchDidLayout(LayoutMilestones milestones)
{
    if (!m_webFrame->client())
        return;

    if (milestones & DidFirstLayout)
        m_webFrame->client()->didFirstLayout(m_webFrame);
    if (milestones & DidFirstVisuallyNonEmptyLayout)
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

void FrameLoaderClientImpl::dispatchWillSendSubmitEvent(PassRefPtr<FormState> prpFormState)
{
    if (m_webFrame->client())
        m_webFrame->client()->willSendSubmitEvent(m_webFrame, WebFormElement(prpFormState->form()));
}

void FrameLoaderClientImpl::dispatchWillSubmitForm(PassRefPtr<FormState> formState)
{
    if (m_webFrame->client())
        m_webFrame->client()->willSubmitForm(m_webFrame, WebFormElement(formState->form()));
}

void FrameLoaderClientImpl::postProgressStartedNotification()
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->client())
        webview->client()->didStartLoading();
}

void FrameLoaderClientImpl::postProgressEstimateChangedNotification()
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->client()) {
        webview->client()->didChangeLoadProgress(
            m_webFrame, m_webFrame->frame()->page()->progress().estimatedProgress());
    }
}

void FrameLoaderClientImpl::postProgressFinishedNotification()
{
    // FIXME: why might the webview be null?  http://b/1234461
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview && webview->client())
        webview->client()->didStopLoading();
}

void FrameLoaderClientImpl::loadURLExternally(const ResourceRequest& request, NavigationPolicy policy, const String& suggestedName)
{
    if (m_webFrame->client()) {
        WrappedResourceRequest webreq(request);
        m_webFrame->client()->loadURLExternally(
            m_webFrame, webreq, static_cast<WebNavigationPolicy>(policy), suggestedName);
    }
}

void FrameLoaderClientImpl::navigateBackForward(int offset) const
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (webview->client())
        webview->client()->navigateBackForwardSoon(offset);
}

void FrameLoaderClientImpl::didAccessInitialDocument()
{
    if (m_webFrame->client())
        m_webFrame->client()->didAccessInitialDocument(m_webFrame);
}

void FrameLoaderClientImpl::didDisownOpener()
{
    if (m_webFrame->client())
        m_webFrame->client()->didDisownOpener(m_webFrame);
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

PassRefPtr<DocumentLoader> FrameLoaderClientImpl::createDocumentLoader(
    const ResourceRequest& request,
    const SubstituteData& data)
{
    RefPtr<WebDataSourceImpl> ds = WebDataSourceImpl::create(request, data);
    if (m_webFrame->client())
        m_webFrame->client()->didCreateDataSource(m_webFrame, ds.get());
    return ds.release();
}

String FrameLoaderClientImpl::userAgent(const KURL& url)
{
    WebString override = m_webFrame->client()->userAgentOverride(m_webFrame, WebURL(url));
    if (!override.isEmpty())
        return override;

    return WebKit::Platform::current()->userAgent(url);
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

PassRefPtr<Frame> FrameLoaderClientImpl::createFrame(
    const KURL& url,
    const String& name,
    const String& referrer,
    HTMLFrameOwnerElement* ownerElement)
{
    FrameLoadRequest frameRequest(m_webFrame->frame()->document()->securityOrigin(),
        ResourceRequest(url, referrer), name);
    return m_webFrame->createChildFrame(frameRequest, ownerElement);
}

PassRefPtr<Widget> FrameLoaderClientImpl::createPlugin(
    const IntSize& size, // FIXME: how do we use this?
    HTMLPlugInElement* element,
    const KURL& url,
    const Vector<String>& paramNames,
    const Vector<String>& paramValues,
    const String& mimeType,
    bool loadManually)
{
    if (!m_webFrame->client())
        return 0;

    WebPluginParams params;
    params.url = url;
    params.mimeType = mimeType;
    params.attributeNames = paramNames;
    params.attributeValues = paramValues;
    params.loadManually = loadManually;

    WebPlugin* webPlugin = m_webFrame->client()->createPlugin(m_webFrame, params);
    if (!webPlugin)
        return 0;

    // The container takes ownership of the WebPlugin.
    RefPtr<WebPluginContainerImpl> container =
        WebPluginContainerImpl::create(element, webPlugin);

    if (!webPlugin->initialize(container.get()))
        return 0;

    // The element might have been removed during plugin initialization!
    if (!element->renderer())
        return 0;

    return container;
}

PassRefPtr<Widget> FrameLoaderClientImpl::createJavaAppletWidget(
    const IntSize& size,
    HTMLAppletElement* element,
    const KURL& /* baseURL */,
    const Vector<String>& paramNames,
    const Vector<String>& paramValues)
{
    return createPlugin(size, element, KURL(), paramNames, paramValues,
        "application/x-java-applet", false);
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

PassOwnPtr<WebPluginLoadObserver> FrameLoaderClientImpl::pluginLoadObserver()
{
    WebDataSourceImpl* ds = WebDataSourceImpl::fromDocumentLoader(
        m_webFrame->frame()->loader()->activeDocumentLoader());
    if (!ds) {
        // We can arrive here if a popstate event handler detaches this frame.
        // FIXME: Remove this code once http://webkit.org/b/36202 is fixed.
        ASSERT(!m_webFrame->frame()->page());
        return nullptr;
    }
    return ds->releasePluginLoadObserver();
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

void FrameLoaderClientImpl::dispatchWillStartUsingPeerConnectionHandler(RTCPeerConnectionHandler* handler)
{
    m_webFrame->client()->willStartUsingPeerConnectionHandler(webFrame(), RTCPeerConnectionHandler::toWebRTCPeerConnectionHandler(handler));
}

void FrameLoaderClientImpl::didRequestAutocomplete(PassRefPtr<FormState> formState)
{
    if (m_webFrame->viewImpl() && m_webFrame->viewImpl()->autofillClient())
        m_webFrame->viewImpl()->autofillClient()->didRequestAutocomplete(m_webFrame, WebFormElement(formState->form()));
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
}

WebServiceWorkerRegistry* FrameLoaderClientImpl::serviceWorkerRegistry()
{
    if (!m_webFrame->client())
        return 0;
    return m_webFrame->client()->serviceWorkerRegistry(m_webFrame);
}

void FrameLoaderClientImpl::didStopAllLoaders()
{
    if (m_webFrame->client())
        m_webFrame->client()->didAbortLoading(m_webFrame);
}

} // namespace WebKit
