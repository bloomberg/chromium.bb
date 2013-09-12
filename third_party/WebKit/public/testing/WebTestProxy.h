/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebTestProxy_h
#define WebTestProxy_h

#include "WebTask.h"
#include "WebTestCommon.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebAXEnums.h"
#include "public/web/WebDOMMessageEvent.h"
#include "public/web/WebDataSource.h"
#include "public/web/WebDragOperation.h"
#include "public/web/WebEditingAction.h"
#include "public/web/WebIconURL.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebNavigationType.h"
#include "public/web/WebSecurityOrigin.h"
#include "public/web/WebTextAffinity.h"
#include "public/web/WebTextDirection.h"
#include <map>
#include <memory>
#include <string>

namespace WebKit {
class WebAXObject;
class WebAudioDevice;
class WebCachedURLRequest;
class WebColorChooser;
class WebColorChooserClient;
class WebDataSource;
class WebDeviceOrientationClient;
class WebDeviceOrientationClientMock;
class WebDragData;
class WebFileChooserCompletion;
class WebFrame;
class WebGeolocationClient;
class WebGeolocationClientMock;
class WebImage;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMIDIClient;
class WebMIDIClientMock;
class WebNode;
class WebNotificationPresenter;
class WebPlugin;
class WebRange;
class WebSerializedScriptValue;
class WebSpeechInputController;
class WebSpeechInputListener;
class WebSpeechRecognizer;
class WebSpellCheckClient;
class WebString;
class WebURL;
class WebURLResponse;
class WebUserMediaClient;
class WebValidationMessageClient;
class WebView;
class WebWidget;
struct WebConsoleMessage;
struct WebContextMenuData;
struct WebFileChooserParams;
struct WebPluginParams;
struct WebPoint;
struct WebSize;
struct WebWindowFeatures;
typedef unsigned WebColor;
}

class SkCanvas;

namespace WebTestRunner {

class MockWebSpeechInputController;
class MockWebSpeechRecognizer;
class MockWebValidationMessageClient;
class SpellCheckClient;
class TestInterfaces;
class WebTestDelegate;
class WebTestInterfaces;
class WebTestRunner;
class WebUserMediaClientMock;

class WEBTESTRUNNER_EXPORT WebTestProxyBase {
public:
    void setInterfaces(WebTestInterfaces*);
    void setDelegate(WebTestDelegate*);
    void setWidget(WebKit::WebWidget*);

    void reset();

    WebKit::WebSpellCheckClient *spellCheckClient() const;
    WebKit::WebValidationMessageClient* validationMessageClient();
    WebKit::WebColorChooser* createColorChooser(WebKit::WebColorChooserClient*, const WebKit::WebColor&);
    bool runFileChooser(const WebKit::WebFileChooserParams&, WebKit::WebFileChooserCompletion*);

    std::string captureTree(bool debugRenderTree);
    SkCanvas* capturePixels();

    void setLogConsoleOutput(bool enabled);

    // FIXME: Make this private again.
    void scheduleComposite();

    void didOpenChooser();
    void didCloseChooser();
    bool isChooserShown();

#if WEBTESTRUNNER_IMPLEMENTATION
    void display();
    void displayInvalidatedRegion();
    void discardBackingStore();

    WebKit::WebDeviceOrientationClientMock* deviceOrientationClientMock();
    WebKit::WebGeolocationClientMock* geolocationClientMock();
    WebKit::WebMIDIClientMock* midiClientMock();
    MockWebSpeechInputController* speechInputControllerMock();
    MockWebSpeechRecognizer* speechRecognizerMock();
#endif

    WebTaskList* taskList() { return &m_taskList; }

    WebKit::WebView* webView();

protected:
    WebTestProxyBase();
    ~WebTestProxyBase();

