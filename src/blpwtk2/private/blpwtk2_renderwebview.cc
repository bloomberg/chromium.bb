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

#include <blpwtk2_rendercompositor.h>
#include <blpwtk2_rendermessagedelegate.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_webviewproxy.h>

#include <base/message_loop/message_loop.h>
#include <cc/trees/layer_tree_host.h>
#include <content/browser/renderer_host/display_util.h>
#include <content/common/widget_messages.h>
#include <content/public/renderer/render_view_observer.h>
#include <content/renderer/gpu/layer_tree_view.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/render_view_impl.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_view.h>
#include <ui/display/display.h>
#include <ui/display/screen.h>

namespace {

void GetNativeViewScreenInfo(content::ScreenInfo* screen_info,
                             HWND hwnd) {
    display::Screen* screen = display::Screen::GetScreen();
    if (!screen) {
        *screen_info = content::ScreenInfo();
        return;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO monitor_info = { sizeof(MONITORINFO) };
    GetMonitorInfo(monitor, &monitor_info);

    content::DisplayUtil::DisplayToScreenInfo(
        screen_info,
        screen->GetDisplayMatching(
            gfx::Rect(monitor_info.rcMonitor)));
}

}

namespace blpwtk2 {

                  // ---------------------------------------
                  // class RenderWebView::RenderViewObserver
                  // ---------------------------------------

class RenderWebView::RenderViewObserver : public content::RenderViewObserver {
    RenderWebView *d_renderWebView;

  public:

    RenderViewObserver(content::RenderViewImpl *rv, RenderWebView *renderWebView);
    ~RenderViewObserver() override;

    void OnDestruct() override;
};

RenderWebView::RenderViewObserver::RenderViewObserver(
    content::RenderViewImpl *rv, RenderWebView *renderWebView)
: content::RenderViewObserver(rv)
, d_renderWebView(renderWebView)
{
    d_renderWebView->d_renderViewObserver = this;
}

RenderWebView::RenderViewObserver::~RenderViewObserver()
{
    d_renderWebView->detachFromRoutingId();
    d_renderWebView->d_renderViewObserver = nullptr;
}

void RenderWebView::RenderViewObserver::OnDestruct()
{
    LOG(INFO) << "Destroyed RenderView, routingId=" << routing_id();
    delete this;
}

