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

#include <blpwtk2_mainmessagepump.h>
#include <blpwtk2_rendercompositor.h>
#include <blpwtk2_rendermessagedelegate.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_webviewproxy.h>

#include <base/message_loop/message_loop.h>
#include <cc/trees/layer_tree_host.h>
#include <content/browser/renderer_host/display_util.h>
#include <content/common/drag_messages.h>
#include <content/common/frame_messages.h>
#include <content/common/view_messages.h>
#include <content/common/widget_messages.h>
#include <content/public/renderer/render_view_observer.h>
#include <content/renderer/compositor/layer_tree_view.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/render_view_impl.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_view.h>
#include <ui/base/ime/init/input_method_factory.h>
#include <ui/base/ime/input_method.h>
#include <ui/base/win/lock_state.h>
#include <ui/base/win/mouse_wheel_util.h>
#include <ui/display/display.h>
#include <ui/display/screen.h>
#include <ui/events/blink/web_input_event.h>
#include <ui/base/win/session_change_observer.h>

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
#include <ui/base/win/rubberband_windows.h>
#endif

#include <windowsx.h>

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

constexpr base::TimeDelta kDefaultMouseWheelLatchingTransaction =
    base::TimeDelta::FromMilliseconds(500);
constexpr double kWheelLatchingSlopRegion = 10.0;

constexpr base::TimeDelta kDelayForTooltipUpdateInMs =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kDefaultTooltipShownTimeoutMs =
    base::TimeDelta::FromMilliseconds(10000);

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

RenderWebView::RenderWebView(ProfileImpl              *profile,
                             int                       routingId,
                             const gfx::Rect&          initialRect)
    : d_proxy(nullptr)
    , d_delegate(nullptr)
    , d_profile(profile)
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    , d_properties({ false, false, false, false, false, false })
#endif
    , d_pendingDestroy(false)
    , d_renderViewRoutingId(0)
    , d_renderWidgetRoutingId(0)
    , d_mainFrameRoutingId(0)
{
    initialize();

    SetWindowLong(
        d_hwnd.get(), GWL_STYLE,
        GetWindowLong(d_hwnd.get(), GWL_STYLE) | WS_POPUP);

    //
    d_gotRenderViewInfo = true;

    d_renderWidgetRoutingId = routingId;

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderWidgetRoutingId, this);

    // Create a RenderCompositor that is associated with this
    // content::RenderWidget:
    d_compositor = RenderCompositorFactory::GetInstance()->CreateCompositor(
        d_renderWidgetRoutingId, d_hwnd.get(), d_profile);

    updateVisibility();
    updateGeometry();

    // Set input event routing:
    d_inputRouterImpl.reset(
        new content::InputRouterImpl(
            this, this, this, content::InputRouter::Config()));

    content::mojom::WidgetInputHandlerHostPtr input_handler_host_ptr;
    auto widgetInputHandlerHostRequest = mojo::MakeRequest(&input_handler_host_ptr);

    content::RenderWidget::FromRoutingID(routingId)->
        SetupWidgetInputHandler(
            mojo::MakeRequest(&d_widgetInputHandler), std::move(input_handler_host_ptr));

    d_inputRouterImpl->BindHost(
        std::move(widgetInputHandlerHostRequest), true);

    updateFocus();

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    updateAltDragRubberBanding();
#endif

    //
    d_shown = true;

    SetWindowPos(
        d_hwnd.get(),
        0,
        initialRect.x(),     initialRect.y(),
        initialRect.width(), initialRect.height(),
        SWP_SHOWWINDOW | SWP_FRAMECHANGED |
        SWP_NOACTIVATE | SWP_NOOWNERZORDER);
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
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(d_hwnd.get(), &ps);

        if (d_gotRenderViewInfo) {
            content::RenderWidget::FromRoutingID(d_renderWidgetRoutingId)->
                Redraw();
        }

        EndPaint(d_hwnd.get(), &ps);
    } return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST: {
        if (d_ncHitTestEnabled && d_delegate) {
            d_ncHitTestResult = HTCLIENT;
            d_delegate->requestNCHitTest(this);
            return d_ncHitTestResult;
        }
    } break;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_IME_CHAR:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_REQUEST:
    case WM_IME_NOTIFY:
    case WM_IME_SETCONTEXT:
    case WM_IME_STARTCOMPOSITION: {
        MSG msg;
        msg.hwnd    = d_hwnd.get();
        msg.message = uMsg;
        msg.wParam  = wParam;
        msg.lParam  = lParam;
        msg.time    = GetMessageTime();

        auto pt     = GetMessagePos();
        msg.pt.x    = GET_X_LPARAM(pt);
        msg.pt.y    = GET_Y_LPARAM(pt);

        switch (uMsg) {
        // Mouse events:
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            auto event =
                ui::MakeWebMouseEvent(
                    ui::MouseEvent(msg));

            // Mouse enter/leave:
            switch (uMsg) {
            case WM_MOUSEMOVE: {
                if (!d_mouseEntered) {
                    TRACKMOUSEEVENT track_mouse_event = {
                        sizeof(TRACKMOUSEEVENT),
                        TME_LEAVE,
                        d_hwnd.get(),
                        0
                    };

                    if (TrackMouseEvent(&track_mouse_event)) {
                        d_mouseEntered = true;

                        d_mouseScreenPosition.SetPoint(
                            event.PositionInScreen().x,
                            event.PositionInScreen().y);
                    }
                }
            } break;
            case WM_MOUSELEAVE: {
                d_mouseEntered = false;

                d_mouseScreenPosition.SetPoint(
                    event.PositionInScreen().x,
                    event.PositionInScreen().y);
            } break;
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN: {
                d_mousePressed = true;

                d_tooltipTextAtMousePress = d_lastTooltipText;
                d_tooltip->Hide();

#if defined(BLPWTK2_FEATURE_FOCUS)
                // Focus on mouse button down:
                if (d_properties.takeKeyboardFocusOnMouseDown) {
                    SetFocus(d_hwnd.get());
                }
#endif

                // Capture on mouse button down:
                SetCapture(d_hwnd.get());
            } break;
            // Capture on mouse button up:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP: {
                d_mousePressed = false;

                ReleaseCapture();
            } break;
            }

            event.movement_x = event.PositionInScreen().x - d_mouseScreenPosition.x();
            event.movement_y = event.PositionInScreen().y - d_mouseScreenPosition.y();

            d_mouseScreenPosition.SetPoint(
                event.PositionInScreen().x,
                event.PositionInScreen().y);

            if (d_mouseLocked) {
                event.SetPositionInWidget(
                    d_unlockedMouseWebViewPosition.x(),
                    d_unlockedMouseWebViewPosition.y());
                event.SetPositionInScreen(
                    d_unlockedMouseScreenPosition.x(),
                    d_unlockedMouseScreenPosition.y());
            }
            else {
                d_unlockedMouseWebViewPosition.SetPoint(
                    event.PositionInWidget().x, event.PositionInWidget().y);
                d_unlockedMouseScreenPosition.SetPoint(
                    event.PositionInScreen().x, event.PositionInScreen().y);
            }

            if (d_inputRouterImpl) {
                d_inputRouterImpl->SendMouseEvent(
                    content::MouseEventWithLatencyInfo(
                        event,
                        ui::LatencyInfo()),
                    base::BindOnce(
                        &RenderWebView::onMouseEventAck,
                        base::Unretained(this)));
            }

            return 0;
        } break;
        // Mousewheel:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            if (d_tooltip->IsVisible()) {
                d_tooltip->Hide();
            }

