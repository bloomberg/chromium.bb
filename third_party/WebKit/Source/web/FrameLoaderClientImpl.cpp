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

#include "web/FrameLoaderClientImpl.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/events/MessageEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/UIEventWithKeyState.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestResult.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"
#include "modules/device_light/DeviceLightController.h"
#include "modules/device_orientation/DeviceMotionController.h"
#include "modules/device_orientation/DeviceOrientationAbsoluteController.h"
#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"
#include "modules/gamepad/NavigatorGamepad.h"
#include "modules/mediasession/HTMLMediaElementMediaSession.h"
#include "modules/mediasession/MediaSession.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/storage/DOMWindowStorageController.h"
#include "modules/vr/NavigatorVRDevice.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/fonts/GlyphPageTreeNode.h"
#include "platform/network/HTTPParsers.h"
#include "platform/plugins/PluginData.h"
#include "public/platform/Platform.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/WebMimeRegistry.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/mediasession/WebMediaSession.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebContentSettingsClient.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebNode.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPluginParams.h"
#include "public/web/WebViewClient.h"
#include "web/DevToolsEmulator.h"
#include "web/SharedWorkerRepositoryClientImpl.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebDevToolsFrontendImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

namespace {

// Convenience helper for frame tree helpers in FrameClient to reduce the amount
// of null-checking boilerplate code. Since the frame tree is maintained in the
// web/ layer, the frame tree helpers often have to deal with null WebFrames:
// for example, a frame with no parent will return null for WebFrame::parent().
// TODO(dcheng): Remove duplication between FrameLoaderClientImpl and
// RemoteFrameClientImpl somehow...
Frame* toCoreFrame(WebFrame* frame)
{
    return frame ? frame->toImplBase()->frame() : nullptr;
}

} // namespace

FrameLoaderClientImpl::FrameLoaderClientImpl(WebLocalFrameImpl* frame)
    : m_webFrame(frame)
{
}

PassOwnPtrWillBeRawPtr<FrameLoaderClientImpl> FrameLoaderClientImpl::create(WebLocalFrameImpl* frame)
{
    return adoptPtrWillBeNoop(new FrameLoaderClientImpl(frame));
}

FrameLoaderClientImpl::~FrameLoaderClientImpl()
{
}

DEFINE_TRACE(FrameLoaderClientImpl)
{
    visitor->trace(m_webFrame);
    FrameLoaderClient::trace(visitor);
}

void FrameLoaderClientImpl::didCreateNewDocument()
{
    if (m_webFrame->client())
        m_webFrame->client()->didCreateNewDocument(m_webFrame);
}

void FrameLoaderClientImpl::dispatchDidClearWindowObjectInMainWorld()
{
    if (m_webFrame->client()) {
        m_webFrame->client()->didClearWindowObject(m_webFrame);
        Document* document = m_webFrame->frame()->document();
        if (document) {
            DeviceMotionController::from(*document);
            DeviceOrientationController::from(*document);
            if (RuntimeEnabledFeatures::deviceOrientationAbsoluteEnabled())
                DeviceOrientationAbsoluteController::from(*document);
            if (RuntimeEnabledFeatures::deviceLightEnabled())
                DeviceLightController::from(*document);
            NavigatorGamepad::from(*document);
            NavigatorServiceWorker::from(*document);
            DOMWindowStorageController::from(*document);
            if (RuntimeEnabledFeatures::webVREnabled())
                NavigatorVRDevice::from(*document);
        }
    }
    // FIXME: when extensions go out of process, this whole concept stops working.
    WebDevToolsFrontendImpl* devToolsFrontend = m_webFrame->top()->isWebLocalFrame() ? toWebLocalFrameImpl(m_webFrame->top())->devToolsFrontend() : nullptr;
    if (devToolsFrontend)
        devToolsFrontend->didClearWindowObject(m_webFrame);
}

