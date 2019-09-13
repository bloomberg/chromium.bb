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

#include <blpwtk2_webviewclientimpl.h>
#include <blpwtk2_webviewclientdelegate.h>

#include <blpwtk2_string.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_contextmenuparams.h>

#include <ipc/ipc_message_macros.h>
#include <windows.h>

namespace blpwtk2 {

                        // -----------------------
                        // class WebViewClientImpl
                        // -----------------------

WebViewClientImpl::WebViewClientImpl(mojom::WebViewHostPtr&&  hostPtr,
                                     WebViewClientDelegate   *delegate)
    : d_hostPtr(std::move(hostPtr))
    , d_delegate(delegate)
    , d_nativeView(0)
    , d_originalParentView(0)
    , d_pendingMoveAck(false)
{
    DCHECK(d_hostPtr);
}

WebViewClientImpl::~WebViewClientImpl()
{
    releaseHost();
}

// blpwtk2::WebViewClient overrides
void WebViewClientImpl::releaseHost()
{
    if (d_nativeView) {
        ::SetParent(d_nativeView, d_originalParentView);
    }

    if (d_delegate) {
        WebViewClientDelegate* delegate = d_delegate;
        d_delegate = nullptr;

        delegate->setClient(nullptr);
    }

    d_hostPtr.reset();
}

mojom::WebViewHost *WebViewClientImpl::proxy()
{
    return d_hostPtr.get();
}

void WebViewClientImpl::goBack()
{
    DCHECK(d_hostPtr);
    d_hostPtr->back(base::Bind(&WebViewClientImpl::loadStatus,
                               base::Unretained(this)));
}

void WebViewClientImpl::goForward()
{
    DCHECK(d_hostPtr);
    d_hostPtr->forward(base::Bind(&WebViewClientImpl::loadStatus,
                                  base::Unretained(this)));
}

void WebViewClientImpl::reload()
{
    DCHECK(d_hostPtr);
    d_hostPtr->reload(base::Bind(&WebViewClientImpl::loadStatus,
                                 base::Unretained(this)));
}

void WebViewClientImpl::loadUrl(const std::string& url)
{
    DCHECK(d_hostPtr);
    d_hostPtr->loadUrl(url, base::Bind(&WebViewClientImpl::loadStatus,
                                       base::Unretained(this)));
}

void WebViewClientImpl::move(const gfx::Rect& rect)
{
    d_rect = rect;

    if (!d_pendingMoveAck) {
        d_pendingMoveAck = true;
        d_delegate->preResize(d_rect.size());
        DCHECK(d_hostPtr);
        d_hostPtr->move(d_rect.x(),
                        d_rect.y(),
                        d_rect.width(),
                        d_rect.height(),
                        base::Bind(&WebViewClientImpl::moveAck,
                                   base::Unretained(this)));
    }
}

static void deleteRegion(NativeRegion region)
{
    // After a call to ::SetWindowRgn(), the system owns the region. Since this
    // region has been copied, it should be deleted here:
    if (region) {
        ::DeleteObject(region);
    }
}

void WebViewClientImpl::applyRegion(NativeRegion region)
{
    // Blobify the region:
    std::string regionBlob;

    if (region) {
        DWORD blobSize = ::GetRegionData(region, 0, nullptr);
        regionBlob.resize(blobSize);
        char *addr = const_cast<char*>(regionBlob.data());
        ::GetRegionData(region, blobSize, reinterpret_cast<LPRGNDATA>(addr));
    }

    DCHECK(d_hostPtr);
    d_hostPtr->applyRegion(regionBlob,
                           base::Bind(&deleteRegion, region));
}

void WebViewClientImpl::ncHitTestResult(int x, int y, int result)
{
    if (d_ncHitTestCallback) {
        std::move(d_ncHitTestCallback).Run(x, y, result);
    }
}

void WebViewClientImpl::setParentStatusUpdate(int status, unsigned int parent)
{
    DCHECK(d_delegate);

    if (d_delegate) {
       d_delegate->didParentStatus(status,  reinterpret_cast<NativeView>(parent));
    }
}

void WebViewClientImpl::setParent(NativeView parent)
{
    LOG(INFO) << "setParent=" << (void*)parent;

    if (d_nativeView) {
        NativeView parentView = parent ? parent : d_originalParentView;
        int status = 0;
        if (!::SetParent(d_nativeView, parentView)) {
            status = ::GetLastError();
            LOG(ERROR) << "WebViewClientImpl::setParent failed: hwnd =" << d_nativeView << ", parent = " << (void*)parentView << ", status = " << status;
        }
        setParentStatusUpdate(status, reinterpret_cast<unsigned int>(parentView));
    }
    else {
        DCHECK(d_hostPtr);
        d_hostPtr->setParent(reinterpret_cast<unsigned int>(parent), base::Bind(&WebViewClientImpl::setParentStatusUpdate, base::Unretained(this)));
    }
}

void WebViewClientImpl::find(
        const std::string& text, bool matchCase, bool forward)
{
    if (!d_findState) {
        d_findState.reset(new FindOnPage());
    }

    FindOnPageRequest request =
        d_findState->makeRequest(text, matchCase, forward);

    DCHECK(d_hostPtr);
    d_hostPtr->find(request.reqId,
                    request.text,
                    request.matchCase,
                    request.findNext,
                    request.forward);
}

void WebViewClientImpl::stopFind(bool preserveSelection)
{
    d_findState.reset(new FindOnPage());
    DCHECK(d_hostPtr);
    d_hostPtr->stopFind(preserveSelection);
}

// mojom::WebViewClient overrides
void WebViewClientImpl::ncHitTest(ncHitTestCallback callback)
{
    DCHECK(d_delegate);
    DCHECK(!d_ncHitTestCallback);

    if (d_delegate) {
        d_ncHitTestCallback = std::move(callback);
        d_delegate->ncHitTest();
        // The proxy is expected to call WebViewClientImpl::ncHitTestResult
    }
    else {
        std::move(callback).Run(0, 0, 0);
    }
}

void WebViewClientImpl::ncDragBegin(
        int hitTestCode, int x, int y, ncDragBeginCallback callback)
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->ncDragBegin(hitTestCode, gfx::Point(x, y));
    }

    std::move(callback).Run();
}

