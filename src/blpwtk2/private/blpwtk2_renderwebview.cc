/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_renderwebview.h>

#include <blpwtk2_statics.h>

#include <base/message_loop/message_loop.h>

namespace blpwtk2 {

                        // -------------------
                        // class RenderWebView
                        // -------------------

RenderWebView::RenderWebView(WebViewDelegate          *delegate,
                             ProfileImpl              *profile,
                             const WebViewProperties&  properties)
    : d_proxy(nullptr)
    , d_delegate(delegate)
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    , d_properties(properties)
#endif
    , d_pendingDestroy(false)
{
}

RenderWebView::~RenderWebView()
{
}

// blpwtk2::WebView overrides:
void RenderWebView::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);

    if (d_proxy) {
        d_proxy->destroy();
    }

    d_pendingDestroy = true;
    d_delegate = nullptr;

    base::MessageLoopCurrent::Get()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *RenderWebView::mainFrame()
{
    return d_proxy->mainFrame();
}

int RenderWebView::loadUrl(const StringRef& url)
{
    return d_proxy->loadUrl(url);
}

#if defined(BLPWTK2_FEATURE_DWM)
void RenderWebView::rootWindowCompositionChanged()
{
    d_proxy->rootWindowCompositionChanged();
}
#endif

void RenderWebView::loadInspector(unsigned int pid, int routingId)
{
    d_proxy->loadInspector(pid, routingId);
}

void RenderWebView::inspectElementAt(const POINT& point)
{
    d_proxy->inspectElementAt(point);
}

#if defined(BLPWTK2_FEATURE_SCREENPRINT)
void RenderWebView::drawContentsToBlob(Blob *blob, const DrawParams& params)
{
    d_proxy->drawContentsToBlob(blob, params);
}
#endif

int RenderWebView::goBack()
{
    return d_proxy->goBack();
}

int RenderWebView::goForward()
{
    return d_proxy->goForward();
}

int RenderWebView::reload()
{
    return d_proxy->reload();
}

void RenderWebView::stop()
{
    d_proxy->stop();
}

#if defined(BLPWTK2_FEATURE_FOCUS)
void RenderWebView::takeKeyboardFocus()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", takeKeyboardFocus";

    // TODO: once the HWND is added, show it instead:
    d_proxy->takeKeyboardFocus();
}

void RenderWebView::setLogicalFocus(bool focused)
{
    // TODO
    d_proxy->setLogicalFocus(focused);
}
#endif

void RenderWebView::show()
{
    DCHECK(Statics::isInApplicationMainThread());

    // TODO: once the HWND is added, show it instead:
    d_proxy->show();
}

void RenderWebView::hide()
{
    DCHECK(Statics::isInApplicationMainThread());

    // TODO: once the HWND is added, hide it instead:
    d_proxy->hide();
}

void RenderWebView::setParent(NativeView parent)
{
    DCHECK(Statics::isInApplicationMainThread());

    // TODO: once the HWND is added, set its parent instead:
    d_proxy->setParent(parent);
}

void RenderWebView::move(int left, int top, int width, int height)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(0 <= width);
    DCHECK(0 <= height);

    // TODO: once the HWND is added, move it instead:
    d_proxy->move(left, top, width, height);
}

void RenderWebView::cutSelection()
{
    d_proxy->cutSelection();
}

void RenderWebView::copySelection()
{
    d_proxy->copySelection();
}

void RenderWebView::paste()
{
    d_proxy->paste();
}

void RenderWebView::deleteSelection()
{
    d_proxy->deleteSelection();
}

void RenderWebView::enableNCHitTest(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
}

void RenderWebView::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
}

void RenderWebView::performCustomContextMenuAction(int actionId)
{
    d_proxy->performCustomContextMenuAction(actionId);
}

void RenderWebView::find(const StringRef& text, bool matchCase, bool forward)
{
    d_proxy->find(text, matchCase, forward);
}

void RenderWebView::stopFind(bool preserveSelection)
{
    d_proxy->stopFind(preserveSelection);
}