void FrameLoaderClientImpl::documentElementAvailable()
{
    if (m_webFrame->client())
        m_webFrame->client()->didCreateDocumentElement(m_webFrame);

    if (m_webFrame->parent())
        return;

    if (m_webFrame->viewImpl())
        m_webFrame->viewImpl()->mainFrameDocumentElementAvailable();
}

void FrameLoaderClientImpl::didCreateScriptContext(v8::Local<v8::Context> context, int extensionGroup, int worldId)
{
    if (m_webFrame->client())
        m_webFrame->client()->didCreateScriptContext(m_webFrame, context, extensionGroup, worldId);
}

void FrameLoaderClientImpl::willReleaseScriptContext(v8::Local<v8::Context> context, int worldId)
{
    if (m_webFrame->client())
        m_webFrame->client()->willReleaseScriptContext(m_webFrame, context, worldId);
}

bool FrameLoaderClientImpl::allowScriptExtension(const String& extensionName,
                                                 int extensionGroup,
                                                 int worldId)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowScriptExtension(extensionName, extensionGroup, worldId);

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

// TODO(dglazkov): Can this be plumbing be streamlined out?
void FrameLoaderClientImpl::didRemoveAllPendingStylesheet()
{
    if (m_webFrame->parent())
        return;

    if (WebViewImpl* webview = m_webFrame->viewImpl())
        webview->didRemoveAllPendingStylesheetsInMainFrameDocument();
}

bool FrameLoaderClientImpl::allowScript(bool enabledPerSettings)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowScript(enabledPerSettings);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowScriptFromSource(bool enabledPerSettings, const KURL& scriptURL)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowScriptFromSource(enabledPerSettings, scriptURL);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowPlugins(bool enabledPerSettings)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowPlugins(enabledPerSettings);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowImage(bool enabledPerSettings, const KURL& imageURL)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowImage(enabledPerSettings, imageURL);

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowMedia(const KURL& mediaURL)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowMedia(mediaURL);

    return true;
}

bool FrameLoaderClientImpl::allowDisplayingInsecureContent(bool enabledPerSettings, const KURL& url)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowDisplayingInsecureContent(enabledPerSettings, WebURL(url));

    return enabledPerSettings;
}

bool FrameLoaderClientImpl::allowRunningInsecureContent(bool enabledPerSettings, SecurityOrigin* context, const KURL& url)
{
    if (m_webFrame->contentSettingsClient())
        return m_webFrame->contentSettingsClient()->allowRunningInsecureContent(enabledPerSettings, WebSecurityOrigin(context), WebURL(url));

    return enabledPerSettings;
}

void FrameLoaderClientImpl::didNotAllowScript()
{
    if (m_webFrame->contentSettingsClient())
        m_webFrame->contentSettingsClient()->didNotAllowScript();
}

void FrameLoaderClientImpl::didNotAllowPlugins()
{
    if (m_webFrame->contentSettingsClient())
        m_webFrame->contentSettingsClient()->didNotAllowPlugins();

}

void FrameLoaderClientImpl::didUseKeygen()
{
    if (m_webFrame->contentSettingsClient())
        m_webFrame->contentSettingsClient()->didUseKeygen();
}

bool FrameLoaderClientImpl::hasWebView() const
{
    return m_webFrame->viewImpl();
}

bool FrameLoaderClientImpl::inShadowTree() const
{
    return m_webFrame->inShadowTree();
}

Frame* FrameLoaderClientImpl::opener() const
{
    return toCoreFrame(m_webFrame->opener());
}

void FrameLoaderClientImpl::setOpener(Frame* opener)
{
    WebFrame* openerFrame = WebFrame::fromFrame(opener);
    if (m_webFrame->client() && m_webFrame->opener() != openerFrame)
        m_webFrame->client()->didChangeOpener(openerFrame);
    m_webFrame->setOpener(openerFrame);
}

Frame* FrameLoaderClientImpl::parent() const
{
    return toCoreFrame(m_webFrame->parent());
}

Frame* FrameLoaderClientImpl::top() const
{
    return toCoreFrame(m_webFrame->top());
}