                        // -------------------
                        // class RenderWebView
                        // -------------------

RenderWebView::RenderWebView(WebViewDelegate          *delegate,
                             ProfileImpl              *profile,
                             const WebViewProperties&  properties)
    : d_proxy(nullptr)
    , d_delegate(delegate)
    , d_profile(profile)
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    , d_properties(properties)
#endif
    , d_pendingDestroy(false)
{
    initialize();
}

RenderWebView::~RenderWebView()
{
    LOG(INFO) << "Destroying RenderWebView, routingId=" << d_renderViewRoutingId;

    if (d_renderViewObserver) {
        delete d_renderViewObserver;
    }
}

LPCTSTR RenderWebView::GetWindowClass()
{
    static const LPCTSTR s_className    = L"blpwtk2-RenderWebView";
    static ATOM          s_atom         = 0;

    if (s_atom == 0) {
        static WNDCLASSEX s_class = {};

        s_class.cbSize        = sizeof(WNDCLASSEX);
        s_class.style         = 0;
        s_class.lpfnWndProc   = WindowProcedure;
        s_class.cbClsExtra    = 0;
        s_class.cbWndExtra    = 0;
        s_class.hInstance     = GetModuleHandle(NULL);
        s_class.hIcon         = NULL;
        s_class.hCursor       = NULL;
        s_class.hbrBackground = NULL;
        s_class.lpszMenuName  = NULL;
        s_class.lpszClassName = s_className;
        s_class.hIconSm       = NULL;

        s_atom = RegisterClassEx(&s_class);
        DCHECK(s_atom);
    }

    return s_className;
}

LRESULT CALLBACK RenderWebView::WindowProcedure(HWND      hWnd,
                                                UINT      uMsg,
                                                WPARAM    wParam,
                                                LPARAM    lParam)
{
    RenderWebView *that = reinterpret_cast<RenderWebView *>(
        GetWindowLongPtr(hWnd, GWLP_USERDATA)
    );

    // GWL_USERDATA hasn't been set to anything yet:
    if (!that) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    else {
        // Otherwise:
        DCHECK(that->d_hwnd == hWnd);

        return that->windowProcedure(uMsg, wParam, lParam);
    }
}

LRESULT RenderWebView::windowProcedure(UINT   uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam)
{
    switch (uMsg) {
    case WM_NCDESTROY: {
        if (d_gotRenderViewInfo) {
            content::RenderWidget::FromRoutingID(d_renderWidgetRoutingId)->
                layer_tree_view()->SetVisible(false);
        }

        if (d_compositor) {
            d_compositor->SetVisible(false);
        }

        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wunused"

        HWND leaked_hwnd = d_hwnd.release();

        #pragma clang diagnostic pop
    } return 0;
    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS *windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

        gfx::Size size(windowpos->cx, windowpos->cy);

        if (d_compositor &&
            ((size != d_geometry.size() && !(windowpos->flags & SWP_NOSIZE)) ||
             windowpos->flags & SWP_FRAMECHANGED)) {
            d_compositor->Resize(gfx::Size());
        }
    } break;
    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS *windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

        if (windowpos->flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW)) {
            d_visible = (windowpos->flags & SWP_SHOWWINDOW)?
                true : false;

            updateVisibility();
        }

        gfx::Rect geometry(
            windowpos->x, windowpos->y, windowpos->cx, windowpos->cy);

        if ((geometry != d_geometry &&
            (!(windowpos->flags & SWP_NOSIZE) ||
             !(windowpos->flags & SWP_NOMOVE))) ||
            windowpos->flags & SWP_FRAMECHANGED) {
            d_geometry = geometry;

            updateGeometry();
        }
    } return 0;
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }

    return DefWindowProc(d_hwnd.get(), uMsg, wParam, lParam);
}

void RenderWebView::initialize()
{
    d_hwnd.reset(CreateWindowEx(
#if defined(BLPWTK2_FEATURE_FOCUS)
        d_properties.activateWindowOnMouseDown?
            0 : WS_EX_NOACTIVATE,
#else
        0,
#endif
        GetWindowClass(),
        L"blpwtk2-RenderWebView",
        WS_OVERLAPPED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1,
        1,
        NULL,
        NULL,
        NULL,
        NULL));
    DCHECK(d_hwnd.is_valid());

    SetWindowLongPtr(d_hwnd.get(), GWLP_USERDATA, reinterpret_cast<LONG>(this));

    SetWindowLong(
        d_hwnd.get(),
        GWL_STYLE,
        GetWindowLong(d_hwnd.get(), GWL_STYLE) & ~WS_CAPTION);

    RECT rect;
    GetWindowRect(d_hwnd.get(), &rect);
    d_geometry = gfx::Rect(rect);
}

void RenderWebView::updateVisibility()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    d_compositor->SetVisible(d_visible);

    if (d_visible) {
        dispatchToRenderWidget(
            WidgetMsg_WasShown(d_renderWidgetRoutingId,
                base::TimeTicks::Now(), false));
    }
    else {
        dispatchToRenderWidget(
            WidgetMsg_WasHidden(d_renderWidgetRoutingId));
    }
}

void RenderWebView::updateGeometry()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    auto size = d_geometry.size();

    d_compositor->Resize(size);

    content::VisualProperties params = {};
    params.new_size = size;
    params.compositor_viewport_pixel_size = size;
    params.visible_viewport_size = size;
    params.display_mode = blink::kWebDisplayModeBrowser;
    params.local_surface_id_allocation = d_compositor->GetLocalSurfaceIdAllocation();
    GetNativeViewScreenInfo(&params.screen_info, d_hwnd.get());

    dispatchToRenderWidget(
        WidgetMsg_SynchronizeVisualProperties(d_renderWidgetRoutingId,
            params));
}