void RenderWebView::replaceMisspelledRange(const StringRef& text)
{
    d_proxy->replaceMisspelledRange(text);
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
void RenderWebView::enableAltDragRubberbanding(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
}

bool RenderWebView::forceStartRubberbanding(int x, int y)
{
    return d_proxy->forceStartRubberbanding(x, y);
}

bool RenderWebView::isRubberbanding() const
{
    return d_proxy->isRubberbanding();
}

void RenderWebView::abortRubberbanding()
{
    d_proxy->abortRubberbanding();
}

String RenderWebView::getTextInRubberband(const NativeRect& rect)
{
    return d_proxy->getTextInRubberband(rect);
}
#endif

void RenderWebView::rootWindowPositionChanged()
{
    // TODO:
    d_proxy->rootWindowPositionChanged();
}

void RenderWebView::rootWindowSettingsChanged()
{
    // TODO:
    d_proxy->rootWindowSettingsChanged();
}

void RenderWebView::handleInputEvents(const InputEvent *events, size_t eventsCount)
{
    d_proxy->handleInputEvents(events, eventsCount);
}

void RenderWebView::setDelegate(WebViewDelegate *delegate)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_delegate = delegate;
}

int RenderWebView::getRoutingId() const
{
    return d_proxy->getRoutingId();
}

void RenderWebView::setBackgroundColor(NativeColor color)
{
    d_proxy->setBackgroundColor(color);
}

void RenderWebView::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());
}

#if defined(BLPWTK2_FEATURE_KEYBOARD_LAYOUT)
void RenderWebView::activateKeyboardLayout(unsigned int hkl)
{
    d_proxy->activateKeyboardLayout(hkl);
}
#endif

void RenderWebView::clearTooltip()
{
    d_proxy->clearTooltip();
}

v8::MaybeLocal<v8::Value> RenderWebView::callFunction(
        v8::Local<v8::Function>  func,
        v8::Local<v8::Value>     recv,
        int                      argc,
        v8::Local<v8::Value>    *argv)
{
    return d_proxy->callFunction(func, recv, argc, argv);
}

#if defined(BLPWTK2_FEATURE_PRINTPDF)
String RenderWebView::printToPDF(const StringRef& propertyName)
{
    return d_proxy->printToPDF(propertyName);
}
#endif

#if defined(BLPWTK2_FEATURE_FASTRESIZE)
void RenderWebView::disableResizeOptimization()
{
    d_proxy->disableResizeOptimization();
}
#endif

void RenderWebView::setSecurityToken(v8::Isolate *isolate,
                                     v8::Local<v8::Value> token)
{
    d_proxy->setSecurityToken(isolate, token);
}

// WebViewDelegate overrides
void RenderWebView::created(WebView *source)
{
    d_proxy = source;

    if (d_delegate) {
        d_delegate->created(this);
    }
}

void RenderWebView::didFinishLoad(WebView *source, const StringRef& url)
{
    if (d_delegate) {
        d_delegate->didFinishLoad(this, url);
    }
}

void RenderWebView::didFailLoad(WebView *source, const StringRef& url)
{
    if (d_delegate) {
        d_delegate->didFinishLoad(this, url);
    }
}

void RenderWebView::focused(WebView *source)
{
    NOTREACHED();
}

void RenderWebView::blurred(WebView *source)
{
    NOTREACHED();
}

void RenderWebView::showContextMenu(
        WebView *source, const ContextMenuParams& params)
{
    if (d_delegate) {
        d_delegate->showContextMenu(this, params);
    }
}

void RenderWebView::requestNCHitTest(WebView *source)
{
    NOTREACHED();
}

void RenderWebView::ncDragBegin(WebView      *source,
                                  int           hitTestCode,
                                  const POINT&  startPoint)
{
    NOTREACHED();
}

void RenderWebView::ncDragMove(WebView *source, const POINT& movePoint)
{
    NOTREACHED();
}

void RenderWebView::ncDragEnd(WebView *source, const POINT& endPoint)
{
    NOTREACHED();
}

void RenderWebView::ncDoubleClick(WebView *source, const POINT& point)
{
    NOTREACHED();
}

void RenderWebView::findState(WebView *source,
                                int      numberOfMatches,
                                int      activeMatchOrdinal,
                                bool     finalUpdate)
{
    // The RenderWebView is only used when the embedder lives in another
    // process.  Instead of filtering out all but the latest response in
    // this process, we ship all the responses to the process running the
    // WebViewClientImpl (by using findStateWithReqId) and let it filter out
    // all but the latest response.
    NOTREACHED() << "findState should come in via findStateWithReqId";
}

#if defined(BLPWTK2_FEATURE_DEVTOOLSINTEGRATION)
void RenderWebView::devToolsAgentHostAttached(WebView *source)
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostAttached(this);
    }
}

void RenderWebView::devToolsAgentHostDetached(WebView *source)
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostDetached(this);
    }
}
#endif

}  // close namespace blpwtk2

// vim: ts=4 et