Frame* FrameLoaderClientImpl::previousSibling() const
{
    return toCoreFrame(m_webFrame->previousSibling());
}

Frame* FrameLoaderClientImpl::nextSibling() const
{
    return toCoreFrame(m_webFrame->nextSibling());
}

Frame* FrameLoaderClientImpl::firstChild() const
{
    return toCoreFrame(m_webFrame->firstChild());
}

Frame* FrameLoaderClientImpl::lastChild() const
{
    return toCoreFrame(m_webFrame->lastChild());
}

void FrameLoaderClientImpl::willBeDetached()
{
    m_webFrame->willBeDetached();
}

void FrameLoaderClientImpl::detached(FrameDetachType type)
{
    // Alert the client that the frame is being detached. This is the last
    // chance we have to communicate with the client.
    RefPtrWillBeRawPtr<WebLocalFrameImpl> protector(m_webFrame.get());

    WebFrameClient* client = m_webFrame->client();
    if (!client)
        return;

    m_webFrame->willDetachParent();

    // Signal that no further communication with WebFrameClient should take
    // place at this point since we are no longer associated with the Page.
    m_webFrame->setClient(0);

    client->frameDetached(m_webFrame, static_cast<WebFrameClient::DetachType>(type));
    // Clear our reference to LocalFrame at the very end, in case the client
    // refers to it.
    m_webFrame->setCoreFrame(nullptr);
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
        m_webFrame->client()->didChangeResourcePriority(m_webFrame, identifier, static_cast<WebURLRequest::Priority>(priority), intraPriorityValue);
}

// Called when a particular resource load completes
void FrameLoaderClientImpl::dispatchDidFinishLoading(DocumentLoader* loader,
                                                    unsigned long identifier)
{
    if (m_webFrame->client())
        m_webFrame->client()->didFinishResourceLoad(m_webFrame, identifier);
}

void FrameLoaderClientImpl::dispatchDidFinishDocumentLoad(bool documentIsEmpty)
{
    if (!m_webFrame->parent()) {
        if (WebViewImpl* webview = m_webFrame->viewImpl())
            webview->didFinishMainFrameDocumentLoad();
    }

    // TODO(dglazkov): Sadly, workers are WebFrameClients, and they can totally
    // destroy themselves when didFinishDocumentLoad is invoked, and in turn destroy
    // the fake WebLocalFrame that they create, which means that you should not
    // put any code touching `this` after the two lines below.
    if (m_webFrame->client())
        m_webFrame->client()->didFinishDocumentLoad(m_webFrame, documentIsEmpty);
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

void FrameLoaderClientImpl::dispatchDidNavigateWithinPage(HistoryItem* item, HistoryCommitType commitType)
{
    bool shouldCreateHistoryEntry = commitType == StandardCommit;
    // TODO(dglazkov): Does this need to be called for subframes?
    m_webFrame->viewImpl()->didCommitLoad(shouldCreateHistoryEntry, true);
    if (m_webFrame->client())
        m_webFrame->client()->didNavigateWithinPage(m_webFrame, WebHistoryItem(item), static_cast<WebHistoryCommitType>(commitType));
}

void FrameLoaderClientImpl::dispatchWillClose()
{
    if (m_webFrame->client())
        m_webFrame->client()->willClose(m_webFrame);
}

void FrameLoaderClientImpl::dispatchDidStartProvisionalLoad(double triggeringEventTime)
{
    if (m_webFrame->client())
        m_webFrame->client()->didStartProvisionalLoad(m_webFrame, triggeringEventTime);
}

void FrameLoaderClientImpl::dispatchDidReceiveTitle(const String& title)
{
    if (m_webFrame->client())
        m_webFrame->client()->didReceiveTitle(m_webFrame, title, WebTextDirectionLeftToRight);
}

void FrameLoaderClientImpl::dispatchDidChangeIcons(IconType type)
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeIcon(m_webFrame, static_cast<WebIconURL::Type>(type));
}