void RenderWebView::detachFromRoutingId()
{
    d_compositor.reset();

    RenderMessageDelegate::GetInstance()->RemoveRoute(
        d_mainFrameRoutingId);
    d_mainFrameRoutingId = 0;

    RenderMessageDelegate::GetInstance()->RemoveRoute(
        d_renderWidgetRoutingId);
    d_renderWidgetRoutingId = 0;

    RenderMessageDelegate::GetInstance()->RemoveRoute(
        d_renderViewRoutingId);
    d_renderViewRoutingId = 0;

    d_gotRenderViewInfo = false;
}

bool RenderWebView::dispatchToRenderWidget(const IPC::Message& message)
{
    content::RenderView *rv =
        content::RenderView::FromRoutingID(d_renderViewRoutingId);

    if (rv) {
        blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();

        v8::Isolate* isolate = webFrame->ScriptIsolate();
        v8::Isolate::Scope isolateScope(isolate);

        v8::HandleScope handleScope(isolate);

        v8::Context::Scope contextScope(
            webFrame->ToWebLocalFrame()->MainWorldScriptContext());

        return static_cast<IPC::Listener *>(
            content::RenderThreadImpl::current())
                ->OnMessageReceived(message);
    }
    else {
        return static_cast<IPC::Listener *>(
            content::RenderThreadImpl::current())
                ->OnMessageReceived(message);
    }
}

void RenderWebView::sendScreenRects()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    RECT view_screen_rect;
    GetWindowRect(d_hwnd.get(), &view_screen_rect);

    dispatchToRenderWidget(
        WidgetMsg_UpdateScreenRects(d_renderWidgetRoutingId,
            gfx::Rect(view_screen_rect),
            gfx::Rect(view_screen_rect)));
}

// blpwtk2::WebView overrides:
void RenderWebView::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);

    if (d_hwnd.is_valid()) {
        d_hwnd.reset();
    }

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
    //TODO
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

    // TODO
}

void RenderWebView::setLogicalFocus(bool focused)
{
    // TODO
}
#endif

void RenderWebView::show()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());

    if (d_shown) {
        return;
    }

    d_shown = true;

    if (d_hasParent) {
        SetWindowPos(
            d_hwnd.get(),
            0,
            0, 0, 0, 0,
            SWP_SHOWWINDOW |
            SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }
}

void RenderWebView::hide()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());

    if (!d_shown) {
        return;
    }

    d_shown = false;

    if (d_hasParent) {
        SetWindowPos(
            d_hwnd.get(),
            0,
            0, 0, 0, 0,
            SWP_HIDEWINDOW |
            SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }
}