    void didInvalidateRect(const WebKit::WebRect&);
    void didScrollRect(int, int, const WebKit::WebRect&);
    void scheduleAnimation();
    void setWindowRect(const WebKit::WebRect&);
    void show(WebKit::WebNavigationPolicy);
    void didAutoResize(const WebKit::WebSize&);
    void postAccessibilityEvent(const WebKit::WebAXObject&, WebKit::WebAXEvent);
    void startDragging(WebKit::WebFrame*, const WebKit::WebDragData&, WebKit::WebDragOperationsMask, const WebKit::WebImage&, const WebKit::WebPoint&);
    bool shouldBeginEditing(const WebKit::WebRange&);
    bool shouldEndEditing(const WebKit::WebRange&);
    bool shouldInsertNode(const WebKit::WebNode&, const WebKit::WebRange&, WebKit::WebEditingAction);
    bool shouldInsertText(const WebKit::WebString& text, const WebKit::WebRange&, WebKit::WebEditingAction);
    bool shouldChangeSelectedRange(const WebKit::WebRange& fromRange, const WebKit::WebRange& toRange, WebKit::WebTextAffinity, bool stillSelecting);
    bool shouldDeleteRange(const WebKit::WebRange&);
    bool shouldApplyStyle(const WebKit::WebString& style, const WebKit::WebRange&);
    void didBeginEditing();
    void didChangeSelection(bool isEmptySelection);
    void didChangeContents();
    void didEndEditing();
    bool createView(WebKit::WebFrame* creator, const WebKit::WebURLRequest&, const WebKit::WebWindowFeatures&, const WebKit::WebString& frameName, WebKit::WebNavigationPolicy);
    WebKit::WebPlugin* createPlugin(WebKit::WebFrame*, const WebKit::WebPluginParams&);
    void setStatusText(const WebKit::WebString&);
    void didStopLoading();
    void showContextMenu(WebKit::WebFrame*, const WebKit::WebContextMenuData&);
    WebKit::WebUserMediaClient* userMediaClient();
    void printPage(WebKit::WebFrame*);
    WebKit::WebNotificationPresenter* notificationPresenter();
    WebKit::WebGeolocationClient* geolocationClient();
    WebKit::WebMIDIClient* webMIDIClient();
    WebKit::WebSpeechInputController* speechInputController(WebKit::WebSpeechInputListener*);
    WebKit::WebSpeechRecognizer* speechRecognizer();
    WebKit::WebDeviceOrientationClient* deviceOrientationClient();
    bool requestPointerLock();
    void requestPointerUnlock();
    bool isPointerLocked();
    void didFocus();
    void didBlur();
    void setToolTipText(const WebKit::WebString&, WebKit::WebTextDirection);
    void didAddMessageToConsole(const WebKit::WebConsoleMessage&, const WebKit::WebString& sourceName, unsigned sourceLine);
    void runModalAlertDialog(WebKit::WebFrame*, const WebKit::WebString&);
    bool runModalConfirmDialog(WebKit::WebFrame*, const WebKit::WebString&);
    bool runModalPromptDialog(WebKit::WebFrame*, const WebKit::WebString& message, const WebKit::WebString& defaultValue, WebKit::WebString* actualValue);
    bool runModalBeforeUnloadDialog(WebKit::WebFrame*, const WebKit::WebString&);