void FrameLoaderClientImpl::dispatchDidCommitLoad(HistoryItem* item, HistoryCommitType commitType)
{
    if (!m_webFrame->parent()) {
        m_webFrame->viewImpl()->didCommitLoad(commitType == StandardCommit, false);
    }

    // Save some histogram data so we can compute the average memory used per
    // page load of the glyphs.
    // TODO(esprehn): Is this ancient uma actually useful?
    Platform::current()->histogramCustomCounts("Memory.GlyphPagesPerLoad", GlyphPageTreeNode::treeGlyphPageCount(), 1, 10000, 50);

    if (m_webFrame->client())
        m_webFrame->client()->didCommitProvisionalLoad(m_webFrame, WebHistoryItem(item), static_cast<WebHistoryCommitType>(commitType));
    WebDevToolsAgentImpl* devToolsAgent = WebLocalFrameImpl::fromFrame(m_webFrame->frame()->localFrameRoot())->devToolsAgentImpl();
    if (devToolsAgent)
        devToolsAgent->didCommitLoadForLocalFrame(m_webFrame->frame());
}

void FrameLoaderClientImpl::dispatchDidFailProvisionalLoad(
    const ResourceError& error, HistoryCommitType commitType)
{
    m_webFrame->didFail(error, true, commitType);
}

void FrameLoaderClientImpl::dispatchDidFailLoad(const ResourceError& error, HistoryCommitType commitType)
{
    m_webFrame->didFail(error, false, commitType);
}

void FrameLoaderClientImpl::dispatchDidFinishLoad()
{
    m_webFrame->didFinish();
}

void FrameLoaderClientImpl::dispatchDidChangeThemeColor()
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeThemeColor();
}

static bool allowCreatingBackgroundTabs()
{

    const WebInputEvent* inputEvent = WebViewImpl::currentInputEvent();
    if (!inputEvent || (inputEvent->type != WebInputEvent::MouseUp && (inputEvent->type != WebInputEvent::RawKeyDown && inputEvent->type != WebInputEvent::KeyDown)
        && inputEvent->type != WebInputEvent::GestureTap))
        return false;

    unsigned short buttonNumber;
    if (WebInputEvent::isMouseEventType(inputEvent->type)) {
        const WebMouseEvent* mouseEvent = static_cast<const WebMouseEvent*>(inputEvent);

        switch (mouseEvent->button) {
        case WebMouseEvent::ButtonLeft:
            buttonNumber = 0;
            break;
        case WebMouseEvent::ButtonMiddle:
            buttonNumber = 1;
            break;
        case WebMouseEvent::ButtonRight:
            buttonNumber = 2;
            break;
        default:
            return false;
        }
    } else {
        // The click is simulated when triggering the keypress event.
        buttonNumber = 0;
    }
    bool ctrl = inputEvent->modifiers & WebMouseEvent::ControlKey;
    bool shift = inputEvent->modifiers & WebMouseEvent::ShiftKey;
    bool alt = inputEvent->modifiers & WebMouseEvent::AltKey;
    bool meta = inputEvent->modifiers & WebMouseEvent::MetaKey;

    NavigationPolicy userPolicy;
    if (!navigationPolicyFromMouseEvent(buttonNumber, ctrl, shift, alt, meta, &userPolicy))
        return false;
    return userPolicy == NavigationPolicyNewBackgroundTab;
}