void WebViewClientImpl::ncDragMove(
        int x, int y, ncDragMoveCallback callback)
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->ncDragMove(gfx::Point(x, y));
    }

    std::move(callback).Run();
}

void WebViewClientImpl::ncDragEnd(
        int x, int y, ncDragEndCallback callback)
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->ncDragEnd(gfx::Point(x, y));
    }

    std::move(callback).Run();
}

void WebViewClientImpl::ncDoubleClick(int x, int y)
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->ncDoubleClick(gfx::Point(x, y));
    }
}

void WebViewClientImpl::focused()
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->focused();
    }
}

void WebViewClientImpl::blurred()
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->blurred();
    }
}

void WebViewClientImpl::showContextMenu(mojom::ContextMenuParamsPtr params)
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->showContextMenu(ContextMenuParams(params.get()));
    }
}

void WebViewClientImpl::notifyNativeViews(int nativeView, int originalParent)
{
    d_nativeView = reinterpret_cast<NativeView>(nativeView);
    d_originalParentView = reinterpret_cast<NativeView>(originalParent);
}

void WebViewClientImpl::notifyRoutingId(int id)
{
    DCHECK(d_delegate);
    if (d_delegate) {
        d_delegate->notifyRoutingId(id);
    }
}

void WebViewClientImpl::findReply(int  reqId,
                                  int  numberOfMatches,
                                  int  activeMatchOrdinal,
                                  bool finalUpdate)
{
    DCHECK(d_findState);
    if (d_delegate &&
        d_findState->applyUpdate(reqId, numberOfMatches, activeMatchOrdinal)) {
        d_delegate->findReply(d_findState->numberOfMatches(),
                              d_findState->activeMatchIndex(),
                              finalUpdate);
    }
}

void WebViewClientImpl::didFinishLoadForFrame(int   routingId,
                                              const std::string& url)
{
    if (d_delegate) {
        d_delegate->didFinishLoadForFrame(routingId, StringRef(url));
    }
}

void WebViewClientImpl::didFailLoadForFrame(int   routingId,
                                            const std::string& url)
{
    if (d_delegate) {
        d_delegate->didFailLoadForFrame(routingId, StringRef(url));
    }
}

// Mojo callbacks
void WebViewClientImpl::loadStatus(int status)
{
    DCHECK(d_delegate);

    if (d_delegate) {
        d_delegate->onLoadStatus(status);
    }
}

void WebViewClientImpl::moveAck(int x, int y, int w, int h)
{
    DCHECK(d_pendingMoveAck);

    gfx::Rect rect(x, y, w, h);
    if (d_delegate) {
        d_delegate->preResize(rect.size());
    }

    if (d_rect != rect) {
        DCHECK(d_hostPtr);
        d_hostPtr->move(d_rect.x(),
                        d_rect.y(),
                        d_rect.width(),
                        d_rect.height(),
                        base::Bind(&WebViewClientImpl::moveAck,
                                   base::Unretained(this)));
    }
    else {
        d_pendingMoveAck = false;
    }
}



}  // close namespace blpwtk2

// vim: ts=4 et

