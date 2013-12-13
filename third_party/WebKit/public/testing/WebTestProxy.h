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

#include "WebScopedPtr.h"
#include "WebTask.h"
#include "WebTestCommon.h"
#include "public/platform/WebNonCopyable.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebAXEnums.h"
#include "public/web/WebDOMMessageEvent.h"
#include "public/web/WebDataSource.h"
#include "public/web/WebDragOperation.h"
#include "public/web/WebIconURL.h"
#include "public/web/WebNavigationPolicy.h"
#include "public/web/WebNavigationType.h"
#include "public/web/WebSecurityOrigin.h"
#include "public/web/WebTextAffinity.h"
#include "public/web/WebTextDirection.h"
#include <map>
#include <string>

namespace blink {
class WebAXObject;
class WebAudioDevice;
class WebCachedURLRequest;
class WebColorChooser;
class WebColorChooserClient;
class WebDataSource;
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
struct WebColorSuggestion;
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
    void setWidget(blink::WebWidget*);

    void reset();

    blink::WebSpellCheckClient *spellCheckClient() const;
    blink::WebValidationMessageClient* validationMessageClient();
    blink::WebColorChooser* createColorChooser(blink::WebColorChooserClient*, const blink::WebColor&);
    blink::WebColorChooser* createColorChooser(blink::WebColorChooserClient*, const blink::WebColor&, const blink::WebVector<blink::WebColorSuggestion>& suggestions);
    bool runFileChooser(const blink::WebFileChooserParams&, blink::WebFileChooserCompletion*);

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

    blink::WebGeolocationClientMock* geolocationClientMock();
    blink::WebMIDIClientMock* midiClientMock();
    MockWebSpeechInputController* speechInputControllerMock();
    MockWebSpeechRecognizer* speechRecognizerMock();
#endif

    WebTaskList* taskList() { return &m_taskList; }

    blink::WebView* webView();

    void didForceResize();

    void postSpellCheckEvent(const blink::WebString& eventName);

protected:
    WebTestProxyBase();
    ~WebTestProxyBase();

    void didInvalidateRect(const blink::WebRect&);
    void didScrollRect(int, int, const blink::WebRect&);
    void scheduleAnimation();
    // FIXME: Remove once we switch to use didForceResize.
    void setWindowRect(const blink::WebRect&);
    void show(blink::WebNavigationPolicy);
    void didAutoResize(const blink::WebSize&);
    void postAccessibilityEvent(const blink::WebAXObject&, blink::WebAXEvent);
    void startDragging(blink::WebFrame*, const blink::WebDragData&, blink::WebDragOperationsMask, const blink::WebImage&, const blink::WebPoint&);
    void didChangeSelection(bool isEmptySelection);
    void didChangeContents();
    void didEndEditing();
    bool createView(blink::WebFrame* creator, const blink::WebURLRequest&, const blink::WebWindowFeatures&, const blink::WebString& frameName, blink::WebNavigationPolicy, bool suppressOpener);
    blink::WebPlugin* createPlugin(blink::WebFrame*, const blink::WebPluginParams&);
    void setStatusText(const blink::WebString&);
    void didStopLoading();
    void showContextMenu(blink::WebFrame*, const blink::WebContextMenuData&);
    blink::WebUserMediaClient* userMediaClient();
    void printPage(blink::WebFrame*);
    blink::WebNotificationPresenter* notificationPresenter();
    blink::WebGeolocationClient* geolocationClient();
    blink::WebMIDIClient* webMIDIClient();
    blink::WebSpeechInputController* speechInputController(blink::WebSpeechInputListener*);
    blink::WebSpeechRecognizer* speechRecognizer();
    bool requestPointerLock();
    void requestPointerUnlock();
    bool isPointerLocked();
    void didFocus();
    void didBlur();
    void setToolTipText(const blink::WebString&, blink::WebTextDirection);
    void didAddMessageToConsole(const blink::WebConsoleMessage&, const blink::WebString& sourceName, unsigned sourceLine);
    void runModalAlertDialog(blink::WebFrame*, const blink::WebString&);
    bool runModalConfirmDialog(blink::WebFrame*, const blink::WebString&);
    bool runModalPromptDialog(blink::WebFrame*, const blink::WebString& message, const blink::WebString& defaultValue, blink::WebString* actualValue);
    bool runModalBeforeUnloadDialog(blink::WebFrame*, const blink::WebString&);