#if defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
            if (ui::RerouteMouseWheel(
                d_hwnd.get(),
                wParam, lParam,
                d_properties.rerouteMouseWheelToAnyRelatedWindow)) {
                return 0;
            }
#else
            if (ui::RerouteMouseWheel(
                d_hwnd.get(),
                wParam, lParam)) {
                return 0;
            }
#endif

            auto event =
                ui::MakeWebMouseWheelEvent(
                    ui::MouseWheelEvent(msg));

            gfx::Vector2dF location(event.PositionInWidget().x, event.PositionInWidget().y);

            event.has_synthetic_phase = true;

            const auto is_within_slop_region =
                (location - d_firstWheelLocation).LengthSquared() <
                (kWheelLatchingSlopRegion*kWheelLatchingSlopRegion);
            const auto has_different_modifiers =
                event.GetModifiers() != d_initialWheelEvent.GetModifiers();
            const auto consistent_x_direction =
                (event.delta_x == 0 && d_initialWheelEvent.delta_x == 0) ||
                event.delta_x * d_initialWheelEvent.delta_x > 0;
            const auto consistent_y_direction =
                (event.delta_y == 0 && d_initialWheelEvent.delta_y == 0) ||
                event.delta_y * d_initialWheelEvent.delta_y > 0;

            if (is_within_slop_region ||
                has_different_modifiers ||
                (d_firstScrollUpdateAckState == FirstScrollUpdateAckState::kNotConsumed && (!consistent_x_direction || !consistent_y_direction))) {
                if (d_mouseWheelEndDispatchTimer.IsRunning()) {
                    d_mouseWheelEndDispatchTimer.FireNow();
                }
            }

            if (!d_mouseWheelEndDispatchTimer.IsRunning()) {
                event.phase = blink::WebMouseWheelEvent::kPhaseBegan;

                d_firstWheelLocation = location;
                d_initialWheelEvent = event;
                d_firstScrollUpdateAckState = FirstScrollUpdateAckState::kNotArrived;

                d_mouseWheelEndDispatchTimer.Start(
                    FROM_HERE,
                    kDefaultMouseWheelLatchingTransaction,
                    base::BindOnce(
                        &RenderWebView::onQueueWheelEventWithPhaseEnded,
                        base::Unretained(this)));
            }
            else {
                event.phase =
                    (event.delta_x || event.delta_y)                ?
                        blink::WebMouseWheelEvent::kPhaseChanged    :
                        blink::WebMouseWheelEvent::kPhaseStationary;

                d_mouseWheelEndDispatchTimer.Reset();
            }

            d_lastMouseWheelEvent = event;

            if (d_inputRouterImpl) {
                d_inputRouterImpl->SendWheelEvent(
                    content::MouseWheelEventWithLatencyInfo(
                        event,
                        ui::LatencyInfo()));
            }

            return 0;
        } break;
        // Keyboard:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            if (d_tooltipShownTimer.IsRunning()) {
                d_tooltipShownTimer.Stop();
                hideTooltip();
            }

            ui::KeyEvent event(msg);

            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wunused"

            d_inputMethod->DispatchKeyEvent(&event);

            #pragma clang diagnostic pop

            if (event.handled()) {
                return 0;
            }
        } break;
        // Input method keyboard:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_IME_CHAR:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_REQUEST:
        case WM_IME_NOTIFY:
        case WM_IME_SETCONTEXT:
        case WM_IME_STARTCOMPOSITION: {
            LRESULT result = 0;
            auto handled = d_inputMethod->OnUntranslatedIMEMessage(msg, &result);

            if (handled) {
                return result;
            }
        } break;
        }
    } break;
    case WM_SETCURSOR: {
        wchar_t *cursor = IDC_ARROW;

        switch (LOWORD(lParam)) {
        case HTSIZE:
            cursor = IDC_SIZENWSE;
            break;
        case HTLEFT:
        case HTRIGHT:
            cursor = IDC_SIZEWE;
            break;
        case HTTOP:
        case HTBOTTOM:
            cursor = IDC_SIZENS;
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
        case HTOBJECT:
            cursor = IDC_SIZENWSE;
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            cursor = IDC_SIZENESW;
            break;
        case HTCLIENT:
            d_isCursorOverridden = false;
            setPlatformCursor(d_currentPlatformCursor);
            return 1;
        case LOWORD(HTERROR):
            return 0;
        default:
            break;
        }

        d_isCursorOverridden = true;
        SetCursor(
            LoadCursor(NULL, cursor));
    } return 1;
    case WM_MOUSEACTIVATE: {
        if (GetWindowLong(d_hwnd.get(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
            return MA_NOACTIVATE;
        }
    } break;
    case WM_SETFOCUS: {
        d_inputMethod->SetFocusedTextInputClient(this);
        d_inputMethod->OnFocus();

        if (d_delegate) {
            d_delegate->focused(this);
        }

        d_focused = true;

        updateFocus();
    } return 0;
    case WM_KILLFOCUS: {
        d_inputMethod->SetFocusedTextInputClient(nullptr);
        d_inputMethod->OnBlur();

        if (d_delegate) {
            d_delegate->blurred(this);
        }

        d_focused = false;

        updateFocus();
    } return 0;
    case WM_NOTIFY: {
        LPARAM l_result = 0;
        if (d_tooltip && static_cast<views::corewm::TooltipWin *>(d_tooltip.get())->
                HandleNotify(
                    wParam, reinterpret_cast<NMHDR*>(lParam), &l_result)) {
            return 1;
        }
    } return 0;
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

    d_cursorLoader.reset(ui::CursorLoader::Create());
    d_currentPlatformCursor = LoadCursor(NULL, IDC_ARROW);

    d_inputMethod = ui::CreateInputMethod(this, d_hwnd.get());

    d_dragDrop = new DragDrop(d_hwnd.get(), this);

    d_tooltip = std::make_unique<views::corewm::TooltipWin>(d_hwnd.get());

    d_windowsSessionChangeObserver =
        std::make_unique<ui::SessionChangeObserver>(
            base::Bind(&RenderWebView::onSessionChange, base::Unretained(this)));
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
                base::TimeTicks::Now(), false, base::nullopt));
    }
    else {
        dispatchToRenderWidget(
            WidgetMsg_WasHidden(d_renderWidgetRoutingId));

        d_tooltip->Hide();
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

    // `WidgetMsg_SynchronizeVisualProperties` is ignored most of the time,
    // but needs to be allowed here:
    content::RenderViewImpl *rv =
            content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);

    if (rv) {
        content::RenderWidget *rw = rv->GetWidget();

        rw->bbIgnoreSynchronizeVisualPropertiesIPC(false);

        dispatchToRenderWidget(
                WidgetMsg_SynchronizeVisualProperties(d_renderWidgetRoutingId,
                    params));

        rw->bbIgnoreSynchronizeVisualPropertiesIPC(true);
    }
    else {
        dispatchToRenderWidget(
                WidgetMsg_SynchronizeVisualProperties(d_renderWidgetRoutingId,
                    params));
    }
}

