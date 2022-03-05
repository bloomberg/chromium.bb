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
#include <blpwtk2_webviewproxydelegate.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_rendererutil.h>

#include <base/task/single_thread_task_executor.h>
#include <content/renderer/render_frame_impl.h>
#include <content/renderer/render_view_impl.h>
#include <content/public/renderer/render_frame.h>
#include <content/public/renderer/render_view.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_frame_widget.h>
#include <third_party/blink/public/web/web_view.h>

#include <dwmapi.h>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

#define RET_VOID
#define VALIDATE_RENDER_VIEW_IMPL(rv, ret)                                     \
  if (!rv) {                                                                   \
    LOG(WARNING)                                                               \
        << "nullptr is returned from content::RenderViewImpl::FromRoutingID with " \
           "d_renderViewRoutingId = "                                          \
        << d_renderViewRoutingId;                                              \
    return ret;                                                                \
  }

#define VALIDATE_RENDER_VIEW_VOID(rv) VALIDATE_RENDER_VIEW_IMPL(rv, RET_VOID)
#define VALIDATE_RENDER_VIEW(rv) VALIDATE_RENDER_VIEW_IMPL(rv, {})

namespace blpwtk2 {

                        // ------------------
                        // class WebViewProxy
                        // ------------------

WebViewProxy::WebViewProxy(WebViewDelegate *delegate, ProfileImpl *profile)
    : d_client(nullptr)
    , d_delegate(delegate)
    , d_proxyDelegate(nullptr)
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

void WebViewProxy::setProxyDelegate(WebViewProxyDelegate *proxyDelegate)
{
    d_proxyDelegate = proxyDelegate;
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
    d_proxyDelegate = nullptr;
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
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
            content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
        VALIDATE_RENDER_VIEW(rv);

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
    if (!validateClient()) {
        return EFAULT;
    }

    d_pendingLoadStatus = true;
    d_url = std::string(url.data(), url.length());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", loadUrl=" << d_url;
    d_mainFrame.reset();
    d_client->loadUrl(d_url);
    return 0;
}

void WebViewProxy::rootWindowCompositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowCompositionChanged();
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
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->inspectElementAt(point.x, point.y);
}

void WebViewProxy::drawContentsToBlob(Blob *blob, const DrawParams& params)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);
    DCHECK(blob);

    content::RenderView *rv =
        content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);

    VALIDATE_RENDER_VIEW_VOID(rv);
    RendererUtil::drawContentsToBlob(rv, blob, params);
}

int WebViewProxy::goBack()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }
    if (!validateClient()) {
        return EFAULT;
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
    if (!validateClient()) {
        return EFAULT;
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
    if (!validateClient()) {
        return EFAULT;
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
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->stop();
}

void WebViewProxy::takeKeyboardFocus()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->takeKeyboardFocus();
}

void WebViewProxy::setLogicalFocus(bool focused)
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", setLogicalFocus " << (focused ? "true" : "false");

    if (d_gotRenderViewInfo) {
        // If we have the renderer in-process, then set the logical focus
        // immediately so that handleInputEvents will work as expected.
        content::RenderViewImpl *rv =
            content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
        VALIDATE_RENDER_VIEW_VOID(rv);
        blink::WebView* webView = rv->GetWebView();
        webView->SetPageFocus(focused);
    }

    // Send the message, which will update the browser-side aura::Window focus
    // state.
    d_client->proxy()->setLogicalFocus(focused); 
}

void WebViewProxy::show()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", show";
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->show();
}

void WebViewProxy::hide()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", hide";
    if (!validateClient()) {
        return;
    }
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
    if (!validateClient()) {
        return;
    }
    d_client->move(gfx::Rect(left, top, width, height));
}

void WebViewProxy::cutSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->cutSelection();
}

void WebViewProxy::copySelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->copySelection();
}

void WebViewProxy::paste()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->paste();
}

void WebViewProxy::deleteSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->deleteSelection();
}

void WebViewProxy::enableNCHitTest(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->enableNCHitTest(enabled);
}

void WebViewProxy::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->onNCHitTestResult(x, y, result);
}


void WebViewProxy::onEnterFullscreenModeResult(bool isFullscreen)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->onEnterFullscreenModeResult(isFullscreen);
}

void WebViewProxy::setNCHitTestRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->applyNCHitTestRegion(region);
}

void WebViewProxy::performCustomContextMenuAction(int actionId)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->performCustomContextMenuAction(actionId);
}

void WebViewProxy::find(const StringRef& text, bool matchCase, bool forward)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->find(std::string(text.data(), text.size()), matchCase, forward);
}

void WebViewProxy::enableAltDragRubberbanding(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->enableAltDragRubberbanding(enabled);
}

bool WebViewProxy::forceStartRubberbanding(int x, int y)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    VALIDATE_RENDER_VIEW(rv);
    blink::WebView* webView = rv->GetWebView();
    return webView->ForceStartRubberbanding(x, y);
}

bool WebViewProxy::isRubberbanding() const
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    VALIDATE_RENDER_VIEW(rv);
    blink::WebView* webView = rv->GetWebView();
    return webView->IsRubberbanding();
}