    void didStartProvisionalLoad(blink::WebFrame*);
    void didReceiveServerRedirectForProvisionalLoad(blink::WebFrame*);
    bool didFailProvisionalLoad(blink::WebFrame*, const blink::WebURLError&);
    void didCommitProvisionalLoad(blink::WebFrame*, bool isNewNavigation);
    void didReceiveTitle(blink::WebFrame*, const blink::WebString& title, blink::WebTextDirection);
    void didChangeIcon(blink::WebFrame*, blink::WebIconURL::Type);
    void didFinishDocumentLoad(blink::WebFrame*);
    void didHandleOnloadEvents(blink::WebFrame*);
    void didFailLoad(blink::WebFrame*, const blink::WebURLError&);
    void didFinishLoad(blink::WebFrame*);
    void didChangeLocationWithinPage(blink::WebFrame*);
    void didDetectXSS(blink::WebFrame*, const blink::WebURL& insecureURL, bool didBlockEntirePage);
    void didDispatchPingLoader(blink::WebFrame*, const blink::WebURL&);
    void willRequestResource(blink::WebFrame*, const blink::WebCachedURLRequest&);
    void didCreateDataSource(blink::WebFrame*, blink::WebDataSource*);
    void willSendRequest(blink::WebFrame*, unsigned identifier, blink::WebURLRequest&, const blink::WebURLResponse& redirectResponse);
    void didReceiveResponse(blink::WebFrame*, unsigned identifier, const blink::WebURLResponse&);
    void didChangeResourcePriority(blink::WebFrame*, unsigned identifier, const blink::WebURLRequest::Priority&);
    void didFinishResourceLoad(blink::WebFrame*, unsigned identifier);
    blink::WebNavigationPolicy decidePolicyForNavigation(blink::WebFrame*, blink::WebDataSource::ExtraData*, const blink::WebURLRequest&, blink::WebNavigationType, blink::WebNavigationPolicy defaultPolicy, bool isRedirect);
    bool willCheckAndDispatchMessageEvent(blink::WebFrame* sourceFrame, blink::WebFrame* targetFrame, blink::WebSecurityOrigin target, blink::WebDOMMessageEvent);
    void resetInputMethod();

private:
    template<class, typename, typename> friend class WebFrameTestProxy;
    void locationChangeDone(blink::WebFrame*);
    void paintRect(const blink::WebRect&);
    void paintInvalidatedRegion();
    void paintPagesWithBoundaries();
    SkCanvas* canvas();
    void displayRepaintMask();
    void invalidateAll();
    void animateNow();

    blink::WebWidget* webWidget();

    TestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;
    blink::WebWidget* m_webWidget;

    WebTaskList m_taskList;

    WebScopedPtr<SpellCheckClient> m_spellcheck;
    WebScopedPtr<WebUserMediaClientMock> m_userMediaClient;

    // Painting.
    WebScopedPtr<SkCanvas> m_canvas;
    blink::WebRect m_paintRect;
    bool m_isPainting;
    bool m_animateScheduled;
    std::map<unsigned, std::string> m_resourceIdentifierMap;
    std::map<unsigned, blink::WebURLRequest> m_requestMap;

    bool m_logConsoleOutput;
    int m_chooserCount;

    WebScopedPtr<blink::WebGeolocationClientMock> m_geolocationClient;
    WebScopedPtr<blink::WebMIDIClientMock> m_midiClient;
    WebScopedPtr<MockWebSpeechRecognizer> m_speechRecognizer;
    WebScopedPtr<MockWebSpeechInputController> m_speechInputController;
    WebScopedPtr<MockWebValidationMessageClient> m_validationMessageClient;

    // FIXME:: We want to move away from this pattern and mark classes
    // as Noncopyable, but this class is marked as WEBTESTRUNNER_EXPORT
    // while WebNonCopyable is not, so we cannot inherit from WebNonCopyable.
    // To overcome the problem, for now not inheriting from WebNonCopyable
    // but plan to fix it when we make the change of making WebNonCopyable
    // a macro rather than class. We will have a single way to mark all classes
    // as Noncopyable.
    // Tracked under: http://code.google.com/p/chromium/issues/detail?id=229178
private:
    WebTestProxyBase(WebTestProxyBase&);
    WebTestProxyBase& operator=(const WebTestProxyBase&);
};

// Use this template to inject methods into your WebViewClient/WebFrameClient
// implementation required for the running layout tests.
template<class Base, typename T>
class WebTestProxy : public Base, public WebTestProxyBase, public blink::WebNonCopyable {
public:
    explicit WebTestProxy(T t)
        : Base(t)
    {
    }

    virtual ~WebTestProxy() { }