void RenderWebView::updateFocus()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    if (d_focused) {
        d_widgetInputHandler->SetFocus(d_focused);

        dispatchToRenderWidget(
            WidgetMsg_SetActive(d_renderWidgetRoutingId,
                d_focused));
    }
    else {
        dispatchToRenderWidget(
            WidgetMsg_SetActive(d_renderWidgetRoutingId,
                d_focused));

        d_widgetInputHandler->SetFocus(d_focused);
    }
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

void RenderWebView::setPlatformCursor(HCURSOR cursor)
{
    if (d_isCursorOverridden) {
        d_currentPlatformCursor = cursor;
        return;
    }

    if (cursor) {
        d_previousPlatformCursor = SetCursor(cursor);
        d_currentPlatformCursor = cursor;
    }
    else if (d_previousPlatformCursor) {
        SetCursor(d_previousPlatformCursor);
        d_previousPlatformCursor = NULL;
    }
}

void RenderWebView::onQueueWheelEventWithPhaseEnded()
{
    d_lastMouseWheelEvent.SetTimeStamp(ui::EventTimeForNow());
    d_lastMouseWheelEvent.delta_x = 0;
    d_lastMouseWheelEvent.delta_y = 0;
    d_lastMouseWheelEvent.wheel_ticks_x = 0;
    d_lastMouseWheelEvent.wheel_ticks_y = 0;
    d_lastMouseWheelEvent.dispatch_type = blink::WebInputEvent::DispatchType::kEventNonBlocking;

    d_lastMouseWheelEvent.phase = blink::WebMouseWheelEvent::kPhaseEnded;

    if (d_inputRouterImpl) {
        d_inputRouterImpl->SendWheelEvent(
            content::MouseWheelEventWithLatencyInfo(
                d_lastMouseWheelEvent,
                ui::LatencyInfo()));
    }
}