    void didStartProvisionalLoad(WebKit::WebFrame*);
    void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*);
    bool didFailProvisionalLoad(WebKit::WebFrame*, const WebKit::WebURLError&);
    void didCommitProvisionalLoad(WebKit::WebFrame*, bool isNewNavigation);
    void didReceiveTitle(WebKit::WebFrame*, const WebKit::WebString& title, WebKit::WebTextDirection);
    void didChangeIcon(WebKit::WebFrame*, WebKit::WebIconURL::Type);
    void didFinishDocumentLoad(WebKit::WebFrame*);
    void didHandleOnloadEvents(WebKit::WebFrame*);
    void didFailLoad(WebKit::WebFrame*, const WebKit::WebURLError&);
    void didFinishLoad(WebKit::WebFrame*);
    void didChangeLocationWithinPage(WebKit::WebFrame*);
    void didDetectXSS(WebKit::WebFrame*, const WebKit::WebURL& insecureURL, bool didBlockEntirePage);
    void didDispatchPingLoader(WebKit::WebFrame*, const WebKit::WebURL&);
    void willRequestResource(WebKit::WebFrame*, const WebKit::WebCachedURLRequest&);
    void didCreateDataSource(WebKit::WebFrame*, WebKit::WebDataSource*);
    void willSendRequest(WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&, const WebKit::WebURLResponse& redirectResponse);
    void didReceiveResponse(WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLResponse&);
    void didChangeResourcePriority(WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLRequest::Priority&);
    void didFinishResourceLoad(WebKit::WebFrame*, unsigned identifier);
    WebKit::WebNavigationPolicy decidePolicyForNavigation(WebKit::WebFrame*, WebKit::WebDataSource::ExtraData*, const WebKit::WebURLRequest&, WebKit::WebNavigationType, WebKit::WebNavigationPolicy defaultPolicy, bool isRedirect);
    bool willCheckAndDispatchMessageEvent(WebKit::WebFrame* sourceFrame, WebKit::WebFrame* targetFrame, WebKit::WebSecurityOrigin target, WebKit::WebDOMMessageEvent);
    void resetInputMethod();

private:
    template<class, typename, typename> friend class WebFrameTestProxy;
    void locationChangeDone(WebKit::WebFrame*);
    void paintRect(const WebKit::WebRect&);
    void paintInvalidatedRegion();
    void paintPagesWithBoundaries();
    SkCanvas* canvas();
    void displayRepaintMask();
    void invalidateAll();
    void animateNow();

    WebKit::WebWidget* webWidget();

    TestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;
    WebKit::WebWidget* m_webWidget;

    WebTaskList m_taskList;

    std::auto_ptr<SpellCheckClient> m_spellcheck;
    std::auto_ptr<WebUserMediaClientMock> m_userMediaClient;

    // Painting.
    std::auto_ptr<SkCanvas> m_canvas;
    WebKit::WebRect m_paintRect;
    bool m_isPainting;
    bool m_animateScheduled;
    std::map<unsigned, std::string> m_resourceIdentifierMap;
    std::map<unsigned, WebKit::WebURLRequest> m_requestMap;

    bool m_logConsoleOutput;
    int m_chooserCount;

    std::auto_ptr<WebKit::WebGeolocationClientMock> m_geolocationClient;
    std::auto_ptr<WebKit::WebMIDIClientMock> m_midiClient;
    std::auto_ptr<WebKit::WebDeviceOrientationClientMock> m_deviceOrientationClient;
    std::auto_ptr<MockWebSpeechRecognizer> m_speechRecognizer;
    std::auto_ptr<MockWebSpeechInputController> m_speechInputController;
    std::auto_ptr<MockWebValidationMessageClient> m_validationMessageClient;

private:
    WebTestProxyBase(WebTestProxyBase&);
    WebTestProxyBase& operator=(const WebTestProxyBase&);
};

// Use this template to inject methods into your WebViewClient/WebFrameClient
// implementation required for the running layout tests.
template<class Base, typename T>
class WebTestProxy : public Base, public WebTestProxyBase {
public:
    explicit WebTestProxy(T t)
        : Base(t)
    {
    }

    virtual ~WebTestProxy() { }

