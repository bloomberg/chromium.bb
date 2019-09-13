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

#include <blpwtk2_webviewproxy.h>

#include <blpwtk2_webviewclient.h>
#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2_profileimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webframeimpl.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_rendererutil.h>

#include <base/message_loop/message_loop.h>
#include <content/renderer/render_view_impl.h>
#include <content/public/renderer/render_frame.h>
#include <content/public/renderer/render_view.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_view.h>

#include <dwmapi.h>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

#define GetAValue(argb)      (LOBYTE((argb)>>24))

namespace blpwtk2 {

                        // ------------------
                        // class WebViewProxy
                        // ------------------

WebViewProxy::WebViewProxy(WebViewDelegate *delegate, ProfileImpl *profile)
    : d_client(nullptr)
    , d_delegate(delegate)
    , d_profile(profile)
    , d_renderViewRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingLoadStatus(false)
    , d_isMainFrameAccessible(false)
    , d_pendingDestroy(false)
{
    d_profile->incrementWebViewCount();
}

WebViewProxy::~WebViewProxy()
{
    LOG(INFO) << "Destroying WebViewProxy, routingId=" << d_renderViewRoutingId;
    d_profile->decrementWebViewCount();

    if (d_client) {
        WebViewClient* client = d_client;
        d_client = nullptr;
        client->releaseHost();
    }
}

void WebViewProxy::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);

    // Schedule a deletion of this WebViewProxy.  The reason we don't delete
    // the object right here right now is because there may be a callback
    // that is already scheduled and the callback requires the existence of
    // the WebView.
    d_pendingDestroy = true;
    d_delegate = nullptr;
    base::MessageLoopCurrent::Get()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *WebViewProxy::mainFrame()
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    if (!d_mainFrame.get()) {
        content::RenderView *rv =
            content::RenderView::FromRoutingID(d_renderViewRoutingId);
        DCHECK(rv);

        blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();
        d_mainFrame.reset(new WebFrameImpl(webFrame));
    }

    return d_mainFrame.get();
}

int WebViewProxy::loadUrl(const StringRef& url)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    d_url = std::string(url.data(), url.length());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", loadUrl=" << d_url;
    d_mainFrame.reset();
    d_client->loadUrl(d_url);
    return 0;
}

void WebViewProxy::loadInspector(unsigned int pid, int routingId)
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", loading inspector for " << routingId;

    d_client->proxy()->loadInspector(pid, routingId);
}

void WebViewProxy::inspectElementAt(const POINT& point)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->inspectElementAt(point.x, point.y);
}

int WebViewProxy::goBack()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goBack()";
    d_mainFrame.reset();
    d_client->goBack();
    return 0;
}

int WebViewProxy::goForward()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goForward()";
    d_mainFrame.reset();
    d_client->goForward();
    return 0;
}

int WebViewProxy::reload()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", reload()";
    d_mainFrame.reset();
    d_client->reload();
    return 0;
}

void WebViewProxy::stop()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", stop";
    d_client->proxy()->stop();
}

void WebViewProxy::show()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", show";
    d_client->proxy()->show();
}

void WebViewProxy::hide()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", hide";
    d_client->proxy()->hide();
}

int WebViewProxy::setParent(NativeView parent)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->setParent(parent);
    return 0;
}

void WebViewProxy::move(int left, int top, int width, int height)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->move(gfx::Rect(left, top, width, height));
}

void WebViewProxy::cutSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->cutSelection();
}

void WebViewProxy::copySelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->copySelection();
}

void WebViewProxy::paste()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->paste();
}

void WebViewProxy::deleteSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->deleteSelection();
}

void WebViewProxy::enableNCHitTest(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->enableNCHitTest(enabled);
}

void WebViewProxy::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->ncHitTestResult(x, y, result);
}

void WebViewProxy::performCustomContextMenuAction(int actionId)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->performCustomContextMenuAction(actionId);
}

void WebViewProxy::find(const StringRef& text, bool matchCase, bool forward)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->find(std::string(text.data(), text.size()), matchCase, forward);
}

void WebViewProxy::stopFind(bool preserveSelection)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->stopFind(preserveSelection);
}

void WebViewProxy::replaceMisspelledRange(const StringRef& text)
{
    DCHECK(Statics::isInApplicationMainThread());
    std::string stext(text.data(), text.length());
    d_client->proxy()->replaceMisspelledRange(stext);
}

void WebViewProxy::rootWindowPositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowPositionChanged();
}

void WebViewProxy::rootWindowSettingsChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowSettingsChanged();
}

void WebViewProxy::handleInputEvents(const InputEvent *events, size_t eventsCount)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    content::RenderViewImpl *rw =
        content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    DCHECK(rw);

    RendererUtil::handleInputEvents(rw->GetWidget(), events, eventsCount);
}

void WebViewProxy::setDelegate(WebViewDelegate *delegate)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_delegate = delegate;
}

int WebViewProxy::getRoutingId() const
{
    return d_renderViewRoutingId;
}

void WebViewProxy::setBackgroundColor(NativeColor color)
{
    int red = GetRValue(color);
    int green = GetGValue(color);
    int blue = GetBValue(color);
    int alpha = GetAValue(color);

    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible) << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    d_client->proxy()->setBackgroundColor(red, green, blue);

    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebView* web_view = rv->GetWebView();
    web_view->SetBaseBackgroundColor(
        SkColorSetARGB(alpha, red, green, blue));
}

void WebViewProxy::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->applyRegion(region);
}

void WebViewProxy::clearTooltip()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->clearTooltip();
}

