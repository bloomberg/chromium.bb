/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
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

#include <blpwtk2_webviewhostimpl.h>
#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2_string.h>
#include <blpwtk2_webviewimpl.h>

#include <ipc/ipc_message_macros.h>
#include <windows.h>
#include <errno.h>

namespace blpwtk2 {

                        // ---------------------
                        // class WebViewHostImpl
                        // ---------------------

WebViewHostImpl::WebViewHostImpl(
        mojom::WebViewClientPtr&&                    clientPtr,
        const mojom::WebViewCreateParams&            params,
        BrowserContextImpl                          *browserContext,
        unsigned int                                 hostAffinity,
        const scoped_refptr<ProcessHostImpl::Impl>&  processHost)
    : d_clientPtr(std::move(clientPtr))
    , d_dragState({})
    , d_processHost(processHost)
{
    WebViewProperties properties;

    properties.domPasteEnabled =
        params.domPasteEnabled;
    properties.javascriptCanAccessClipboard =
        params.javascriptCanAccessClipboard;
    properties.rerouteMouseWheelToAnyRelatedWindow =
        params.rerouteMouseWheelToAnyRelatedWindow;

    d_impl = new WebViewImpl(this,              // delegate
                             0,                 // parent window
                             browserContext,    // browser context
                             hostAffinity,      // host affinity
                             false,             // initially visible
                             properties);       // properties

    d_impl->setImplClient(this);
}

WebViewHostImpl::~WebViewHostImpl()
{
    // If the webview client is waiting for a url resource to load, notify it
    // of its cancellation.
    if (d_loadUrlCallback) {
        std::move(d_loadUrlCallback).Run(ECANCELED);
        d_loadUrlCallback.Reset();
    }

    // Destroy the WebViewImpl
    if (d_impl) {
        WebView *impl = d_impl;
        d_impl = nullptr;

        impl->destroy();
    }

    // Disconnect from the WebViewClient
    d_clientPtr.reset();
}

// blpwtk2::WebViewImplClient overrides
void WebViewHostImpl::updateNativeViews(blpwtk2::NativeView webview,
                                        blpwtk2::NativeView hiddenView)
{
    d_clientPtr->notifyNativeViews(reinterpret_cast<int>(webview),
                                   reinterpret_cast<int>(hiddenView));
}

void WebViewHostImpl::gotNewRenderViewRoutingId(int renderViewRoutingId)
{
    d_clientPtr->notifyRoutingId(renderViewRoutingId);
}

void WebViewHostImpl::findStateWithReqId(int  reqId,
                                         int  numberOfMatches,
                                         int  activeMatchOrdinal,
                                         bool finalUpdate)
{
    d_clientPtr->findReply(
            reqId, numberOfMatches, activeMatchOrdinal, finalUpdate);
}

void WebViewHostImpl::didFinishLoadForFrame(int              routingId,
                                            const StringRef& url)
{
    d_clientPtr->didFinishLoadForFrame(routingId, url.toStdString());
}

void WebViewHostImpl::didFailLoadForFrame(int              routingId,
                                          const StringRef& url)
{
    d_clientPtr->didFailLoadForFrame(routingId, url.toStdString());
}

// blpwtk2::WebViewDelegate overrides
void WebViewHostImpl::created(WebView *source)
{
    DCHECK(source == d_impl);
}

void WebViewHostImpl::didFinishLoad(WebView *source, const StringRef& url)
{
    if (!d_loadUrlCallback) {
        return;
    }
    std::move(d_loadUrlCallback).Run(0);
    d_loadUrlCallback.Reset();
}

void WebViewHostImpl::didFailLoad(WebView *source, const StringRef& url)
{
    if (!d_loadUrlCallback) {
        return;
    }
    std::move(d_loadUrlCallback).Run(EFAULT);
    d_loadUrlCallback.Reset();
}

void WebViewHostImpl::focused(WebView *source)
{
    DCHECK(source == d_impl);
    d_clientPtr->focused();
}

void WebViewHostImpl::blurred(WebView *source)
{
    DCHECK(source == d_impl);
    d_clientPtr->blurred();
}

void WebViewHostImpl::showContextMenu(
        WebView *source, const ContextMenuParams& params)
{
    DCHECK(source == d_impl);
    d_clientPtr->showContextMenu(getContextMenuParamsImpl(params)->Clone());
}

void WebViewHostImpl::requestNCHitTest(WebView *source)
{
    DCHECK(source == d_impl);
    d_clientPtr->ncHitTest(
            base::Bind(&WebViewImpl::onNCHitTestResult,
                       base::Unretained(d_impl)));
}

void WebViewHostImpl::ncDragBegin(WebView      *source,
                                  int           hitTestCode,
                                  const POINT&  startPoint)
{
    DCHECK(source == d_impl);
    d_dragState.isDragging = true;

    if (d_dragState.pendingAck) {
        d_dragState.startPoint = startPoint;
        d_dragState.startPointSet = true;
        return;
    }

    d_dragState.hitTestCode = hitTestCode;
    d_dragState.pendingAck = true;
    d_clientPtr->ncDragBegin(
            hitTestCode,
            startPoint.x,
            startPoint.y,
            base::Bind(&WebViewHostImpl::onNCDragAck,
                       base::Unretained(this)));
}

void WebViewHostImpl::ncDragMove(WebView *source, const POINT& movePoint)
{
    DCHECK(source == d_impl);

    if (d_dragState.pendingAck && HTCAPTION != d_dragState.hitTestCode) {
        // If we are dragging the caption, then we want the drag to be as
        // smooth as possible, so we will sent continuous ncDragMove
        // notifications.  However, other drag types would cause resizes to
        // happen, which would be slow.  In those case, we don't want to send
        // a continuous stream of ncDragMove notifications, so we will need
        // an ack from the WebViewProxy for each move.
        d_dragState.movePoint = movePoint;
        d_dragState.movePointSet = true;
        return;
    }

    d_dragState.pendingAck = true;
    d_clientPtr->ncDragMove(
            movePoint.x,
            movePoint.y,
            base::Bind(&WebViewHostImpl::onNCDragAck,
                       base::Unretained(this)));
}

void WebViewHostImpl::ncDragEnd(WebView *source, const POINT& endPoint)
{
    DCHECK(source == d_impl);
    d_dragState.isDragging = false;

    if (d_dragState.pendingAck) {
        d_dragState.endPoint = endPoint;
        d_dragState.endPointSet = true;
        d_dragState.movePointSet = false;
        return;
    }

    d_dragState.pendingAck = true;
    d_clientPtr->ncDragEnd(
            endPoint.x,
            endPoint.y,
            base::Bind(&WebViewHostImpl::onNCDragAck,
                       base::Unretained(this)));
}

void WebViewHostImpl::findState(WebView *source,
                                int      numberOfMatches,
                                int      activeMatchOrdinal,
                                bool     finalUpdate)
{
    // The WebViewHostImpl is only used when the embedder lives in another
    // process.  Instead of filtering out all but the latest response in
    // this process, we ship all the responses to the process running the
    // WebViewClientImpl (by using findStateWithReqId) and let it filter out
    // all but the latest response.
    NOTREACHED() << "findState should come in via findStateWithReqId";
}

// Mojo callbacks
void WebViewHostImpl::onNCDragAck()
{
    enum Operation {
        BEGIN,
        MOVE,
        END
    };

    Operation operations[3];

    if (d_dragState.isDragging) {
        operations[0] = END;
        operations[1] = BEGIN;
        operations[2] = MOVE;
    }
    else {
        operations[0] = BEGIN;
        operations[1] = MOVE;
        operations[2] = END;
    }

    d_dragState.pendingAck = false;

    for (size_t i=0; i<(sizeof(operations)/sizeof(Operation)); ++i) {
        if (BEGIN == operations[i]) {
            if (d_dragState.startPointSet) {
                d_dragState.startPointSet = false;
                d_dragState.pendingAck = true;
                d_clientPtr->ncDragBegin(
                        d_dragState.hitTestCode,
                        d_dragState.startPoint.x(),
                        d_dragState.startPoint.y(),
                        base::Bind(&WebViewHostImpl::onNCDragAck,
                                   base::Unretained(this)));
                break;
            }
        }
        else if (MOVE == operations[i]) {
            if (d_dragState.movePointSet) {
                d_dragState.movePointSet = false;
                d_dragState.pendingAck = true;
                d_clientPtr->ncDragMove(
                        d_dragState.movePoint.x(),
                        d_dragState.movePoint.y(),
                        base::Bind(&WebViewHostImpl::onNCDragAck,
                                   base::Unretained(this)));
                break;
            }
        }
        else if (END == operations[i]) {
            if (d_dragState.endPointSet) {
                d_dragState.endPointSet = false;
                d_dragState.pendingAck = true;
                d_clientPtr->ncDragEnd(
                        d_dragState.endPoint.x(),
                        d_dragState.endPoint.y(),
                        base::Bind(&WebViewHostImpl::onNCDragAck,
                                   base::Unretained(this)));
                break;
            }
        }
    }
}

// mojom::WebViewHost overrides
void WebViewHostImpl::loadUrl(
        const std::string& url, loadUrlCallback callback)
{
    if (!d_loadUrlCallback) {
        d_impl->loadUrl(url);
        d_loadUrlCallback = std::move(callback);
    }
    else {
        std::move(callback).Run(EBUSY);
    }
}

static void onInspectorLoad(int status)
{
    if (0 == status) {
        LOG(INFO) << "DevTools loaded successfully";
    }
    else {
        LOG(ERROR) << "DevTools failed to load with error " << status;
    }
}

void WebViewHostImpl::loadInspector(unsigned int pid, int routingId)
{
    if (!d_loadUrlCallback) {
        d_impl->loadInspector(pid, routingId);
        d_loadUrlCallback = base::Bind(&onInspectorLoad);
    }
}

void WebViewHostImpl::inspectElementAt(int x, int y)
{
    POINT point = {x, y};
    d_impl->inspectElementAt(point);
}

void WebViewHostImpl::back(backCallback callback)
{
    if (!d_loadUrlCallback) {
        int rc = d_impl->goBack();
        if (0 == rc) {
            d_loadUrlCallback = std::move(callback);
        }
        else {
            std::move(callback).Run(rc);
        }
    }
    else {
        std::move(callback).Run(EBUSY);
    }
}

void WebViewHostImpl::forward(forwardCallback callback)
{
    if (!d_loadUrlCallback) {
        int rc = d_impl->goForward();
        if (0 == rc) {
            d_loadUrlCallback = std::move(callback);
        }
        else {
            std::move(callback).Run(rc);
        }
    }
    else {
        std::move(callback).Run(EBUSY);
    }
}

void WebViewHostImpl::reload(reloadCallback callback)
{
    if (!d_loadUrlCallback) {
        d_impl->reload();
        d_loadUrlCallback = std::move(callback);
    }
    else {
        std::move(callback).Run(EBUSY);
    }
}

void WebViewHostImpl::stop()
{
    d_impl->stop();
}

void WebViewHostImpl::performCustomContextMenuAction(int id)
{
    d_impl->performCustomContextMenuAction(id);
}

void WebViewHostImpl::show()
{
    d_impl->show();
}

void WebViewHostImpl::hide()
{
    d_impl->hide();
}

void WebViewHostImpl::move(int                 x,
                           int                 y,
                           int                 width,
                           int                 height,
                           moveCallback callback)
{
    d_impl->move(x, y, width, height);
    std::move(callback).Run(x, y, width, height);
}

void WebViewHostImpl::cutSelection()
{
    d_impl->cutSelection();
}

void WebViewHostImpl::copySelection()
{
    d_impl->copySelection();
}

void WebViewHostImpl::paste()
{
    d_impl->paste();
}

void WebViewHostImpl::deleteSelection()
{
    d_impl->deleteSelection();
}

void WebViewHostImpl::enableNCHitTest(bool enabled)
{
    d_impl->enableNCHitTest(enabled);
}

void WebViewHostImpl::find(int                reqId,
                           const std::string& text,
                           bool               matchCase,
                           bool               findNext,
                           bool               forward)
{
    FindOnPageRequest request;
    request.reqId = reqId;
    request.text = text;
    request.matchCase = matchCase;
    request.findNext = findNext;
    request.forward = forward;

    d_impl->handleFindRequest(request);
}

void WebViewHostImpl::stopFind(bool preserveSelection)
{
    d_impl->stopFind(preserveSelection);
}

void WebViewHostImpl::replaceMisspelledRange(const std::string& text)
{
    d_impl->replaceMisspelledRange(text);
}

void WebViewHostImpl::rootWindowPositionChanged()
{
    d_impl->rootWindowPositionChanged();
}

void WebViewHostImpl::rootWindowSettingsChanged()
{
    d_impl->rootWindowSettingsChanged();
}

void WebViewHostImpl::setBackgroundColor(int r, int g, int b)
{
    d_impl->setBackgroundColor(RGB(r, g, b));
}

void WebViewHostImpl::applyRegion(
        const std::string& blob, applyRegionCallback callback)
{
    HRGN region = 0;

    if (blob.size() > 0) {
        region = ::ExtCreateRegion(
                nullptr,
                blob.size(),
                reinterpret_cast<const RGNDATA* >(blob.data()));
        DCHECK(region);
    }

    d_impl->setRegion(region);
    std::move(callback).Run();
}

void WebViewHostImpl::clearTooltip()
{
    d_impl->clearTooltip();
}

void WebViewHostImpl::setParent(unsigned int window, setParentCallback callback)
{
    DWORD status = d_impl->setParent(reinterpret_cast<NativeView>(window));
    std::move(callback).Run(status, window);
}

}  // close namespace blpwtk2

// vim: ts=4 et