NavigationPolicy FrameLoaderClientImpl::decidePolicyForNavigation(const ResourceRequest& request, DocumentLoader* loader, NavigationType type, NavigationPolicy policy, bool replacesCurrentHistoryItem)
{
    if (!m_webFrame->client())
        return NavigationPolicyIgnore;

    if (policy == NavigationPolicyNewBackgroundTab && !allowCreatingBackgroundTabs() && !UIEventWithKeyState::newTabModifierSetFromIsolatedWorld())
        policy = NavigationPolicyNewForegroundTab;

    WebDataSourceImpl* ds = WebDataSourceImpl::fromDocumentLoader(loader);

    // Newly created child frames may need to be navigated to a history item
    // during a back/forward navigation. This will only happen when the parent
    // is a LocalFrame doing a back/forward navigation that has not completed.
    // (If the load has completed and the parent later adds a frame with script,
    // we do not want to use a history item for it.)
    bool isHistoryNavigationInNewChildFrame = m_webFrame->parent()
        && m_webFrame->parent()->isWebLocalFrame()
        && isBackForwardLoadType(toWebLocalFrameImpl(m_webFrame->parent())->frame()->loader().loadType())
        && !toWebLocalFrameImpl(m_webFrame->parent())->frame()->document()->loadEventFinished();

    WrappedResourceRequest wrappedResourceRequest(request);
    WebFrameClient::NavigationPolicyInfo navigationInfo(wrappedResourceRequest);
    navigationInfo.navigationType = static_cast<WebNavigationType>(type);
    navigationInfo.defaultPolicy = static_cast<WebNavigationPolicy>(policy);
    navigationInfo.extraData = ds ? ds->extraData() : nullptr;
    navigationInfo.replacesCurrentHistoryItem = replacesCurrentHistoryItem;
    navigationInfo.isHistoryNavigationInNewChildFrame = isHistoryNavigationInNewChildFrame;

    WebNavigationPolicy webPolicy = m_webFrame->client()->decidePolicyForNavigation(navigationInfo);
    return static_cast<NavigationPolicy>(webPolicy);
}

bool FrameLoaderClientImpl::hasPendingNavigation()
{
    if (!m_webFrame->client())
        return false;

    return m_webFrame->client()->hasPendingNavigation(m_webFrame);
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

void FrameLoaderClientImpl::loadURLExternally(const ResourceRequest& request, NavigationPolicy policy, const String& suggestedName, bool shouldReplaceCurrentEntry)
{
    if (!m_webFrame->client())
        return;
    ASSERT(m_webFrame->frame()->document());
    Fullscreen::fullyExitFullscreen(*m_webFrame->frame()->document());
    m_webFrame->client()->loadURLExternally(
        WrappedResourceRequest(request), static_cast<WebNavigationPolicy>(policy), suggestedName, shouldReplaceCurrentEntry);
}

bool FrameLoaderClientImpl::navigateBackForward(int offset) const
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (!webview->client())
        return false;

    ASSERT(offset);
    if (offset > webview->client()->historyForwardListCount())
        return false;
    if (offset < -webview->client()->historyBackListCount())
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
        m_webFrame->client()->didDisplayInsecureContent();
}

void FrameLoaderClientImpl::didRunInsecureContent(SecurityOrigin* origin, const KURL& insecureURL)
{
    if (m_webFrame->client())
        m_webFrame->client()->didRunInsecureContent(WebSecurityOrigin(origin), insecureURL);
}

void FrameLoaderClientImpl::didDetectXSS(const KURL& insecureURL, bool didBlockEntirePage)
{
    if (m_webFrame->client())
        m_webFrame->client()->didDetectXSS(insecureURL, didBlockEntirePage);
}

void FrameLoaderClientImpl::didDispatchPingLoader(const KURL& url)
{
    if (m_webFrame->client())
        m_webFrame->client()->didDispatchPingLoader(m_webFrame, url);
}

void FrameLoaderClientImpl::didDisplayContentWithCertificateErrors(const KURL& url, const CString& securityInfo, const WebURL& mainResourceUrl, const CString& mainResourceSecurityInfo)
{
    if (m_webFrame->client())
        m_webFrame->client()->didDisplayContentWithCertificateErrors(url, securityInfo, mainResourceUrl, mainResourceSecurityInfo);
}

void FrameLoaderClientImpl::didRunContentWithCertificateErrors(const KURL& url, const CString& securityInfo, const WebURL& mainResourceUrl, const CString& mainResourceSecurityInfo)
{
    if (m_webFrame->client())
        m_webFrame->client()->didRunContentWithCertificateErrors(url, securityInfo, mainResourceUrl, mainResourceSecurityInfo);
}