    // WebViewClient implementation.
    virtual void didInvalidateRect(const WebKit::WebRect& rect)
    {
        WebTestProxyBase::didInvalidateRect(rect);
    }
    virtual void didScrollRect(int dx, int dy, const WebKit::WebRect& clipRect)
    {
        WebTestProxyBase::didScrollRect(dx, dy, clipRect);
    }
    virtual void scheduleComposite()
    {
        WebTestProxyBase::scheduleComposite();
    }
    virtual void scheduleAnimation()
    {
        WebTestProxyBase::scheduleAnimation();
    }
    virtual void setWindowRect(const WebKit::WebRect& rect)
    {
        WebTestProxyBase::setWindowRect(rect);
        Base::setWindowRect(rect);
    }
    virtual void show(WebKit::WebNavigationPolicy policy)
    {
        WebTestProxyBase::show(policy);
        Base::show(policy);
    }
    virtual void didAutoResize(const WebKit::WebSize& newSize)
    {
        WebTestProxyBase::didAutoResize(newSize);
        Base::didAutoResize(newSize);
    }
    virtual void postAccessibilityEvent(const WebKit::WebAXObject& object, WebKit::WebAXEvent event)
    {
        WebTestProxyBase::postAccessibilityEvent(object, event);
        Base::postAccessibilityEvent(object, event);
    }
    virtual void startDragging(WebKit::WebFrame* frame, const WebKit::WebDragData& data, WebKit::WebDragOperationsMask mask, const WebKit::WebImage& image, const WebKit::WebPoint& point)
    {
        WebTestProxyBase::startDragging(frame, data, mask, image, point);
        // Don't forward this call to Base because we don't want to do a real drag-and-drop.
    }
    virtual bool shouldBeginEditing(const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldBeginEditing(range);
        return Base::shouldBeginEditing(range);
    }
    virtual bool shouldEndEditing(const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldEndEditing(range);
        return Base::shouldEndEditing(range);
    }
    virtual bool shouldInsertNode(const WebKit::WebNode& node, const WebKit::WebRange& range, WebKit::WebEditingAction action)
    {
        WebTestProxyBase::shouldInsertNode(node, range, action);
        return Base::shouldInsertNode(node, range, action);
    }
    virtual bool shouldInsertText(const WebKit::WebString& text, const WebKit::WebRange& range, WebKit::WebEditingAction action)
    {
        WebTestProxyBase::shouldInsertText(text, range, action);
        return Base::shouldInsertText(text, range, action);
    }
    virtual bool shouldChangeSelectedRange(const WebKit::WebRange& fromRange, const WebKit::WebRange& toRange, WebKit::WebTextAffinity affinity, bool stillSelecting)
    {
        WebTestProxyBase::shouldChangeSelectedRange(fromRange, toRange, affinity, stillSelecting);
        return Base::shouldChangeSelectedRange(fromRange, toRange, affinity, stillSelecting);
    }
    virtual bool shouldDeleteRange(const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldDeleteRange(range);
        return Base::shouldDeleteRange(range);
    }
    virtual bool shouldApplyStyle(const WebKit::WebString& style, const WebKit::WebRange& range)
    {
        WebTestProxyBase::shouldApplyStyle(style, range);
        return Base::shouldApplyStyle(style, range);
    }
    virtual void didBeginEditing()
    {
        WebTestProxyBase::didBeginEditing();
        Base::didBeginEditing();
    }
    virtual void didChangeSelection(bool isEmptySelection)
    {
        WebTestProxyBase::didChangeSelection(isEmptySelection);
        Base::didChangeSelection(isEmptySelection);
    }
    virtual void didChangeContents()
    {
        WebTestProxyBase::didChangeContents();
        Base::didChangeContents();
    }
    virtual void didEndEditing()
    {
        WebTestProxyBase::didEndEditing();
        Base::didEndEditing();
    }
    virtual WebKit::WebView* createView(WebKit::WebFrame* creator, const WebKit::WebURLRequest& request, const WebKit::WebWindowFeatures& features, const WebKit::WebString& frameName, WebKit::WebNavigationPolicy policy)
    {
        if (!WebTestProxyBase::createView(creator, request, features, frameName, policy))
            return 0;
        return Base::createView(creator, request, features, frameName, policy);
    }
    virtual void setStatusText(const WebKit::WebString& text)
    {
        WebTestProxyBase::setStatusText(text);
        Base::setStatusText(text);
    }
    virtual void didStopLoading()
    {
        WebTestProxyBase::didStopLoading();
        Base::didStopLoading();
    }
    virtual void showContextMenu(WebKit::WebFrame* frame, const WebKit::WebContextMenuData& contextMenuData)
    {
        WebTestProxyBase::showContextMenu(frame, contextMenuData);
        Base::showContextMenu(frame, contextMenuData);
    }
    virtual WebKit::WebUserMediaClient* userMediaClient()
    {
        return WebTestProxyBase::userMediaClient();
    }
    virtual void printPage(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::printPage(frame);
    }
    virtual WebKit::WebNotificationPresenter* notificationPresenter()
    {
        return WebTestProxyBase::notificationPresenter();
    }
    virtual WebKit::WebGeolocationClient* geolocationClient()
    {
        return WebTestProxyBase::geolocationClient();
    }
    virtual WebKit::WebMIDIClient* webMIDIClient()
    {
        return WebTestProxyBase::webMIDIClient();
    }
    virtual WebKit::WebSpeechInputController* speechInputController(WebKit::WebSpeechInputListener* listener)
    {
        return WebTestProxyBase::speechInputController(listener);
    }
    virtual WebKit::WebSpeechRecognizer* speechRecognizer()
    {
        return WebTestProxyBase::speechRecognizer();
    }
    virtual WebKit::WebDeviceOrientationClient* deviceOrientationClient()
    {
        return WebTestProxyBase::deviceOrientationClient();
    }
    virtual bool requestPointerLock()
    {
        return WebTestProxyBase::requestPointerLock();
    }
    virtual void requestPointerUnlock()
    {
        WebTestProxyBase::requestPointerUnlock();
    }
    virtual bool isPointerLocked()
    {
        return WebTestProxyBase::isPointerLocked();
    }
    virtual void didFocus()
    {
        WebTestProxyBase::didFocus();
        Base::didFocus();
    }
    virtual void didBlur()
    {
        WebTestProxyBase::didBlur();
        Base::didBlur();
    }
    virtual void setToolTipText(const WebKit::WebString& text, WebKit::WebTextDirection hint)
    {
        WebTestProxyBase::setToolTipText(text, hint);
        Base::setToolTipText(text, hint);
    }
    virtual void resetInputMethod()
    {
        WebTestProxyBase::resetInputMethod();
    }