void RenderWebView::onStartDraggingImpl(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask operations_allowed,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const content::DragEventSourceInfo& event_info)
{
    MainMessagePump::ScopedModalLoopWorkAllower allow(
            MainMessagePump::current());

    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    run_loop.BeforeRun();

    d_dragDrop->StartDragging(
        drop_data, operations_allowed, bitmap, bitmap_offset_in_dip, event_info);

    run_loop.AfterRun();
}

void RenderWebView::showTooltip()
{
    POINT location;
    GetCursorPos(&location);

    d_tooltip->SetText(nullptr, d_lastTooltipText, gfx::Point(location));
    d_tooltip->Show();

    d_tooltipShownTimer.Start(
        FROM_HERE,
        kDefaultTooltipShownTimeoutMs,
        this,
        &RenderWebView::hideTooltip);
}

void RenderWebView::hideTooltip()
{
    d_tooltip->Hide();
}

void RenderWebView::updateTooltip()
{
    if (d_mousePressed) {
        if (d_tooltipTextAtMousePress == d_lastTooltipText) {
            d_tooltip->Hide();
            return;
        }
    }

    if (d_tooltipText != d_lastTooltipText || !d_tooltip->IsVisible()) {
        d_tooltipShownTimer.Stop();
        d_lastTooltipText = d_tooltipText;

        if (d_lastTooltipText.empty()) {
            d_tooltip->Hide();
            d_tooltipDeferTimer.Stop();
        }
        else {
            if (d_tooltipDeferTimer.IsRunning()) {
                d_tooltipDeferTimer.Reset();
            }
            else {
                d_tooltipDeferTimer.Start(
                    FROM_HERE,
                    kDelayForTooltipUpdateInMs,
                    this,
                    &RenderWebView::showTooltip);
            }
        }
    }
}

void RenderWebView::onSessionChange(WPARAM status_code)
{
    // Direct3D presents are ignored while the screen is locked, so force the
    // window to be redrawn on unlock.
    if (status_code == WTS_SESSION_UNLOCK)
        forceRedrawWindow(10);
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
void RenderWebView::updateAltDragRubberBanding()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderWidget(
        ViewMsg_EnableAltDragRubberbanding(d_renderViewRoutingId,
            d_enableAltDragRubberBanding));
}
#endif

