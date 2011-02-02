// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_win.h"

#include "base/scoped_ptr.h"
#include "gfx/canvas_skia.h"
#include "gfx/path.h"
#include "gfx/native_theme_win.h"
#include "ui/base/system_monitor/system_monitor.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_listener.h"
#include "ui/views/widget/widget.h"

namespace ui {
namespace internal {

namespace {

// Called from NativeWidgetWin::Paint() to asynchronously redraw child windows.
BOOL CALLBACK EnumChildProcForRedraw(HWND hwnd, LPARAM lparam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  gfx::Rect invalid_rect = *reinterpret_cast<gfx::Rect*>(lparam);

  RECT window_rect;
  GetWindowRect(hwnd, &window_rect);
  invalid_rect.Offset(-window_rect.left, -window_rect.top);

  int flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
  if (process_id == GetCurrentProcessId())
    flags |= RDW_UPDATENOW;
  RedrawWindow(hwnd, &invalid_rect.ToRECT(), NULL, flags);
  return TRUE;
}

// Links the HWND to its Widget.
const char* const kNativeWidgetKey = "__VIEWS_NATIVE_WIDGET__";

// A custom MSAA object id used to determine if a screen reader is actively
// listening for MSAA events.
const int kMSAAObjectID = 1;

}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, public:

NativeWidgetWin::NativeWidgetWin(NativeWidgetListener* listener)
    : listener_(listener),
      active_mouse_tracking_flags_(0),
      has_capture_(false) {
}

NativeWidgetWin::~NativeWidgetWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, NativeWidget implementation:

void NativeWidgetWin::InitWithNativeViewParent(gfx::NativeView parent,
                                               const gfx::Rect& bounds) {
  WindowImpl::Init(parent, bounds);
}

void NativeWidgetWin::InitWithWidgetParent(Widget* parent,
                                           const gfx::Rect& bounds) {
  InitWithNativeViewParent(parent->native_widget()->GetNativeView(), bounds);
}

void NativeWidgetWin::InitWithViewParent(View* parent,
                                         const gfx::Rect& bounds) {
  InitWithWidgetParent(parent->GetWidget(), bounds);
}

void NativeWidgetWin::SetNativeWindowProperty(const char* name, void* value) {
  // Remove the existing property (if any).
  for (ViewProps::iterator i = props_.begin(); i != props_.end(); ++i) {
    if ((*i)->Key() == name) {
      props_.erase(i);
      break;
    }
  }

  if (value)
    props_.push_back(new ViewProp(hwnd(), name, value));
}

void* NativeWidgetWin::GetNativeWindowProperty(const char* name) const {
  return ViewProp::GetValue(hwnd(), name);
}

gfx::Rect NativeWidgetWin::GetWindowScreenBounds() const {
  RECT r;
  GetWindowRect(hwnd(), &r);
  return gfx::Rect(r);
}

gfx::Rect NativeWidgetWin::GetClientAreaScreenBounds() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  POINT point = { r.left, r.top };
  ClientToScreen(hwnd(), &point);
  return gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

void NativeWidgetWin::SetBounds(const gfx::Rect& bounds) {
  SetWindowPos(hwnd(), NULL, bounds.x(), bounds.y(), bounds.width(),
               bounds.height(), SWP_NOACTIVATE | SWP_NOZORDER);
}

void NativeWidgetWin::SetShape(const gfx::Path& shape) {
  SetWindowRgn(hwnd(), shape.CreateNativeRegion(), TRUE);
}

gfx::NativeView NativeWidgetWin::GetNativeView() const {
  return hwnd();
}

void NativeWidgetWin::Show() {
  if (IsWindow(hwnd()))
    ShowWindow(hwnd(), SW_SHOWNOACTIVATE);
  // TODO(beng): move to windowposchanging to trap visibility changes instead.
  if (IsLayeredWindow())
    Invalidate();
}

void NativeWidgetWin::Hide() {
  if (IsWindow(hwnd())) {
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(hwnd(), NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

void NativeWidgetWin::Close() {
  DestroyWindow(hwnd());
}

void NativeWidgetWin::MoveAbove(NativeWidget* other) {
  SetWindowPos(hwnd(), other->GetNativeView(), 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void NativeWidgetWin::SetAlwaysOnTop(bool always_on_top) {
  DWORD style = always_on_top ? window_ex_style() | WS_EX_TOPMOST
                              : window_ex_style() & ~WS_EX_TOPMOST;
  set_window_ex_style(style);
  SetWindowLong(hwnd(), GWL_EXSTYLE, window_ex_style());
}

bool NativeWidgetWin::IsVisible() const {
  return !!IsWindowVisible(hwnd());
}

bool NativeWidgetWin::IsActive() const {
  WINDOWINFO info;
  return ::GetWindowInfo(hwnd(), &info) &&
      ((info.dwWindowStatus & WS_ACTIVECAPTION) != 0);
}

void NativeWidgetWin::SetMouseCapture() {
  SetCapture(hwnd());
  has_capture_ = true;
}

void NativeWidgetWin::ReleaseMouseCapture() {
  ReleaseCapture();
  has_capture_ = false;
}

bool NativeWidgetWin::HasMouseCapture() const {
  return has_capture_;
}

bool NativeWidgetWin::ShouldReleaseCaptureOnMouseReleased() const {
  return true;
}

void NativeWidgetWin::Invalidate() {
  ::InvalidateRect(hwnd(), NULL, FALSE);
}

void NativeWidgetWin::InvalidateRect(const gfx::Rect& invalid_rect) {
  // InvalidateRect() expects client coordinates.
  RECT r = invalid_rect.ToRECT();
  ::InvalidateRect(hwnd(), &r, FALSE);
}

void NativeWidgetWin::Paint() {
  RECT r;
  GetUpdateRect(hwnd(), &r, FALSE);
  if (!IsRectEmpty(&r)) {
    // TODO(beng): WS_EX_TRANSPARENT windows (see WidgetWin::opaque_)
    // Paint child windows that are in a different process asynchronously.
    // This prevents a hang in other processes from blocking this process.

    // Calculate the invalid rect in screen coordinates before the first
    // RedrawWindow() call to the parent HWND, since that will empty update_rect
    // (which comes from a member variable) in the OnPaint call.
    gfx::Rect screen_rect = GetWindowScreenBounds();
    gfx::Rect invalid_screen_rect(r);
    invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());

    RedrawWindow(hwnd(), &r, NULL,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);

    LPARAM lparam = reinterpret_cast<LPARAM>(&invalid_screen_rect);
    EnumChildWindows(hwnd(), EnumChildProcForRedraw, lparam);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidetWin, MessageLoopForUI::Observer implementation

void NativeWidgetWin::WillProcessMessage(const MSG& msg) {
}

void NativeWidgetWin::DidProcessMessage(const MSG& msg) {
  // We need to add ourselves as a message loop observer so that we can repaint
  // aggressively if the contents of our window become invalid. Unfortunately
  // WM_PAINT messages are starved and we get flickery redrawing when resizing
  // if we do not do this.
  Paint();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, message handlers:

void NativeWidgetWin::OnActivate(UINT action, BOOL minimized, HWND window) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnActivateApp(BOOL active, DWORD thread_id) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnAppCommand(HWND window, short app_command,
                                      WORD device, int keystate) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnCancelMode() {
}

void NativeWidgetWin::OnCaptureChanged(HWND hwnd) {
  has_capture_ = false;
  listener_->OnMouseCaptureLost();
}

void NativeWidgetWin::OnClose() {
  listener_->OnClose();
}

void NativeWidgetWin::OnCommand(UINT notification_code, int command_id,
                                HWND window) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnCreate(CREATESTRUCT* create_struct) {
  SetNativeWindowProperty(kNativeWidgetKey, this);
  MessageLoopForUI::current()->AddObserver(this);
  return 0;
}

void NativeWidgetWin::OnDestroy() {
  // TODO(beng): drop_target_
  props_.reset();
}

void NativeWidgetWin::OnDisplayChange(UINT bits_per_pixel, CSize screen_size) {
  listener_->OnDisplayChanged();
}

LRESULT NativeWidgetWin::OnDwmCompositionChanged(UINT message,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnEndSession(BOOL ending, UINT logoff) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnEnterSizeMove() {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnEraseBkgnd(HDC dc) {
  // This is needed for magical win32 flicker ju-ju
  return 1;
}

void NativeWidgetWin::OnExitMenuLoop(BOOL is_track_popup_menu) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnExitSizeMove() {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnGetObject(UINT message, WPARAM w_param,
                                     LPARAM l_param) {
  return static_cast<LRESULT>(0L);
}

void NativeWidgetWin::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnHScroll(int scroll_type, short position,
                                HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnInitMenu(HMENU menu) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnInitMenuPopup(HMENU menu, UINT position,
                                      BOOL is_system_menu) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnKeyDown(UINT message, WPARAM w_param,
                                   LPARAM l_param) {
  MSG msg;
  MakeMSG(&msg, message, w_param, l_param);
  SetMsgHandled(listener_->OnKeyEvent(KeyEvent(msg)));
  return 0;
}

LRESULT NativeWidgetWin::OnKeyUp(UINT message, WPARAM w_param, LPARAM l_param) {
  MSG msg;
  MakeMSG(&msg, message, w_param, l_param);
  SetMsgHandled(listener_->OnKeyEvent(KeyEvent(msg)));
  return 0;
}

void NativeWidgetWin::OnKillFocus(HWND focused_window) {
  // TODO(beng): focus
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnMouseActivate(HWND window, UINT hittest_code,
                                         UINT message) {
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

LRESULT NativeWidgetWin::OnMouseLeave(UINT message, WPARAM w_param,
                                      LPARAM l_param) {
  // TODO(beng): tooltip
  MSG msg;
  MakeMSG(&msg, message, w_param, l_param);
  SetMsgHandled(listener_->OnMouseEvent(MouseEvent(msg)));

  // Reset our tracking flag so that future mouse movement over this WidgetWin
  // results in a new tracking session.
  active_mouse_tracking_flags_ = 0;

  return 0;
}

void NativeWidgetWin::OnMove(const CPoint& point) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnMoving(UINT param, LPRECT new_bounds) {
}

LRESULT NativeWidgetWin::OnMouseRange(UINT message, WPARAM w_param,
                                      LPARAM l_param) {
  // TODO(beng): tooltips
  ProcessMouseRange(message, w_param, l_param, false);
  return 0;
}

LRESULT NativeWidgetWin::OnNCActivate(BOOL active) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCHitTest(UINT message, WPARAM w_param,
                                     LPARAM l_param) {
  LRESULT lr = DefWindowProc(hwnd(), message, w_param, l_param);
  return lr;
}

LRESULT NativeWidgetWin::OnNCMouseRange(UINT message, WPARAM w_param,
                                        LPARAM l_param) {
  bool processed = ProcessMouseRange(message, w_param, l_param, true);
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnNCPaint(HRGN rgn) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnNCUAHDrawCaption(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNCUAHDrawFrame(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnNotify(int w_param, NMHDR* l_param) {
  // TODO(beng): tooltips
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnPaint(HDC dc) {
  if (IsLayeredWindow()) {
    // We need to clip to the dirty rect ourselves.
    window_contents_->save(SkCanvas::kClip_SaveFlag);
    RECT r;
    GetUpdateRect(hwnd(), &r, FALSE);
    window_contents_->ClipRectInt(r.left, r.top, r.right - r.left,
                                  r.bottom - r.top);
    listener_->OnPaint(window_contents_.get());
    window_contents_->restore();

    RECT wr;
    GetWindowRect(hwnd(), &wr);
    SIZE size = {wr.right - wr.left, wr.bottom - wr.top};
    POINT position = {wr.left, wr.top};
    HDC dib_dc = window_contents_->getTopPlatformDevice().getBitmapDC();
    POINT zero = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 125, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd(), NULL, &position, &size, dib_dc, &zero,
                        RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
  } else {
    scoped_ptr<gfx::CanvasPaint> canvas(
        gfx::CanvasPaint::CreateCanvasPaint(hwnd()));
    listener_->OnPaint(canvas->AsCanvas());
  }
}

LRESULT NativeWidgetWin::OnPowerBroadcast(DWORD power_event, DWORD data) {
  SystemMonitor* monitor = SystemMonitor::Get();
  if (monitor)
    monitor->ProcessWmPowerBroadcastMessage(power_event);
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnReflectedMessage(UINT message, WPARAM w_param,
                                            LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnSetFocus(HWND focused_window) {
  // TODO(beng): focus
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnSetIcon(UINT size_type, HICON new_icon) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnSetText(const wchar_t* text) {
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnSettingChange(UINT flags, const wchar_t* section) {
  if (flags == SPI_SETWORKAREA)
    listener_->OnWorkAreaChanged();
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnSize(UINT param, const CSize& size) {
  gfx::Size s(size.cx, size.cy);
  listener_->OnSizeChanged(s);
  if (IsLayeredWindow()) {
    window_contents_.reset(
        new gfx::CanvasSkia(s.width(), s.height(), false));
  }
}

void NativeWidgetWin::OnSysCommand(UINT notification_code, CPoint click) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnThemeChanged() {
  gfx::NativeTheme::instance()->CloseHandles();
}

void NativeWidgetWin::OnVScroll(int scroll_type, short position,
                                HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnFinalMessage(HWND window) {
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, WindowImpl overrides:

HICON NativeWidgetWin::GetDefaultWindowIcon() const {
  return NULL;
}

LRESULT NativeWidgetWin::OnWndProc(UINT message, WPARAM w_param,
                                   LPARAM l_param) {
  LRESULT result = 0;

  // Otherwise we handle everything else.
  if (!ProcessWindowMessage(hwnd(), message, w_param, l_param, result))
    result = DefWindowProc(hwnd(), message, w_param, l_param);
  if (message == WM_NCDESTROY) {
    MessageLoopForUI::current()->RemoveObserver(this);
    OnFinalMessage(hwnd());
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, private:

void NativeWidgetWin::TrackMouseEvents(DWORD mouse_tracking_flags) {
  // Begin tracking mouse events for this HWND so that we get WM_MOUSELEAVE
  // when the user moves the mouse outside this HWND's bounds.
  if (active_mouse_tracking_flags_ == 0 || mouse_tracking_flags & TME_CANCEL) {
    if (mouse_tracking_flags & TME_CANCEL) {
      // We're about to cancel active mouse tracking, so empty out the stored
      // state.
      active_mouse_tracking_flags_ = 0;
    } else {
      active_mouse_tracking_flags_ = mouse_tracking_flags;
    }

    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = mouse_tracking_flags;
    tme.hwndTrack = hwnd();
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  } else if (mouse_tracking_flags != active_mouse_tracking_flags_) {
    TrackMouseEvents(active_mouse_tracking_flags_ | TME_CANCEL);
    TrackMouseEvents(mouse_tracking_flags);
  }
}

bool NativeWidgetWin::ProcessMouseRange(UINT message, WPARAM w_param,
                                        LPARAM l_param, bool non_client) {
  MSG msg;
  MakeMSG(&msg, message, w_param, l_param);
  if (message == WM_MOUSEWHEEL) {
    // Reroute the mouse-wheel to the window under the mouse pointer if
    // applicable.
    // TODO(beng):
    //if (views::RerouteMouseWheel(hwnd(), w_param, l_param))
    //  return 0;
    return listener_->OnMouseWheelEvent(MouseWheelEvent(msg));
  }
  // Windows only fires WM_MOUSELEAVE events if the application begins
  // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
  // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
  if (!has_capture_)
    TrackMouseEvents(non_client ? TME_NONCLIENT | TME_LEAVE : TME_LEAVE);
  return listener_->OnMouseEvent(MouseEvent(msg));
}

void NativeWidgetWin::MakeMSG(MSG* msg, UINT message, WPARAM w_param,
                              LPARAM l_param) const {
  msg->hwnd = hwnd();
  msg->message = message;
  msg->wParam = w_param;
  msg->lParam = l_param;
  msg->time = 0;
  msg->pt.x = msg->pt.y = 0;
}

void NativeWidgetWin::CloseNow() {
  DestroyWindow(hwnd());
}

bool NativeWidgetWin::IsLayeredWindow() const {
  return !!(window_ex_style() & WS_EX_LAYERED);
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// NativeWidget, public:

// static
NativeWidget* NativeWidget::CreateNativeWidget(
    internal::NativeWidgetListener* listener) {
  return new internal::NativeWidgetWin(listener);
}

// static
NativeWidget* NativeWidget::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  if (!WindowImpl::IsWindowImpl(native_view))
    return NULL;
  return reinterpret_cast<internal::NativeWidgetWin*>(
      ViewProp::GetValue(native_view, internal::kNativeWidgetKey));
}

// static
NativeWidget* NativeWidget::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return GetNativeWidgetForNativeView(native_window);
}

// static
NativeWidget* NativeWidget::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  // First, check if the top-level window is a Widget.
  HWND root = ::GetAncestor(native_view, GA_ROOT);
  if (!root)
    return NULL;

  NativeWidget* widget = GetNativeWidgetForNativeView(root);
  if (widget)
    return widget;

  // Second, try to locate the last Widget window in the parent hierarchy.
  HWND parent_hwnd = native_view;
  NativeWidget* parent_widget;
  do {
    parent_widget = GetNativeWidgetForNativeView(parent_hwnd);
    if (parent_widget) {
      widget = parent_widget;
      parent_hwnd = ::GetAncestor(parent_hwnd, GA_PARENT);
    }
  } while (parent_hwnd != NULL && parent_widget != NULL);

  return widget;
}

}  // namespace ui