v8::MaybeLocal<v8::Value> WebViewProxy::callFunction(
        v8::Local<v8::Function>  func,
        v8::Local<v8::Value>     recv,
        int                      argc,
        v8::Local<v8::Value>    *argv)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    content::RenderView *rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();
    DCHECK(webFrame->IsWebLocalFrame());
    blink::WebLocalFrame* localWebFrame = webFrame->ToWebLocalFrame();

    return localWebFrame->CallFunctionEvenIfScriptDisabled(func, recv, argc, argv);
}

void WebViewProxy::setSecurityToken(v8::Isolate *isolate,
                                    v8::Local<v8::Value> token)
{
    d_securityToken.Reset(isolate, token);
}

// blpwtk2::WebViewClientDelegate overrides
void WebViewProxy::setClient(WebViewClient *client)
{
    d_client = client;
}

void WebViewProxy::ncHitTest()
{
    if (d_delegate) {
        d_delegate->requestNCHitTest(this);
        // Note: The embedder is expected to call WebView::onNCHitTestResult
    }
    else {
        onNCHitTestResult(0, 0, HTNOWHERE);
    }
}

void WebViewProxy::ncDragBegin(int hitTestCode, const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDragBegin(this, hitTestCode, winPoint);
    }
}

void WebViewProxy::ncDragMove(const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDragMove(this, winPoint);
    }
}

void WebViewProxy::ncDragEnd(const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDragEnd(this, winPoint);
    }
}

void WebViewProxy::ncDoubleClick(const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDoubleClick(this, winPoint);
    }
}

void WebViewProxy::focused()
{
    if (d_delegate) {
        d_delegate->focused(this);
    }
}

void WebViewProxy::blurred()
{
    if (d_delegate) {
        d_delegate->blurred(this);
    }
}

void WebViewProxy::showContextMenu(const ContextMenuParams& params)
{
    if (d_delegate) {
        d_delegate->showContextMenu(this, params);
    }
}

void WebViewProxy::findReply(int  numberOfMatches,
                             int  activeMatchOrdinal,
                             bool finalUpdate)
{
    if (d_delegate) {
        d_delegate->findState(
                this, numberOfMatches, activeMatchOrdinal, finalUpdate);
    }
}

void WebViewProxy::preResize(const gfx::Size& size)
{
}

void WebViewProxy::notifyRoutingId(int id)
{
    if (d_pendingDestroy) {
        LOG(INFO) << "WebView destroyed before we got a reference to a RenderView";
        return;
    }

    content::RenderView *rv =
        content::RenderView::FromRoutingID(id);

    if (!rv) {
        // The RenderView has not been created yet.  Keep reposting this task
        // until the RenderView is available.
        base::MessageLoopCurrent::Get()->task_runner()->PostTask(
            FROM_HERE,
            base::Bind(&WebViewProxy::notifyRoutingId,
                       base::Unretained(this),
                       id));
        return;
    }

    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;
}

void WebViewProxy::onLoadStatus(int status)
{
    d_pendingLoadStatus = false;

    if (0 == status) {
        LOG(INFO) << "routingId=" << d_renderViewRoutingId
                  << ", didFinishLoad url=" << d_url;

        if (!d_securityToken.IsEmpty()) {
            content::RenderView *rv =
                content::RenderView::FromRoutingID(d_renderViewRoutingId);
            DCHECK(rv);

            blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();

            if (webFrame && webFrame->IsWebLocalFrame()) {
                v8::Isolate *isolate = webFrame->ScriptIsolate();

                v8::HandleScope hs(isolate);
                webFrame->ToWebLocalFrame()->
                    MainWorldScriptContext()->
                        SetSecurityToken(d_securityToken.Get(isolate));
            }
        }

        // wait until we receive this
        // notification before we make the
        // mainFrame accessible
        d_isMainFrameAccessible = true;

        if (d_delegate) {
            d_delegate->didFinishLoad(this, StringRef(d_url));
        }
    }
    else {
        LOG(INFO) << "routingId=" << d_renderViewRoutingId
                  << ", didFailLoad url=" << d_url;

        if (d_delegate) {
            d_delegate->didFailLoad(this, StringRef(d_url));
        }
    }
}

void WebViewProxy::didFinishLoadForFrame(int              routingId,
                                         const StringRef& url)
{
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", frame routingId=" << routingId
              << ", didFinishLoadForFrame url=" << d_url;

    if (d_securityToken.IsEmpty()) {
        return;
    }

    content::RenderFrame *rf = content::RenderFrame::FromRoutingID(routingId);

    if (!rf) {
        return;
    }

    blink::WebLocalFrame *webFrame = rf->GetWebFrame();

    if (!webFrame) {
        return;
    }

    // Ensure that d_mainFrame is populated:
    mainFrame();

    // Propagate V8 security token:
    v8::Isolate *isolate = webFrame->ScriptIsolate();
    v8::HandleScope hs(isolate);

    webFrame->MainWorldScriptContext()->
        SetSecurityToken(d_securityToken.Get(isolate));

    // Propagate content settings overrides:
    webFrame->SetContentSettingsClient(d_mainFrame.get());
}

void WebViewProxy::didFailLoadForFrame(int              routingId,
                                       const StringRef& url)
{
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", frame routingId=" << routingId
              << ", didFailLoadForFrame url=" << d_url;
}

void WebViewProxy::didParentStatus(int status, NativeView parent)
{
    if (d_delegate) {
        d_delegate->didParentStatus(this, status, parent);
    }
}

}  // close namespace blpwtk2

// vim: ts=4 et