void WebViewProxy::abortRubberbanding()
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    VALIDATE_RENDER_VIEW_VOID(rv);
    blink::WebView* webView = rv->GetWebView();
    webView->AbortRubberbanding();
}

String WebViewProxy::getTextInRubberband(const NativeRect& rect)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    VALIDATE_RENDER_VIEW(rv);
    blink::WebView* webView = rv->GetWebView();
    gfx::Rect gfxrect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    std::string str = webView->GetTextInRubberband(gfxrect).Utf8();
    return String(str.data(), str.size());
}

void WebViewProxy::stopFind(bool preserveSelection)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->stopFind(preserveSelection);
}

void WebViewProxy::replaceMisspelledRange(const StringRef& text)
{
    DCHECK(Statics::isInApplicationMainThread());
    std::string stext(text.data(), text.length());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->replaceMisspelledRange(stext);
}

void WebViewProxy::rootWindowPositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->rootWindowPositionChanged();
}

void WebViewProxy::rootWindowSettingsChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->proxy()->rootWindowSettingsChanged();
}

void WebViewProxy::handleInputEvents(const InputEvent *events, size_t eventsCount)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    content::RenderView *rv =
        content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    DCHECK(rv);

    blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();
    DCHECK(webFrame->IsWebLocalFrame());
    
    blink::WebLocalFrame* localWebFrame = webFrame->ToWebLocalFrame();
    content::RenderFrameImpl* render_frame =
        content::RenderFrameImpl::FromWebFrame(localWebFrame);
    blink::WebFrameWidget* web_widget = render_frame->GetLocalRootWebFrameWidget();
    
    RendererUtil::handleInputEvents(web_widget, events, eventsCount);
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

    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible) << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    if (!validateClient()) {
        return;
    }
    d_client->proxy()->setBackgroundColor(red, green, blue);
}

void WebViewProxy::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
    d_client->applyRegion(region);
}

void WebViewProxy::clearTooltip()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (!validateClient()) {
        return;
    }
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

    content::RenderView *rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    VALIDATE_RENDER_VIEW(rv);
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

String WebViewProxy::printToPDF()
{
    content::RenderView *rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    VALIDATE_RENDER_VIEW(rv);
    return RendererUtil::printToPDF(rv);
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

void WebViewProxy::enterFullscreenMode()
{
    if (d_delegate) {
        d_delegate->enterFullscreenMode(this);
    }
}

void WebViewProxy::exitFullscreenMode()
{
    if (d_delegate) {
        d_delegate->exitFullscreenMode(this);
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
        content::RenderViewImpl::FromRoutingID(id);

    if (!rv) {
        // The RenderView has not been created yet.  Keep reposting this task
        // until the RenderView is available.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&WebViewProxy::notifyRoutingId,
                           base::Unretained(this),
                           id));
        return;
    }

    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;

    if (d_proxyDelegate) {
        d_proxyDelegate->notifyRoutingId(id);
    }
}

void WebViewProxy::onLoadStatus(int status)
{
    d_pendingLoadStatus = false;

    if (0 == status) {
        LOG(INFO) << "routingId=" << d_renderViewRoutingId
                  << ", didFinishLoad url=" << d_url;

        if (!d_securityToken.IsEmpty()) {
            content::RenderView *rv =
                content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
            VALIDATE_RENDER_VIEW_VOID(rv);

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

bool WebViewProxy::validateClient()
{
    if (!d_client) {
        LOG(WARNING) << "d_client is nullptr";
        if (d_delegate) {
            d_delegate->validateClientFailed(this);
        }
        return false;
    }
    return true;
}



// patch section: msg interception


// patch section: devtools integration
void WebViewProxy::devToolsAgentHostAttached()
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostAttached(this);
    }
}

void WebViewProxy::devToolsAgentHostDetached()
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostDetached(this);
    }
}


// patch section: memory diagnostics
blink::WebFrameWidget* WebViewProxy::getWebFrameWidget() const {
    content::RenderView *rv =
        content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();
    blink::WebLocalFrame* localWebFrame = webFrame->ToWebLocalFrame();
    content::RenderFrameImpl* render_frame =
        content::RenderFrameImpl::FromWebFrame(localWebFrame);
    return render_frame->GetLocalRootWebFrameWidget();
}

std::size_t WebViewProxy::getDefaultTileMemoryLimit() const {
    if(blink::WebFrameWidget* web_frame_widget = getWebFrameWidget()) {
        return web_frame_widget->getDefaultTileMemoryLimit();
    }
    return 0;
}

std::size_t WebViewProxy::getTileMemoryBytes() const
{
    if(blink::WebFrameWidget* web_frame_widget = getWebFrameWidget()) {
        return web_frame_widget->getTileMemoryBytes();
    }
    return 0;
}

void WebViewProxy::overrideTileMemoryLimit(std::size_t limit) {
    if(blink::WebFrameWidget* web_frame_widget = getWebFrameWidget()) {
        return web_frame_widget->overrideTileMemoryLimit(limit);
    }
}

void WebViewProxy::setTag(const char* pTag) {
    if(blink::WebFrameWidget* web_frame_widget = getWebFrameWidget()) {
        return web_frame_widget->setTag(std::string(pTag));
    }
}



}  // close namespace blpwtk2

// vim: ts=4 et