void RenderWebView::setParent(NativeView parent)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());

    auto shown = d_shown;

    // The window is losing its parent:
    if (!parent && d_hasParent) {
        if (shown) {
            SetWindowPos(
                d_hwnd.get(),
                0,
                0, 0, 0, 0,
                SWP_HIDEWINDOW |
                SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        }
    }
    else if (parent && !d_hasParent) {
        SetWindowLong(
            d_hwnd.get(),
            GWL_STYLE,
            (GetWindowLong(d_hwnd.get(), GWL_STYLE) & ~WS_OVERLAPPED) | WS_CHILD);
    }

    SetParent(d_hwnd.get(), parent);

    // The window is gaining a parent:
    if (parent && !d_hasParent) {
        if (shown) {
            SetWindowPos(
                d_hwnd.get(),
                0,
                0, 0, 0, 0,
                SWP_SHOWWINDOW |
                SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        }
    }
    else if (!parent && d_hasParent) {
        SetWindowLong(
            d_hwnd.get(),
            GWL_STYLE,
            (GetWindowLong(d_hwnd.get(), GWL_STYLE) & ~WS_CHILD) | WS_OVERLAPPED);
    }

    d_hasParent = !!parent;
}

void RenderWebView::move(int left, int top, int width, int height)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(0 <= width);
    DCHECK(0 <= height);
    DCHECK(d_hwnd.is_valid());

    SetWindowPos(
        d_hwnd.get(),
        0,
        left, top, width, height,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
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
    sendScreenRects();
}

void RenderWebView::rootWindowSettingsChanged()
{
    sendScreenRects();
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

    static_cast<WebViewProxy *>(d_proxy)->setProxyDelegate(this);

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

// WebViewProxyDelegate overrides
void RenderWebView::notifyRoutingId(int id)
{
    if (d_gotRenderViewInfo) {
        return;
    }

    if (d_pendingDestroy) {
        LOG(INFO) << "WebView destroyed before we got a reference to a RenderView";
        return;
    }

    content::RenderViewImpl *rv =
        content::RenderViewImpl::FromRoutingID(id);
    DCHECK(rv);

    //
    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderViewRoutingId, this);

    d_renderWidgetRoutingId = rv->GetWidget()->routing_id();

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderWidgetRoutingId, this);

    d_mainFrameRoutingId = rv->GetMainRenderFrame()->GetRoutingID();

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_mainFrameRoutingId, this);

    d_renderViewObserver = new RenderViewObserver(rv, this);

    // Create a RenderCompositor that is associated with this
    // content::RenderWidget:
    d_compositor = RenderCompositorFactory::GetInstance()->CreateCompositor(
        d_renderWidgetRoutingId, d_hwnd.get(), d_profile);

    // Destroy any upstream compositor previously created for this widget
    // by hiding it and then calling 'ReleaseLayerTreeFrameSink':
    auto was_visible = rv->GetWidget()->
        layer_tree_view()->layer_tree_host()->IsVisible();
    if (was_visible) {
        rv->GetWidget()->
            layer_tree_view()->SetVisible(false);
    }
    rv->GetWidget()->
        layer_tree_view()->layer_tree_host()->ReleaseLayerTreeFrameSink();
    if (was_visible) {
        rv->GetWidget()->
            layer_tree_view()->SetVisible(true);
    }

    updateVisibility();
    updateGeometry();

    // Set input event routing:
    d_inputRouterImpl.reset(
        new content::InputRouterImpl(
            this, this, this, content::InputRouter::Config()));

    content::mojom::WidgetInputHandlerHostPtr input_handler_host_ptr;
    auto widgetInputHandlerHostRequest = mojo::MakeRequest(&input_handler_host_ptr);

    rv->GetWidget()->SetupWidgetInputHandler(
        mojo::MakeRequest(&d_widgetInputHandler), std::move(input_handler_host_ptr));

    d_inputRouterImpl->BindHost(
        std::move(widgetInputHandlerHostRequest), true);
}

// IPC::Listener overrideds:
bool RenderWebView::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(RenderWebView, message)
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    return handled;
}

// content::InputRouterClient overrides:
content::InputEventAckState RenderWebView::FilterInputEvent(
    const blink::WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info)
{
    return content::INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWebView::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency_info)
{
    d_inputRouterImpl->SendGestureEvent(
        content::GestureEventWithLatencyInfo(
            gesture_event,
            latency_info));
}

bool RenderWebView::IsWheelScrollInProgress()
{
    return false;
}

// content::InputRouterImplClient overrides:
content::mojom::WidgetInputHandler* RenderWebView::GetWidgetInputHandler()
{
    return d_widgetInputHandler.get();
}

// content::FlingControllerSchedulerClient overrides:
bool RenderWebView::NeedsBeginFrameForFlingProgress()
{
    return false;
}

}  // close namespace blpwtk2

// vim: ts=4 et