void RenderWebView::forceRedrawWindow(int attempts)
{
    if (ui::IsWorkstationLocked()) {
        // Presents will continue to fail as long as the input desktop is
        // unavailable.
        if (--attempts <= 0)
            return;
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&RenderWebView::forceRedrawWindow,
                       base::Unretained(this),
                       attempts),
            base::TimeDelta::FromMilliseconds(500));
        return;
    }
    InvalidateRect(d_hwnd.get(), NULL, FALSE);
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

    SetFocus(d_hwnd.get());
}

void RenderWebView::setLogicalFocus(bool focused)
{
    if (d_gotRenderViewInfo) {
        // If we have the renderer in-process, then set the logical focus
        // immediately so that handleInputEvents will work as expected.
        content::RenderViewImpl *rv =
            content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
        DCHECK(rv);
        rv->SetFocus(focused);
    }
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

int RenderWebView::setParent(NativeView parent)
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

    int status = 0;
    if (!::SetParent(d_hwnd.get(), parent)) {
        status = ::GetLastError();
    }

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
    return status;
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
    d_ncHitTestEnabled = enabled;
}

void RenderWebView::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_ncHitTestResult = result;
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

    d_enableAltDragRubberBanding = enabled;
    updateAltDragRubberBanding();
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
String RenderWebView::printToPDF()
{
    return d_proxy->printToPDF();
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

    d_proxy->move(0, 0, 0, 0);

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

#if defined(BLPWTK2_FEATURE_PERFORMANCETIMING)
void RenderWebView::startPerformanceTiming()
{
    if (d_delegate) {
        d_delegate->startPerformanceTiming();
    }
}

void RenderWebView::stopPerformanceTiming()
{
    if (d_delegate) {
        d_delegate->stopPerformanceTiming();
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

    // Ignore `WidgetMsg_SynchronizeVisualProperties` sent from the browser:
    rv->GetWidget()->bbIgnoreSynchronizeVisualPropertiesIPC(true);

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

    rv->GetWebView()->SetHwnd(d_hwnd.get());

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

    updateFocus();

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    updateAltDragRubberBanding();
#endif
}

// IPC::Listener overrideds:
bool RenderWebView::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(RenderWebView, message)
        // Mouse locking:
        IPC_MESSAGE_HANDLER(WidgetHostMsg_LockMouse,
            OnLockMouse)
        IPC_MESSAGE_HANDLER(WidgetHostMsg_UnlockMouse,
            OnUnlockMouse)
        // Cursor setting:
        IPC_MESSAGE_HANDLER(WidgetHostMsg_SetCursor,
            OnSetCursor)
        // Keyboard events:
        IPC_MESSAGE_HANDLER(WidgetHostMsg_SelectionBoundsChanged,
            OnSelectionBoundsChanged)
        IPC_MESSAGE_HANDLER(FrameHostMsg_SelectionChanged,
            OnSelectionChanged)
        IPC_MESSAGE_HANDLER(WidgetHostMsg_TextInputStateChanged,
            OnTextInputStateChanged)
        IPC_MESSAGE_UNHANDLED(handled = false)
        // Drag and drop:
        IPC_MESSAGE_HANDLER(DragHostMsg_StartDragging,
            OnStartDragging)
        IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor,
            OnUpdateDragCursor)
        // Renderer-driven popups:
        IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget,
            OnShowWidget)
        IPC_MESSAGE_HANDLER(WidgetHostMsg_Close,
            OnClose)
        // Native tooltips:
        IPC_MESSAGE_HANDLER(WidgetHostMsg_SetTooltipText,
            OnSetTooltipText)
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
        // Rubber band selection:
        IPC_MESSAGE_HANDLER(ViewHostMsg_HideRubberbandRect,
            OnHideRubberbandRect)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SetRubberbandRect,
            OnSetRubberbandRect)
#endif
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

bool RenderWebView::IsAutoscrollInProgress()
{
    return false;
}


// content::InputRouterImplClient overrides:
content::mojom::WidgetInputHandler* RenderWebView::GetWidgetInputHandler()
{
    return d_widgetInputHandler.get();
}

void RenderWebView::OnImeCancelComposition()
{
    d_inputMethod->CancelComposition(this);
    d_hasCompositionText = false;
}

void RenderWebView::OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& character_bounds)
{
    d_compositionCharacterBounds = character_bounds;
}

// content::FlingControllerSchedulerClient overrides:
bool RenderWebView::NeedsBeginFrameForFlingProgress()
{
    return false;
}