void FrameLoaderClientImpl::didChangePerformanceTiming()
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangePerformanceTiming();
}

void FrameLoaderClientImpl::selectorMatchChanged(const Vector<String>& addedSelectors, const Vector<String>& removedSelectors)
{
    if (WebFrameClient* client = m_webFrame->client())
        client->didMatchCSS(m_webFrame, WebVector<WebString>(addedSelectors), WebVector<WebString>(removedSelectors));
}

PassRefPtrWillBeRawPtr<DocumentLoader> FrameLoaderClientImpl::createDocumentLoader(LocalFrame* frame, const ResourceRequest& request, const SubstituteData& data)
{
    RefPtrWillBeRawPtr<WebDataSourceImpl> ds = WebDataSourceImpl::create(frame, request, data);
    if (m_webFrame->client())
        m_webFrame->client()->didCreateDataSource(m_webFrame, ds.get());
    return ds.release();
}

String FrameLoaderClientImpl::userAgent()
{
    WebString override = m_webFrame->client()->userAgentOverride(m_webFrame);
    if (!override.isEmpty())
        return override;

    return Platform::current()->userAgent();
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

PassRefPtrWillBeRawPtr<LocalFrame> FrameLoaderClientImpl::createFrame(
    const FrameLoadRequest& request,
    const AtomicString& name,
    HTMLFrameOwnerElement* ownerElement)
{
    return m_webFrame->createChildFrame(request, name, ownerElement);
}

bool FrameLoaderClientImpl::canCreatePluginWithoutRenderer(const String& mimeType) const
{
    if (!m_webFrame->client())
        return false;

    return m_webFrame->client()->canCreatePluginWithoutRenderer(mimeType);
}

PassRefPtrWillBeRawPtr<Widget> FrameLoaderClientImpl::createPlugin(
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
    RefPtrWillBeRawPtr<WebPluginContainerImpl> container =
        WebPluginContainerImpl::create(element, webPlugin);

    if (!webPlugin->initialize(container.get()))
        return nullptr;

    if (policy != AllowDetachedPlugin && !element->layoutObject())
        return nullptr;

    return container;
}

PassOwnPtr<WebMediaPlayer> FrameLoaderClientImpl::createWebMediaPlayer(
    HTMLMediaElement& htmlMediaElement,
    WebMediaPlayer::LoadType loadType,
    const WebURL& url,
    WebMediaPlayerClient* client)
{
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(
        htmlMediaElement.document().frame());

    if (!webFrame || !webFrame->client())
        return nullptr;

    WebMediaSession* webMediaSession = nullptr;
    if (MediaSession* mediaSession = HTMLMediaElementMediaSession::session(htmlMediaElement))
        webMediaSession = mediaSession->webMediaSession();

    HTMLMediaElementEncryptedMedia& encryptedMedia = HTMLMediaElementEncryptedMedia::from(htmlMediaElement);
    WebString sinkId(HTMLMediaElementAudioOutputDevice::sinkId(htmlMediaElement));
    return adoptPtr(webFrame->client()->createMediaPlayer(webFrame, loadType, url,
        client, &encryptedMedia,
        encryptedMedia.contentDecryptionModule(), sinkId, webMediaSession));
}

PassOwnPtr<WebMediaSession> FrameLoaderClientImpl::createWebMediaSession()
{
    if (!m_webFrame->client())
        return nullptr;

    return adoptPtr(m_webFrame->client()->createMediaSession());
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

WebCookieJar* FrameLoaderClientImpl::cookieJar() const
{
    if (!m_webFrame->client())
        return 0;
    return m_webFrame->client()->cookieJar(m_webFrame);
}

bool FrameLoaderClientImpl::willCheckAndDispatchMessageEvent(
    SecurityOrigin* target, MessageEvent* event, LocalFrame* sourceFrame) const
{
    if (!m_webFrame->client())
        return false;
    return m_webFrame->client()->willCheckAndDispatchMessageEvent(
        WebLocalFrameImpl::fromFrame(sourceFrame), m_webFrame, WebSecurityOrigin(target), WebDOMMessageEvent(event));
}

void FrameLoaderClientImpl::frameFocused() const
{
    if (m_webFrame->client())
        m_webFrame->client()->frameFocused();
}

void FrameLoaderClientImpl::didChangeName(const String& name)
{
    if (!m_webFrame->client())
        return;
    m_webFrame->client()->didChangeName(m_webFrame, name);
}

void FrameLoaderClientImpl::didEnforceStrictMixedContentChecking()
{
    if (!m_webFrame->client())
        return;
    m_webFrame->client()->didEnforceStrictMixedContentChecking();
}

void FrameLoaderClientImpl::didChangeSandboxFlags(Frame* childFrame, SandboxFlags flags)
{
    if (!m_webFrame->client())
        return;
    m_webFrame->client()->didChangeSandboxFlags(WebFrame::fromFrame(childFrame), static_cast<WebSandboxFlags>(flags));
}

void FrameLoaderClientImpl::didChangeFrameOwnerProperties(HTMLFrameElementBase* frameElement)
{
    if (!m_webFrame->client())
        return;

    m_webFrame->client()->didChangeFrameOwnerProperties(WebFrame::fromFrame(frameElement->contentFrame()), WebFrameOwnerProperties(frameElement->scrollingMode(), frameElement->marginWidth(), frameElement->marginHeight()));
}

void FrameLoaderClientImpl::dispatchWillOpenWebSocket(WebSocketHandle* handle)
{
    m_webFrame->client()->willOpenWebSocket(handle);
}

void FrameLoaderClientImpl::dispatchWillStartUsingPeerConnectionHandler(WebRTCPeerConnectionHandler* handler)
{
    m_webFrame->client()->willStartUsingPeerConnectionHandler(webFrame(), handler);
}

void FrameLoaderClientImpl::didRequestAutocomplete(HTMLFormElement* form)
{
    if (m_webFrame->autofillClient())
        m_webFrame->autofillClient()->didRequestAutocomplete(WebFormElement(form));
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

    if (m_webFrame->parent())
        return;

    if (m_webFrame->viewImpl())
        m_webFrame->viewImpl()->willInsertMainFrameDocumentBody();
}

v8::Local<v8::Value> FrameLoaderClientImpl::createTestInterface(const AtomicString& name)
{
    return m_webFrame->createTestInterface(name);
}

PassOwnPtr<WebServiceWorkerProvider> FrameLoaderClientImpl::createServiceWorkerProvider()
{
    if (!m_webFrame->client())
        return nullptr;
    return adoptPtr(m_webFrame->client()->createServiceWorkerProvider(m_webFrame));
}

bool FrameLoaderClientImpl::isControlledByServiceWorker(DocumentLoader& loader)
{
    return m_webFrame->client() && m_webFrame->client()->isControlledByServiceWorker(*WebDataSourceImpl::fromDocumentLoader(&loader));
}

int64_t FrameLoaderClientImpl::serviceWorkerID(DocumentLoader& loader)
{
    if (!m_webFrame->client())
        return -1;
    return m_webFrame->client()->serviceWorkerID(*WebDataSourceImpl::fromDocumentLoader(&loader));
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

void FrameLoaderClientImpl::dispatchDidChangeManifest()
{
    if (m_webFrame->client())
        m_webFrame->client()->didChangeManifest(m_webFrame);
}

unsigned FrameLoaderClientImpl::backForwardLength()
{
    WebViewImpl* webview = m_webFrame->viewImpl();
    if (!webview || !webview->client())
        return 0;
    return webview->client()->historyBackListCount() + 1 + webview->client()->historyForwardListCount();
}

void FrameLoaderClientImpl::suddenTerminationDisablerChanged(bool present, SuddenTerminationDisablerType type)
{
    if (m_webFrame->client()) {
        m_webFrame->client()->suddenTerminationDisablerChanged(
            present, static_cast<WebFrameClient::SuddenTerminationDisablerType>(type));
    }
}

} // namespace blink