    virtual void didStartProvisionalLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didStartProvisionalLoad(frame);
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didReceiveServerRedirectForProvisionalLoad(frame);
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        // If the test finished, don't notify the embedder of the failed load,
        // as we already destroyed the document loader.
        if (WebTestProxyBase::didFailProvisionalLoad(frame, error))
            return;
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame, bool isNewNavigation)
    {
        WebTestProxyBase::didCommitProvisionalLoad(frame, isNewNavigation);
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(WebKit::WebFrame* frame, const WebKit::WebString& title, WebKit::WebTextDirection direction)
    {
        WebTestProxyBase::didReceiveTitle(frame, title, direction);
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didChangeIcon(WebKit::WebFrame* frame, WebKit::WebIconURL::Type iconType)
    {
        WebTestProxyBase::didChangeIcon(frame, iconType);
        Base::didChangeIcon(frame, iconType);
    }
    virtual void didFinishDocumentLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didFinishDocumentLoad(frame);
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didHandleOnloadEvents(frame);
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(WebKit::WebFrame* frame, const WebKit::WebURLError& error)
    {
        WebTestProxyBase::didFailLoad(frame, error);
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(WebKit::WebFrame* frame)
    {
        WebTestProxyBase::didFinishLoad(frame);
        Base::didFinishLoad(frame);
    }
    virtual void didDetectXSS(WebKit::WebFrame* frame, const WebKit::WebURL& insecureURL, bool didBlockEntirePage)
    {
        WebTestProxyBase::didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
    virtual void willRequestResource(WebKit::WebFrame* frame, const WebKit::WebCachedURLRequest& request)
    {
        WebTestProxyBase::willRequestResource(frame, request);
        Base::willRequestResource(frame, request);
    }
    virtual void didCreateDataSource(WebKit::WebFrame* frame, WebKit::WebDataSource* ds)
    {
        WebTestProxyBase::didCreateDataSource(frame, ds);
        Base::didCreateDataSource(frame, ds);
    }
    virtual void willSendRequest(WebKit::WebFrame* frame, unsigned identifier, WebKit::WebURLRequest& request, const WebKit::WebURLResponse& redirectResponse)
    {
        WebTestProxyBase::willSendRequest(frame, identifier, request, redirectResponse);
        Base::willSendRequest(frame, identifier, request, redirectResponse);
    }
    virtual void didReceiveResponse(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLResponse& response)
    {
        WebTestProxyBase::didReceiveResponse(frame, identifier, response);
        Base::didReceiveResponse(frame, identifier, response);
    }
    virtual void didChangeResourcePriority(WebKit::WebFrame* frame, unsigned identifier, const WebKit::WebURLRequest::Priority& priority)
    {
        WebTestProxyBase::didChangeResourcePriority(frame, identifier, priority);
        Base::didChangeResourcePriority(frame, identifier, priority);
    }
    virtual void didFinishResourceLoad(WebKit::WebFrame* frame, unsigned identifier)
    {
        WebTestProxyBase::didFinishResourceLoad(frame, identifier);
        Base::didFinishResourceLoad(frame, identifier);
    }
    virtual void didAddMessageToConsole(const WebKit::WebConsoleMessage& message, const WebKit::WebString& sourceName, unsigned sourceLine, const WebKit::WebString& stackTrace)
    {
        WebTestProxyBase::didAddMessageToConsole(message, sourceName, sourceLine);
        Base::didAddMessageToConsole(message, sourceName, sourceLine, stackTrace);
    }
    virtual void runModalAlertDialog(WebKit::WebFrame* frame, const WebKit::WebString& message)
    {
        WebTestProxyBase::runModalAlertDialog(frame, message);
        Base::runModalAlertDialog(frame, message);
    }
    virtual bool runModalConfirmDialog(WebKit::WebFrame* frame, const WebKit::WebString& message)
    {
        WebTestProxyBase::runModalConfirmDialog(frame, message);
        return Base::runModalConfirmDialog(frame, message);
    }
    virtual bool runModalPromptDialog(WebKit::WebFrame* frame, const WebKit::WebString& message, const WebKit::WebString& defaultValue, WebKit::WebString* actualValue)
    {
        WebTestProxyBase::runModalPromptDialog(frame, message, defaultValue, actualValue);
        return Base::runModalPromptDialog(frame, message, defaultValue, actualValue);
    }
    virtual bool runModalBeforeUnloadDialog(WebKit::WebFrame* frame, const WebKit::WebString& message)
    {
        return WebTestProxyBase::runModalBeforeUnloadDialog(frame, message);
    }
    virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(WebKit::WebFrame* frame, WebKit::WebDataSource::ExtraData* extraData, const WebKit::WebURLRequest& request, WebKit::WebNavigationType type, WebKit::WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        WebKit::WebNavigationPolicy policy = WebTestProxyBase::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
        if (policy == WebKit::WebNavigationPolicyIgnore)
            return policy;
        return Base::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
    }
    virtual bool willCheckAndDispatchMessageEvent(WebKit::WebFrame* sourceFrame, WebKit::WebFrame* targetFrame, WebKit::WebSecurityOrigin target, WebKit::WebDOMMessageEvent event)
    {
        if (WebTestProxyBase::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event))
            return true;
        return Base::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event);
    }
    virtual WebKit::WebColorChooser* createColorChooser(WebKit::WebColorChooserClient* client, const WebKit::WebColor& color)
    {
        return WebTestProxyBase::createColorChooser(client, color);
    }
    virtual bool runFileChooser(const WebKit::WebFileChooserParams& params, WebKit::WebFileChooserCompletion* completion)
    {
        return WebTestProxyBase::runFileChooser(params, completion);
    }
};

}

#endif // WebTestProxy_h