// ui::internal::InputMethodDelegate overrides:
ui::EventDispatchDetails RenderWebView::DispatchKeyEventPostIME(
    ui::KeyEvent* key_event,
    DispatchKeyEventPostIMECallback ack_callback)
{
    if (!key_event->handled()) {
        if (d_inputRouterImpl) {
            d_inputRouterImpl->SendKeyboardEvent(
                content::NativeWebKeyboardEventWithLatencyInfo(
                    content::NativeWebKeyboardEvent(*key_event),
                    ui::LatencyInfo()),
                base::BindOnce(
                    &RenderWebView::onKeyboardEventAck,
                    base::Unretained(this)));
        }
        RunDispatchKeyEventPostIMECallback(key_event, std::move(ack_callback));
    }

    return ui::EventDispatchDetails();
}

// ui::TextInputClient overrides:
void RenderWebView::SetCompositionText(const ui::CompositionText& composition)
{
    d_widgetInputHandler->
        ImeSetComposition(
            composition.text,
            composition.ime_text_spans,
            gfx::Range::InvalidRange(),
            composition.selection.end(), composition.selection.end());

    d_hasCompositionText = !composition.text.empty();
}

void RenderWebView::ConfirmCompositionText()
{
    if (d_hasCompositionText) {
        d_widgetInputHandler->
            ImeFinishComposingText(false);
    }

    d_hasCompositionText = false;
}

void RenderWebView::ClearCompositionText()
{
    if (d_hasCompositionText) {
        d_widgetInputHandler->
            ImeSetComposition(
                base::string16(),
                {},
                gfx::Range::InvalidRange(),
                0, 0);
    }

    d_hasCompositionText = false;
}

void RenderWebView::InsertText(const base::string16& text)
{
    if (!text.empty()) {
        d_widgetInputHandler->
            ImeCommitText(
                text,
                {},
                gfx::Range::InvalidRange(),
                0,
                {});
    }
    else {
        d_widgetInputHandler->
            ImeFinishComposingText(
                false);
    }

    d_hasCompositionText = false;
}

void RenderWebView::InsertChar(const ui::KeyEvent& event)
{
    if (d_inputRouterImpl) {
        d_inputRouterImpl->SendKeyboardEvent(
            content::NativeWebKeyboardEventWithLatencyInfo(
                content::NativeWebKeyboardEvent(event),
                ui::LatencyInfo()),
            base::BindOnce(
                &RenderWebView::onKeyboardEventAck,
                base::Unretained(this)));
    }
}

ui::TextInputType RenderWebView::GetTextInputType() const
{
    return d_textInputState.type;
}

ui::TextInputMode RenderWebView::GetTextInputMode() const
{
    return d_textInputState.mode;
}

base::i18n::TextDirection RenderWebView::GetTextDirection() const
{
    NOTIMPLEMENTED();
    return base::i18n::UNKNOWN_DIRECTION;
}

int RenderWebView::GetTextInputFlags() const
{
    return d_textInputState.flags;
}

bool RenderWebView::CanComposeInline() const
{
    return d_textInputState.can_compose_inline;
}

gfx::Rect RenderWebView::GetCaretBounds() const
{
    auto bounds = gfx::RectBetweenSelectionBounds(
        d_selectionAnchor, d_selectionFocus).ToRECT();

    MapWindowPoints(
        d_hwnd.get(),
        NULL,
        (LPPOINT)&bounds,
        2);

    return gfx::Rect(bounds);
}

bool RenderWebView::GetCompositionCharacterBounds(
    uint32_t index,
    gfx::Rect* rect) const
{
    if (index >= d_compositionCharacterBounds.size()) {
        return false;
    }

    auto bounds = d_compositionCharacterBounds[index].ToRECT();

    MapWindowPoints(
        d_hwnd.get(),
        NULL,
        (LPPOINT)&bounds,
        2);

    *rect = gfx::Rect(bounds);

    return true;
}

bool RenderWebView::HasCompositionText() const
{
    return d_hasCompositionText;
}

ui::TextInputClient::FocusReason RenderWebView::GetFocusReason() const
{
    return ui::TextInputClient::FOCUS_REASON_NONE;
}

bool RenderWebView::GetTextRange(gfx::Range* range) const
{
    range->set_start(d_selectionTextOffset);
    range->set_end(d_selectionTextOffset + d_selectionText.length());
    return true;
}

bool RenderWebView::GetCompositionTextRange(gfx::Range* range) const
{
    NOTIMPLEMENTED();
    return false;
}

bool RenderWebView::GetEditableSelectionRange(gfx::Range* range) const
{
    range->set_start(d_selectionRange.start());
    range->set_end(d_selectionRange.end());
    return true;
}

bool RenderWebView::SetEditableSelectionRange(const gfx::Range& range)
{
    NOTIMPLEMENTED();
    return false;
}

bool RenderWebView::DeleteRange(const gfx::Range& range)
{
    NOTIMPLEMENTED();
    return false;
}