    // WebViewClient implementation.
    virtual void didInvalidateRect(const blink::WebRect& rect)
    {
        WebTestProxyBase::didInvalidateRect(rect);
    }
    virtual void didScrollRect(int dx, int dy, const blink::WebRect& clipRect)
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
    virtual void setWindowRect(const blink::WebRect& rect)
    {
        WebTestProxyBase::setWindowRect(rect);
        Base::setWindowRect(rect);
    }
    virtual void show(blink::WebNavigationPolicy policy)
    {
        WebTestProxyBase::show(policy);
        Base::show(policy);
    }
    virtual void didAutoResize(const blink::WebSize& newSize)
    {
        WebTestProxyBase::didAutoResize(newSize);
        Base::didAutoResize(newSize);
    }
    virtual void postAccessibilityEvent(const blink::WebAXObject& object, blink::WebAXEvent event)
    {
        WebTestProxyBase::postAccessibilityEvent(object, event);
        Base::postAccessibilityEvent(object, event);
    }
    virtual void startDragging(blink::WebFrame* frame, const blink::WebDragData& data, blink::WebDragOperationsMask mask, const blink::WebImage& image, const blink::WebPoint& point)
    {
        WebTestProxyBase::startDragging(frame, data, mask, image, point);
        // Don't forward this call to Base because we don't want to do a real drag-and-drop.
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
    virtual blink::WebView* createView(blink::WebFrame* creator, const blink::WebURLRequest& request, const blink::WebWindowFeatures& features, const blink::WebString& frameName, blink::WebNavigationPolicy policy, bool suppressOpener)
    {
        if (!WebTestProxyBase::createView(creator, request, features, frameName, policy, suppressOpener))
            return 0;
        return Base::createView(creator, request, features, frameName, policy, suppressOpener);
    }
    virtual void setStatusText(const blink::WebString& text)
    {
        WebTestProxyBase::setStatusText(text);
        Base::setStatusText(text);
    }
    virtual void didStopLoading()
    {
        WebTestProxyBase::didStopLoading();
        Base::didStopLoading();
    }
    virtual void showContextMenu(blink::WebFrame* frame, const blink::WebContextMenuData& contextMenuData)
    {
        WebTestProxyBase::showContextMenu(frame, contextMenuData);
        Base::showContextMenu(frame, contextMenuData);
    }
    virtual blink::WebUserMediaClient* userMediaClient()
    {
        return WebTestProxyBase::userMediaClient();
    }
    virtual void printPage(blink::WebFrame* frame)
    {
        WebTestProxyBase::printPage(frame);
    }
    virtual blink::WebNotificationPresenter* notificationPresenter()
    {
        return WebTestProxyBase::notificationPresenter();
    }
    virtual blink::WebGeolocationClient* geolocationClient()
    {
        return WebTestProxyBase::geolocationClient();
    }
    virtual blink::WebMIDIClient* webMIDIClient()
    {
        return WebTestProxyBase::webMIDIClient();
    }
    virtual blink::WebSpeechInputController* speechInputController(blink::WebSpeechInputListener* listener)
    {
        return WebTestProxyBase::speechInputController(listener);
    }
    virtual blink::WebSpeechRecognizer* speechRecognizer()
    {
        return WebTestProxyBase::speechRecognizer();
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
    virtual void setToolTipText(const blink::WebString& text, blink::WebTextDirection hint)
    {
        WebTestProxyBase::setToolTipText(text, hint);
        Base::setToolTipText(text, hint);
    }
    virtual void resetInputMethod()
    {
        WebTestProxyBase::resetInputMethod();
    }

    virtual void didStartProvisionalLoad(blink::WebFrame* frame)
    {
        WebTestProxyBase::didStartProvisionalLoad(frame);
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(blink::WebFrame* frame)
    {
        WebTestProxyBase::didReceiveServerRedirectForProvisionalLoad(frame);
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(blink::WebFrame* frame, const blink::WebURLError& error)
    {
        // If the test finished, don't notify the embedder of the failed load,
        // as we already destroyed the document loader.
        if (WebTestProxyBase::didFailProvisionalLoad(frame, error))
            return;
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(blink::WebFrame* frame, bool isNewNavigation)
    {
        WebTestProxyBase::didCommitProvisionalLoad(frame, isNewNavigation);
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(blink::WebFrame* frame, const blink::WebString& title, blink::WebTextDirection direction)
    {
        WebTestProxyBase::didReceiveTitle(frame, title, direction);
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didChangeIcon(blink::WebFrame* frame, blink::WebIconURL::Type iconType)
    {
        WebTestProxyBase::didChangeIcon(frame, iconType);
        Base::didChangeIcon(frame, iconType);
    }
    virtual void didFinishDocumentLoad(blink::WebFrame* frame)
    {
        WebTestProxyBase::didFinishDocumentLoad(frame);
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(blink::WebFrame* frame)
    {
        WebTestProxyBase::didHandleOnloadEvents(frame);
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(blink::WebFrame* frame, const blink::WebURLError& error)
    {
        WebTestProxyBase::didFailLoad(frame, error);
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(blink::WebFrame* frame)
    {
        WebTestProxyBase::didFinishLoad(frame);
        Base::didFinishLoad(frame);
    }
    virtual void didDetectXSS(blink::WebFrame* frame, const blink::WebURL& insecureURL, bool didBlockEntirePage)
    {
        WebTestProxyBase::didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
    virtual void willRequestResource(blink::WebFrame* frame, const blink::WebCachedURLRequest& request)
    {
        WebTestProxyBase::willRequestResource(frame, request);
        Base::willRequestResource(frame, request);
    }
    virtual void didCreateDataSource(blink::WebFrame* frame, blink::WebDataSource* ds)
    {
        WebTestProxyBase::didCreateDataSource(frame, ds);
        Base::didCreateDataSource(frame, ds);
    }
    virtual void willSendRequest(blink::WebFrame* frame, unsigned identifier, blink::WebURLRequest& request, const blink::WebURLResponse& redirectResponse)
    {
        WebTestProxyBase::willSendRequest(frame, identifier, request, redirectResponse);
        Base::willSendRequest(frame, identifier, request, redirectResponse);
    }
    virtual void didReceiveResponse(blink::WebFrame* frame, unsigned identifier, const blink::WebURLResponse& response)
    {
        WebTestProxyBase::didReceiveResponse(frame, identifier, response);
        Base::didReceiveResponse(frame, identifier, response);
    }
    virtual void didChangeResourcePriority(blink::WebFrame* frame, unsigned identifier, const blink::WebURLRequest::Priority& priority)
    {
        WebTestProxyBase::didChangeResourcePriority(frame, identifier, priority);
        Base::didChangeResourcePriority(frame, identifier, priority);
    }
    virtual void didFinishResourceLoad(blink::WebFrame* frame, unsigned identifier)
    {
        WebTestProxyBase::didFinishResourceLoad(frame, identifier);
        Base::didFinishResourceLoad(frame, identifier);
    }
    virtual void didAddMessageToConsole(const blink::WebConsoleMessage& message, const blink::WebString& sourceName, unsigned sourceLine, const blink::WebString& stackTrace)
    {
        WebTestProxyBase::didAddMessageToConsole(message, sourceName, sourceLine);
        Base::didAddMessageToConsole(message, sourceName, sourceLine, stackTrace);
    }
    virtual void runModalAlertDialog(blink::WebFrame* frame, const blink::WebString& message)
    {
        WebTestProxyBase::runModalAlertDialog(frame, message);
        Base::runModalAlertDialog(frame, message);
    }
    virtual bool runModalConfirmDialog(blink::WebFrame* frame, const blink::WebString& message)
    {
        WebTestProxyBase::runModalConfirmDialog(frame, message);
        return Base::runModalConfirmDialog(frame, message);
    }
    virtual bool runModalPromptDialog(blink::WebFrame* frame, const blink::WebString& message, const blink::WebString& defaultValue, blink::WebString* actualValue)
    {
        WebTestProxyBase::runModalPromptDialog(frame, message, defaultValue, actualValue);
        return Base::runModalPromptDialog(frame, message, defaultValue, actualValue);
    }
    virtual bool runModalBeforeUnloadDialog(blink::WebFrame* frame, const blink::WebString& message)
    {
        return WebTestProxyBase::runModalBeforeUnloadDialog(frame, message);
    }
    virtual blink::WebNavigationPolicy decidePolicyForNavigation(blink::WebFrame* frame, blink::WebDataSource::ExtraData* extraData, const blink::WebURLRequest& request, blink::WebNavigationType type, blink::WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        blink::WebNavigationPolicy policy = WebTestProxyBase::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
        if (policy == blink::WebNavigationPolicyIgnore)
            return policy;
        return Base::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
    }
    virtual bool willCheckAndDispatchMessageEvent(blink::WebFrame* sourceFrame, blink::WebFrame* targetFrame, blink::WebSecurityOrigin target, blink::WebDOMMessageEvent event)
    {
        if (WebTestProxyBase::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event))
            return true;
        return Base::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event);
    }
    virtual blink::WebColorChooser* createColorChooser(blink::WebColorChooserClient* client, const blink::WebColor& color)
    {
        return WebTestProxyBase::createColorChooser(client, color);
    }
    virtual blink::WebColorChooser* createColorChooser(blink::WebColorChooserClient* client, const blink::WebColor& color, const blink::WebVector<blink::WebColorSuggestion>& suggestions)
    {
        return WebTestProxyBase::createColorChooser(client, color, suggestions);
    }
    virtual bool runFileChooser(const blink::WebFileChooserParams& params, blink::WebFileChooserCompletion* completion)
    {
        return WebTestProxyBase::runFileChooser(params, completion);
    }
    virtual void postSpellCheckEvent(const blink::WebString& eventName)
    {
        WebTestProxyBase::postSpellCheckEvent(eventName);
    }
};

}

#endif // WebTestProxy_h