bool RenderWebView::GetTextFromRange(
    const gfx::Range& range,
    base::string16* text) const
{
    gfx::Range selection_text_range(
        d_selectionTextOffset,
        d_selectionTextOffset + d_selectionText.length());

    if (!selection_text_range.Contains(range)) {
        text->clear();
        return false;
    }

    if (selection_text_range.EqualsIgnoringDirection(range)) {
        *text = d_selectionText;
    }
    else {
        *text = d_selectionText.substr(
            range.GetMin() - d_selectionTextOffset,
            range.length());
    }

    return true;
}

void RenderWebView::OnInputMethodChanged()
{
}

bool RenderWebView::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction)
{
    dispatchToRenderWidget(
        WidgetMsg_SetTextDirection(d_renderWidgetRoutingId,
            direction == base::i18n::RIGHT_TO_LEFT?
                blink::kWebTextDirectionRightToLeft :
                blink::kWebTextDirectionLeftToRight));

    return true;
}

void RenderWebView::ExtendSelectionAndDelete(size_t before, size_t after)
{
    //TODO
}

void RenderWebView::EnsureCaretNotInRect(const gfx::Rect& rect)
{
    //TODO
}

bool RenderWebView::IsTextEditCommandEnabled(ui::TextEditCommand command) const
{
    return false;
}

void RenderWebView::SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command)
{
}

ukm::SourceId RenderWebView::GetClientSourceForMetrics() const
{
    return ukm::SourceId();
}

bool RenderWebView::ShouldDoLearning()
{
    return false;
}

bool RenderWebView::SetCompositionFromExistingText(const gfx::Range& range,
                                    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans)
{
    return false;
}

// DragDropDelegate overrides:
void RenderWebView::DragTargetEnter(
    const std::vector<content::DropData::Metadata>& drag_data_metadata,
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    blink::WebDragOperationsMask ops_allowed,
    int key_modifiers)
{
    dispatchToRenderWidget(
        DragMsg_TargetDragEnter(d_renderWidgetRoutingId,
            drag_data_metadata,
            client_pt, screen_pt,
            ops_allowed,
            key_modifiers));
}

void RenderWebView::DragTargetOver(
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    blink::WebDragOperationsMask ops_allowed,
    int key_modifiers)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderWidget(
        DragMsg_TargetDragOver(d_renderWidgetRoutingId,
            client_pt, screen_pt,
            ops_allowed,
            key_modifiers));
}

void RenderWebView::DragTargetLeave()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderWidget(
        DragMsg_TargetDragLeave(d_renderWidgetRoutingId,
            gfx::PointF(), gfx::PointF()));
}

void RenderWebView::DragTargetDrop(
    const content::DropData& drop_data,
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    int key_modifiers)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderWidget(
        DragMsg_TargetDrop(d_renderWidgetRoutingId,
            drop_data,
            client_pt, screen_pt,
            key_modifiers));
}

void RenderWebView::DragSourceEnded(
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    blink::WebDragOperation drag_operation)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderWidget(
        DragMsg_SourceEnded(d_renderWidgetRoutingId,
            client_pt, screen_pt,
            drag_operation));
}

void RenderWebView::DragSourceSystemEnded()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderWidget(
        DragMsg_SourceSystemDragEnded(d_renderWidgetRoutingId));
}

// IPC message handlers:
void RenderWebView::OnLockMouse(
    bool user_gesture,
    bool privileged)
{
    if (GetCapture() != d_hwnd.get()) {
        SetCapture(d_hwnd.get());
        d_mouseLocked = true;
    }

    dispatchToRenderWidget(
        WidgetMsg_LockMouse_ACK(d_renderViewRoutingId,
            GetCapture() == d_hwnd.get()));
}

void RenderWebView::OnUnlockMouse()
{
    if (GetCapture() != d_hwnd.get()) {
        ReleaseCapture();
        d_mouseLocked = false;
    }
}

void RenderWebView::OnSetCursor(const content::WebCursor& cursor)
{
    if (!(d_currentCursor == cursor)) {
        d_currentCursor = cursor;

        HCURSOR platformCursor = NULL;

        if (d_currentCursor.info().type != blink::WebCursorInfo::kTypeCustom) {
            auto native_cursor = d_currentCursor.GetNativeCursor();
            d_cursorLoader->SetPlatformCursor(&native_cursor);

            platformCursor = native_cursor.platform();
        }
        else {
            ui::Cursor uiCursor(ui::CursorType::kCustom);
            SkBitmap bitmap;
            gfx::Point hotspot;
            float scale_factor;
            d_currentCursor.CreateScaledBitmapAndHotspotFromCustomData(
                &bitmap, &hotspot, &scale_factor);
            uiCursor.set_custom_bitmap(bitmap);
            uiCursor.set_custom_hotspot(hotspot);
            uiCursor.set_device_scale_factor(scale_factor);

            platformCursor = d_currentCursor.GetPlatformCursor(uiCursor);
        }

        setPlatformCursor(platformCursor);
    }
}

void RenderWebView::OnSelectionBoundsChanged(
    const WidgetHostMsg_SelectionBounds_Params& params)
{
    gfx::SelectionBound anchor_bound, focus_bound;
    anchor_bound.SetEdge(
        gfx::PointF(params.anchor_rect.origin()),
        gfx::PointF(params.anchor_rect.bottom_left()));
    focus_bound.SetEdge(
        gfx::PointF(params.focus_rect.origin()),
        gfx::PointF(params.focus_rect.bottom_left()));

    if (params.anchor_rect == params.focus_rect) {
        anchor_bound.set_type(gfx::SelectionBound::CENTER);
        focus_bound.set_type(gfx::SelectionBound::CENTER);
    } else {
        // Whether text is LTR at the anchor handle.
        bool anchor_LTR = params.anchor_dir == blink::kWebTextDirectionLeftToRight;
        // Whether text is LTR at the focus handle.
        bool focus_LTR = params.focus_dir == blink::kWebTextDirectionLeftToRight;

        if ((params.is_anchor_first && anchor_LTR) ||
            (!params.is_anchor_first && !anchor_LTR)) {
            anchor_bound.set_type(gfx::SelectionBound::LEFT);
        }
        else {
            anchor_bound.set_type(gfx::SelectionBound::RIGHT);
        }

        if ((params.is_anchor_first && focus_LTR) ||
            (!params.is_anchor_first && !focus_LTR)) {
            focus_bound.set_type(gfx::SelectionBound::RIGHT);
        }
        else {
            focus_bound.set_type(gfx::SelectionBound::LEFT);
        }
    }

    if (anchor_bound == d_selectionAnchor && focus_bound == d_selectionFocus)
        return;

    d_selectionAnchor = anchor_bound;
    d_selectionFocus = focus_bound;

    d_inputMethod->OnCaretBoundsChanged(this);
}

void RenderWebView::OnSelectionChanged(
    const base::string16& text,
    uint32_t offset,
    const gfx::Range& range)
{
    d_selectionText = text;
    d_selectionTextOffset = offset;
    d_selectionRange.set_start(range.start());
    d_selectionRange.set_end(range.end());
}

void RenderWebView::OnTextInputStateChanged(
    const content::TextInputState& text_input_state)
{
    auto changed =
        (d_textInputState.type               != text_input_state.type)  ||
        (d_textInputState.mode               != text_input_state.mode)  ||
        (d_textInputState.flags              != text_input_state.flags) ||
        (d_textInputState.can_compose_inline != text_input_state.can_compose_inline);

    d_textInputState = text_input_state;

    if (changed) {
        d_inputMethod->OnTextInputTypeChanged(this);
    }

    if (d_textInputState.type != ui::TEXT_INPUT_TYPE_NONE) {
        d_widgetInputHandler->
            RequestCompositionUpdates(
                false, true);
    }
    else {
        d_widgetInputHandler->
            RequestCompositionUpdates(
                false, false);
    }
}

void RenderWebView::OnStartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask operations_allowed,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const content::DragEventSourceInfo& event_info)
{
    base::MessageLoopCurrent::Get()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&RenderWebView::onStartDraggingImpl,
            base::Unretained(this),
            drop_data, operations_allowed, bitmap, bitmap_offset_in_dip, event_info));
}

void RenderWebView::OnUpdateDragCursor(
    blink::WebDragOperation drag_operation)
{
    d_dragDrop->UpdateDragCursor(drag_operation);
}

// Used only for renderer-driven popups:
void RenderWebView::OnShowWidget(
    int routing_id, const gfx::Rect initial_rect)
{
    new RenderWebView(d_profile, routing_id, initial_rect);
}

void RenderWebView::OnClose()
{
    this->destroy();
}

// Native tooltips:
void RenderWebView::OnSetTooltipText(
    const base::string16& tooltip_text,
    blink::WebTextDirection text_direction_hint)
{
    d_tooltipText = tooltip_text;
    updateTooltip();
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
// Rubber band selection:
void RenderWebView::OnHideRubberbandRect()
{
    d_rubberbandOutline.reset();
}

void RenderWebView::OnSetRubberbandRect(const gfx::Rect& rect)
{
    if (!d_rubberbandOutline.get()) {
        d_rubberbandOutline.reset(new ui::RubberbandOutline());
    }

    d_rubberbandOutline->SetRect(d_hwnd.get(), rect.ToRECT());
}
#endif

}  // close namespace blpwtk2

// vim: ts=4 et


