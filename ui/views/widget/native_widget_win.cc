// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_win.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>

#include "base/bind.h"
#include "base/string_util.h"
#include "base/system_monitor/system_monitor.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/theme_provider.h"
#include "ui/base/view_prop.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/native_theme_win.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/accessibility/native_view_accessibility_win.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/ime/input_method_win.h"
#include "ui/views/widget/aero_tooltip_manager.h"
#include "ui/views/widget/aero_tooltip_manager.h"
#include "ui/views/widget/child_window_message_processor.h"
#include "ui/views/widget/child_window_message_processor.h"
#include "ui/views/widget/drop_target_win.h"
#include "ui/views/widget/drop_target_win.h"
#include "ui/views/widget/monitor_win.h"
#include "ui/views/widget/monitor_win.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/native_frame_view.h"
#include "views/controls/native_control_win.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/views_delegate.h"

#pragma comment(lib, "dwmapi.lib")

using ui::ViewProp;

namespace views {

namespace {

// Get the source HWND of the specified message. Depending on the message, the
// source HWND is encoded in either the WPARAM or the LPARAM value.
HWND GetControlHWNDForMessage(UINT message, WPARAM w_param, LPARAM l_param) {
  // Each of the following messages can be sent by a child HWND and must be
  // forwarded to its associated NativeControlWin for handling.
  switch (message) {
    case WM_NOTIFY:
      return reinterpret_cast<NMHDR*>(l_param)->hwndFrom;
    case WM_COMMAND:
      return reinterpret_cast<HWND>(l_param);
    case WM_CONTEXTMENU:
      return reinterpret_cast<HWND>(w_param);
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
      return reinterpret_cast<HWND>(l_param);
  }
  return NULL;
}

// Some messages may be sent to us by a child HWND. If this is the case, this
// function will forward those messages on to the object associated with the
// source HWND and return true, in which case the window procedure must not do
// any further processing of the message. If there is no associated
// ChildWindowMessageProcessor, the return value will be false and the WndProc
// can continue processing the message normally.  |l_result| contains the result
// of the message processing by the control and must be returned by the WndProc
// if the return value is true.
bool ProcessChildWindowMessage(UINT message,
                               WPARAM w_param,
                               LPARAM l_param,
                               LRESULT* l_result) {
  *l_result = 0;

  HWND control_hwnd = GetControlHWNDForMessage(message, w_param, l_param);
  if (IsWindow(control_hwnd)) {
    ChildWindowMessageProcessor* processor =
        ChildWindowMessageProcessor::Get(control_hwnd);
    if (processor)
      return processor->ProcessMessage(message, w_param, l_param, l_result);
  }

  return false;
}

// Enumeration callback for NativeWidget::GetAllChildWidgets(). Called for each
// child HWND beneath the original HWND.
BOOL CALLBACK EnumerateChildWindowsForNativeWidgets(HWND hwnd, LPARAM l_param) {
  Widget* widget = Widget::GetWidgetForNativeView(hwnd);
  if (widget) {
    Widget::Widgets* widgets = reinterpret_cast<Widget::Widgets*>(l_param);
    widgets->insert(widget);
  }
  return TRUE;
}

// Returns true if the WINDOWPOS data provided indicates the client area of
// the window may have changed size. This can be caused by the window being
// resized or its frame changing.
bool DidClientAreaSizeChange(const WINDOWPOS* window_pos) {
  return !(window_pos->flags & SWP_NOSIZE) ||
         window_pos->flags & SWP_FRAMECHANGED;
}

// Callback used to notify child windows that the top level window received a
// DWMCompositionChanged message.
BOOL CALLBACK SendDwmCompositionChanged(HWND window, LPARAM param) {
  SendMessage(window, WM_DWMCOMPOSITIONCHANGED, 0, 0);
  return TRUE;
}

// Tells the window its frame (non-client area) has changed.
void SendFrameChanged(HWND window) {
  SetWindowPos(window, NULL, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS |
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION |
      SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOZORDER);
}

// Enables or disables the menu item for the specified command and menu.
void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

BOOL CALLBACK EnumChildWindowsForRedraw(HWND hwnd, LPARAM lparam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  int flags = RDW_INVALIDATE | RDW_NOCHILDREN | RDW_FRAME;
  if (process_id == GetCurrentProcessId())
    flags |= RDW_UPDATENOW;
  RedrawWindow(hwnd, NULL, NULL, flags);
  return TRUE;
}

// See comments in OnNCPaint() for details of this struct.
struct ClipState {
  // The window being painted.
  HWND parent;

  // DC painting to.
  HDC dc;

  // Origin of the window in terms of the screen.
  int x;
  int y;
};

// See comments in OnNCPaint() for details of this function.
static BOOL CALLBACK ClipDCToChild(HWND window, LPARAM param) {
  ClipState* clip_state = reinterpret_cast<ClipState*>(param);
  if (GetParent(window) == clip_state->parent && IsWindowVisible(window)) {
    RECT bounds;
    GetWindowRect(window, &bounds);
    ExcludeClipRect(clip_state->dc,
      bounds.left - clip_state->x,
      bounds.top - clip_state->y,
      bounds.right - clip_state->x,
      bounds.bottom - clip_state->y);
  }
  return TRUE;
}

// The thickness of an auto-hide taskbar in pixels.
static const int kAutoHideTaskbarThicknessPx = 2;

bool GetMonitorAndRects(const RECT& rect,
                        HMONITOR* monitor,
                        gfx::Rect* monitor_rect,
                        gfx::Rect* work_area) {
  DCHECK(monitor);
  DCHECK(monitor_rect);
  DCHECK(work_area);
  *monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
  if (!*monitor)
    return false;
  MONITORINFO monitor_info = { 0 };
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(*monitor, &monitor_info);
  *monitor_rect = monitor_info.rcMonitor;
  *work_area = monitor_info.rcWork;
  return true;
}

// Links the HWND to its NativeWidget.
const char* const kNativeWidgetKey = "__VIEWS_NATIVE_WIDGET__";

// A custom MSAA object id used to determine if a screen reader is actively
// listening for MSAA events.
const int kCustomObjectID = 1;

const int kDragFrameWindowAlpha = 200;

}  // namespace

// static
bool NativeWidgetWin::screen_reader_active_ = false;

// A scoping class that prevents a window from being able to redraw in response
// to invalidations that may occur within it for the lifetime of the object.
//
// Why would we want such a thing? Well, it turns out Windows has some
// "unorthodox" behavior when it comes to painting its non-client areas.
// Occasionally, Windows will paint portions of the default non-client area
// right over the top of the custom frame. This is not simply fixed by handling
// WM_NCPAINT/WM_PAINT, with some investigation it turns out that this
// rendering is being done *inside* the default implementation of some message
// handlers and functions:
//  . WM_SETTEXT
//  . WM_SETICON
//  . WM_NCLBUTTONDOWN
//  . EnableMenuItem, called from our WM_INITMENU handler
// The solution is to handle these messages and call DefWindowProc ourselves,
// but prevent the window from being able to update itself for the duration of
// the call. We do this with this class, which automatically calls its
// associated Window's lock and unlock functions as it is created and destroyed.
// See documentation in those methods for the technique used.
//
// The lock only has an effect if the window was visible upon lock creation, as
// it doesn't guard against direct visiblility changes, and multiple locks may
// exist simultaneously to handle certain nested Windows messages.
//
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class NativeWidgetWin::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(NativeWidgetWin* widget)
    : widget_(widget),
      native_view_(widget_->GetNativeView()),
      was_visible_(widget_->IsVisible()),
      cancel_unlock_(false) {
    if (was_visible_ && ::IsWindow(native_view_))
      widget_->LockUpdates();
  }

  ~ScopedRedrawLock() {
    if (!cancel_unlock_ && was_visible_ && ::IsWindow(native_view_))
      widget_->UnlockUpdates();
  }

  // Cancel the unlock operation, call this if the Widget is being destroyed.
  void CancelUnlockOperation() { cancel_unlock_ = true; }

 private:
  // The widget having its style changed.
  NativeWidgetWin* widget_;
  // The widget's native view, cached to avoid action after window destruction.
  gfx::NativeView native_view_;
  // Records the widget visibility at the time of creation.
  bool was_visible_;
  // A flag indicating that the unlock operation was canceled.
  bool cancel_unlock_;
};

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, public:

NativeWidgetWin::NativeWidgetWin(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      active_mouse_tracking_flags_(0),
      use_layered_buffer_(false),
      layered_alpha_(255),
      ALLOW_THIS_IN_INITIALIZER_LIST(paint_layered_window_factory_(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      can_update_layered_window_(true),
      restore_focus_when_enabled_(false),
      accessibility_view_events_index_(-1),
      accessibility_view_events_(kMaxAccessibilityViewEvents),
      previous_cursor_(NULL),
      fullscreen_(false),
      force_hidden_count_(0),
      lock_updates_count_(0),
      saved_window_style_(0),
      ignore_window_pos_changes_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(ignore_pos_changes_factory_(this)),
      last_monitor_(NULL),
      is_right_mouse_pressed_on_caption_(false),
      restored_enabled_(false),
      destroyed_(NULL),
      has_non_client_view_(false) {
}

NativeWidgetWin::~NativeWidgetWin() {
  if (destroyed_ != NULL)
    *destroyed_ = true;

  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

// static
bool NativeWidgetWin::IsAeroGlassEnabled() {
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
  // If composition is not enabled, we behave like on XP.
  BOOL enabled = FALSE;
  return SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && enabled;
}

// static
gfx::Font NativeWidgetWin::GetWindowTitleFont() {
  NONCLIENTMETRICS ncm;
  base::win::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  base::win::ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
}

void NativeWidgetWin::Show(int show_state) {
  ShowWindow(show_state);
  // When launched from certain programs like bash and Windows Live Messenger,
  // show_state is set to SW_HIDE, so we need to correct that condition. We
  // don't just change show_state to SW_SHOWNORMAL because MSDN says we must
  // always first call ShowWindow with the specified value from STARTUPINFO,
  // otherwise all future ShowWindow calls will be ignored (!!#@@#!). Instead,
  // we call ShowWindow again in this case.
  if (show_state == SW_HIDE) {
    show_state = SW_SHOWNORMAL;
    ShowWindow(show_state);
  }

  // We need to explicitly activate the window if we've been shown with a state
  // that should activate, because if we're opened from a desktop shortcut while
  // an existing window is already running it doesn't seem to be enough to use
  // one of these flags to activate the window.
  if (show_state == SW_SHOWNORMAL || show_state == SW_SHOWMAXIMIZED)
    Activate();

  SetInitialFocus();
}

View* NativeWidgetWin::GetAccessibilityViewEventAt(int id) {
  // Convert from MSAA child id.
  id = -(id + 1);
  DCHECK(id >= 0 && id < kMaxAccessibilityViewEvents);
  return accessibility_view_events_[id];
}

int NativeWidgetWin::AddAccessibilityViewEvent(View* view) {
  accessibility_view_events_index_ =
      (accessibility_view_events_index_ + 1) % kMaxAccessibilityViewEvents;
  accessibility_view_events_[accessibility_view_events_index_] = view;

  // Convert to MSAA child id.
  return -(accessibility_view_events_index_ + 1);
}

void NativeWidgetWin::ClearAccessibilityViewEvent(View* view) {
  for (std::vector<View*>::iterator it = accessibility_view_events_.begin();
      it != accessibility_view_events_.end();
      ++it) {
    if (*it == view)
      *it = NULL;
  }
}

void NativeWidgetWin::PushForceHidden() {
  if (force_hidden_count_++ == 0)
    Hide();
}

void NativeWidgetWin::PopForceHidden() {
  if (--force_hidden_count_ <= 0) {
    force_hidden_count_ = 0;
    ShowWindow(SW_SHOW);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, CompositorDelegate implementation:

void NativeWidgetWin::ScheduleDraw() {
  RECT rect;
  ::GetClientRect(GetNativeView(), &rect);
  InvalidateRect(GetNativeView(), &rect, FALSE);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, NativeWidget implementation:

void NativeWidgetWin::InitNativeWidget(const Widget::InitParams& params) {
  SetInitParams(params);

  GetMonitorAndRects(params.bounds.ToRECT(), &last_monitor_,
                     &last_monitor_rect_, &last_work_area_);

  // Create the window.
  WindowImpl::Init(params.GetParent(), params.bounds);
}

NonClientFrameView* NativeWidgetWin::CreateNonClientFrameView() {
  return GetWidget()->ShouldUseNativeFrame() ?
      new NativeFrameView(GetWidget()) : NULL;
}

void NativeWidgetWin::UpdateFrameAfterFrameChange() {
  // We've either gained or lost a custom window region, so reset it now.
  ResetWindowRegion(true);
}

bool NativeWidgetWin::ShouldUseNativeFrame() const {
  return IsAeroGlassEnabled();
}

void NativeWidgetWin::FrameTypeChanged() {
  // Called when the frame type could possibly be changing (theme change or
  // DWM composition change).
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // We need to toggle the rendering policy of the DWM/glass frame as we
    // change from opaque to glass. "Non client rendering enabled" means that
    // the DWM's glass non-client rendering is enabled, which is why
    // DWMNCRP_ENABLED is used for the native frame case. _DISABLED means the
    // DWM doesn't render glass, and so is used in the custom frame case.
    DWMNCRENDERINGPOLICY policy = GetWidget()->ShouldUseNativeFrame() ?
        DWMNCRP_ENABLED : DWMNCRP_DISABLED;
    DwmSetWindowAttribute(GetNativeView(), DWMWA_NCRENDERING_POLICY,
                          &policy, sizeof(DWMNCRENDERINGPOLICY));
  }

  // Send a frame change notification, since the non-client metrics have
  // changed.
  SendFrameChanged(GetNativeView());

  // Update the non-client view with the correct frame view for the active frame
  // type.
  GetWidget()->non_client_view()->UpdateFrame();

  // WM_DWMCOMPOSITIONCHANGED is only sent to top level windows, however we want
  // to notify our children too, since we can have MDI child windows who need to
  // update their appearance.
  EnumChildWindows(GetNativeView(), &SendDwmCompositionChanged, NULL);
}

Widget* NativeWidgetWin::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetWin::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetWin::GetNativeView() const {
  return WindowImpl::hwnd();
}

gfx::NativeWindow NativeWidgetWin::GetNativeWindow() const {
  return WindowImpl::hwnd();
}

Widget* NativeWidgetWin::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetWin::GetCompositor() const {
  return compositor_.get();
}

ui::Compositor* NativeWidgetWin::GetCompositor() {
  return compositor_.get();
}

void NativeWidgetWin::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
}

void NativeWidgetWin::ReorderLayers() {
}

void NativeWidgetWin::ViewRemoved(View* view) {
  if (drop_target_.get())
    drop_target_->ResetTargetViewIfEquals(view);

  ClearAccessibilityViewEvent(view);
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

TooltipManager* NativeWidgetWin::GetTooltipManager() const {
  return tooltip_manager_.get();
}

bool NativeWidgetWin::IsScreenReaderActive() const {
  return screen_reader_active_;
}

void NativeWidgetWin::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  // Now call the Windows-specific method to notify MSAA clients of this
  // event.  The widget gives us a temporary unique child ID to associate
  // with this view so that clients can call get_accChild in
  // NativeViewAccessibilityWin to retrieve the IAccessible associated
  // with this view.
  int child_id = AddAccessibilityViewEvent(view);
  ::NotifyWinEvent(NativeViewAccessibilityWin::MSAAEvent(event_type),
                   GetNativeView(), OBJID_CLIENT, child_id);
}

void NativeWidgetWin::SetMouseCapture() {
  DCHECK(!HasMouseCapture());
  SetCapture(hwnd());
}

void NativeWidgetWin::ReleaseMouseCapture() {
  ReleaseCapture();
}

bool NativeWidgetWin::HasMouseCapture() const {
  return GetCapture() == hwnd();
}

InputMethod* NativeWidgetWin::CreateInputMethod() {
  if (views::Widget::IsPureViews()) {
    InputMethod* input_method = new InputMethodWin(this);
    input_method->Init(GetWidget());
    return input_method;
  }
  return NULL;
}

void NativeWidgetWin::CenterWindow(const gfx::Size& size) {
  HWND parent = GetParent();
  if (!IsWindow())
    parent = ::GetWindow(GetNativeView(), GW_OWNER);
  ui::CenterAndSizeWindow(parent, GetNativeView(), size, false);
}

void NativeWidgetWin::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool succeeded = !!::GetWindowPlacement(GetNativeView(), &wp);
  DCHECK(succeeded);

  if (bounds != NULL) {
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    const bool succeeded = !!GetMonitorInfo(
        MonitorFromWindow(GetNativeView(), MONITOR_DEFAULTTONEAREST), &mi);
    DCHECK(succeeded);
    *bounds = gfx::Rect(wp.rcNormalPosition);
    // Convert normal position from workarea coordinates to screen coordinates.
    bounds->Offset(mi.rcWork.left - mi.rcMonitor.left,
                   mi.rcWork.top - mi.rcMonitor.top);
  }

  if (show_state != NULL) {
    if (wp.showCmd == SW_SHOWMAXIMIZED)
      *show_state = ui::SHOW_STATE_MAXIMIZED;
    else if (wp.showCmd == SW_SHOWMINIMIZED)
      *show_state = ui::SHOW_STATE_MINIMIZED;
    else
      *show_state = ui::SHOW_STATE_NORMAL;
  }
}

void NativeWidgetWin::SetWindowTitle(const string16& title) {
  SetWindowText(GetNativeView(), title.c_str());
  SetAccessibleName(title);
}

void NativeWidgetWin::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
  if (!window_icon.isNull()) {
    HICON windows_icon = IconUtil::CreateHICONFromSkBitmap(window_icon);
    // We need to make sure to destroy the previous icon, otherwise we'll leak
    // these GDI objects until we crash!
    HICON old_icon = reinterpret_cast<HICON>(
        SendMessage(GetNativeView(), WM_SETICON, ICON_SMALL,
                    reinterpret_cast<LPARAM>(windows_icon)));
    if (old_icon)
      DestroyIcon(old_icon);
  }
  if (!app_icon.isNull()) {
    HICON windows_icon = IconUtil::CreateHICONFromSkBitmap(app_icon);
    HICON old_icon = reinterpret_cast<HICON>(
        SendMessage(GetNativeView(), WM_SETICON, ICON_BIG,
                    reinterpret_cast<LPARAM>(windows_icon)));
    if (old_icon)
      DestroyIcon(old_icon);
  }
}

void NativeWidgetWin::SetAccessibleName(const string16& name) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr))
    hr = pAccPropServices->SetHwndPropStr(GetNativeView(), OBJID_CLIENT,
                                          CHILDID_SELF, PROPID_ACC_NAME,
                                          name.c_str());
}

void NativeWidgetWin::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    if (role) {
      var.vt = VT_I4;
      var.lVal = NativeViewAccessibilityWin::MSAARole(role);
      hr = pAccPropServices->SetHwndProp(GetNativeView(), OBJID_CLIENT,
                                         CHILDID_SELF, PROPID_ACC_ROLE, var);
    }
  }
}

void NativeWidgetWin::SetAccessibleState(ui::AccessibilityTypes::State state) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    if (state) {
      var.vt = VT_I4;
      var.lVal = NativeViewAccessibilityWin::MSAAState(state);
      hr = pAccPropServices->SetHwndProp(GetNativeView(), OBJID_CLIENT,
                                         CHILDID_SELF, PROPID_ACC_STATE, var);
    }
  }
}

void NativeWidgetWin::BecomeModal() {
  // We implement modality by crawling up the hierarchy of windows starting
  // at the owner, disabling all of them so that they don't receive input
  // messages.
  HWND start = ::GetWindow(GetNativeView(), GW_OWNER);
  while (start) {
    ::EnableWindow(start, FALSE);
    start = ::GetParent(start);
  }
}

gfx::Rect NativeWidgetWin::GetWindowScreenBounds() const {
  RECT r;
  GetWindowRect(&r);
  return gfx::Rect(r);
}

gfx::Rect NativeWidgetWin::GetClientAreaScreenBounds() const {
  RECT r;
  GetClientRect(&r);
  POINT point = { r.left, r.top };
  ClientToScreen(hwnd(), &point);
  return gfx::Rect(point.x, point.y, r.right - r.left, r.bottom - r.top);
}

gfx::Rect NativeWidgetWin::GetRestoredBounds() const {
  // If we're in fullscreen mode, we've changed the normal bounds to the monitor
  // rect, so return the saved bounds instead.
  if (IsFullscreen())
    return gfx::Rect(saved_window_info_.window_rect);

  gfx::Rect bounds;
  GetWindowPlacement(&bounds, NULL);
  return bounds;
}

void NativeWidgetWin::SetBounds(const gfx::Rect& bounds) {
  LONG style = GetWindowLong(GWL_STYLE);
  if (style & WS_MAXIMIZE)
    SetWindowLong(GWL_STYLE, style & ~WS_MAXIMIZE);
  SetWindowPos(NULL, bounds.x(), bounds.y(), bounds.width(), bounds.height(),
               SWP_NOACTIVATE | SWP_NOZORDER);
}

void NativeWidgetWin::SetSize(const gfx::Size& size) {
  SetWindowPos(NULL, 0, 0, size.width(), size.height(),
               SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOMOVE);
}

void NativeWidgetWin::MoveAbove(gfx::NativeView native_view) {
  SetWindowPos(native_view, 0, 0, 0, 0,
               SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void NativeWidgetWin::MoveToTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetWin::SetShape(gfx::NativeRegion region) {
  SetWindowRgn(region, TRUE);
}

void NativeWidgetWin::Close() {
  if (!IsWindow())
    return;  // No need to do anything.

  // Let's hide ourselves right away.
  Hide();

  // Modal dialog windows disable their owner windows; re-enable them now so
  // they can activate as foreground windows upon this window's destruction.
  RestoreEnabledIfNecessary();

  if (!close_widget_factory_.HasWeakPtrs()) {
    // And we delay the close so that if we are called from an ATL callback,
    // we don't destroy the window before the callback returned (as the caller
    // may delete ourselves on destroy and the ATL callback would still
    // dereference us when the callback returns).
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NativeWidgetWin::CloseNow,
                   close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetWin::CloseNow() {
  // We may already have been destroyed if the selection resulted in a tab
  // switch which will have reactivated the browser window and closed us, so
  // we need to check to see if we're still a window before trying to destroy
  // ourself.
  if (IsWindow())
    DestroyWindow(hwnd());
}

void NativeWidgetWin::EnableClose(bool enable) {
  // Disable the native frame's close button regardless of whether or not the
  // native frame is in use, since this also affects the system menu.
  EnableMenuItem(GetSystemMenu(GetNativeView(), false), SC_CLOSE, enable);
  SendFrameChanged(GetNativeView());
}

void NativeWidgetWin::Show() {
  if (!IsWindow())
    return;

  ShowWindow(SW_SHOWNOACTIVATE);
  SetInitialFocus();
}

void NativeWidgetWin::Hide() {
  if (IsWindow()) {
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(NULL, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                 SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

void NativeWidgetWin::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  WINDOWPLACEMENT placement = { 0 };
  placement.length = sizeof(WINDOWPLACEMENT);
  placement.showCmd = SW_SHOWMAXIMIZED;
  placement.rcNormalPosition = restored_bounds.ToRECT();
  SetWindowPlacement(hwnd(), &placement);
}

void NativeWidgetWin::ShowWithWindowState(ui::WindowShowState show_state) {
  DWORD native_show_state;
  switch (show_state) {
    case ui::SHOW_STATE_INACTIVE:
      native_show_state = SW_SHOWNOACTIVATE;
      break;
    case ui::SHOW_STATE_MAXIMIZED:
      native_show_state = SW_SHOWMAXIMIZED;
      break;
    case ui::SHOW_STATE_MINIMIZED:
      native_show_state = SW_SHOWMINIMIZED;
      break;
    default:
      native_show_state = GetShowState();
      break;
  }
  Show(native_show_state);
}

bool NativeWidgetWin::IsVisible() const {
  return !!::IsWindowVisible(hwnd());
}

void NativeWidgetWin::Activate() {
  if (IsMinimized())
    ::ShowWindow(GetNativeView(), SW_RESTORE);
  ::SetWindowPos(GetNativeView(), HWND_TOP, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(GetNativeView());
}

void NativeWidgetWin::Deactivate() {
  HWND hwnd = ::GetNextWindow(GetNativeView(), GW_HWNDNEXT);
  if (hwnd)
    ::SetForegroundWindow(hwnd);
}

bool NativeWidgetWin::IsActive() const {
  return GetActiveWindow() == hwnd();
}

void NativeWidgetWin::SetAlwaysOnTop(bool on_top) {
  ::SetWindowPos(GetNativeView(), on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void NativeWidgetWin::Maximize() {
  ExecuteSystemMenuCommand(SC_MAXIMIZE);
}

void NativeWidgetWin::Minimize() {
  ExecuteSystemMenuCommand(SC_MINIMIZE);

  delegate_->OnNativeBlur(NULL);
}

bool NativeWidgetWin::IsMaximized() const {
  return !!::IsZoomed(GetNativeView());
}

bool NativeWidgetWin::IsMinimized() const {
  return !!::IsIconic(GetNativeView());
}

void NativeWidgetWin::Restore() {
  ExecuteSystemMenuCommand(SC_RESTORE);
}

void NativeWidgetWin::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return;  // Nothing to do.

  // Reduce jankiness during the following position changes by hiding the window
  // until it's in the final position.
  PushForceHidden();

  // Size/position/style window appropriately.
  if (!fullscreen_) {
    // Save current window information.  We force the window into restored mode
    // before going fullscreen because Windows doesn't seem to hide the
    // taskbar if the window is in the maximized state.
    saved_window_info_.maximized = IsMaximized();
    if (saved_window_info_.maximized)
      Restore();
    saved_window_info_.style = GetWindowLong(GWL_STYLE);
    saved_window_info_.ex_style = GetWindowLong(GWL_EXSTYLE);
    GetWindowRect(&saved_window_info_.window_rect);
  }

  fullscreen_ = fullscreen;

  if (fullscreen_) {
    // Set new window style and size.
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(MonitorFromWindow(GetNativeView(), MONITOR_DEFAULTTONEAREST),
                   &monitor_info);
    gfx::Rect monitor_rect(monitor_info.rcMonitor);
    SetWindowLong(GWL_STYLE,
                  saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(GWL_EXSTYLE,
                  saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME |
                  WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    SetWindowPos(NULL, monitor_rect.x(), monitor_rect.y(),
                 monitor_rect.width(), monitor_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  } else {
    // Reset original window style and size.  The multiple window size/moves
    // here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
    // repainted.  Better-looking methods welcome.
    gfx::Rect new_rect(saved_window_info_.window_rect);
    SetWindowLong(GWL_STYLE, saved_window_info_.style);
    SetWindowLong(GWL_EXSTYLE, saved_window_info_.ex_style);
    SetWindowPos(NULL, new_rect.x(), new_rect.y(), new_rect.width(),
                 new_rect.height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (saved_window_info_.maximized)
      Maximize();
  }

  // Undo our anti-jankiness hacks.
  PopForceHidden();
}

bool NativeWidgetWin::IsFullscreen() const {
  return fullscreen_;
}

void NativeWidgetWin::SetOpacity(unsigned char opacity) {
  layered_alpha_ = static_cast<BYTE>(opacity);
}

void NativeWidgetWin::SetUseDragFrame(bool use_drag_frame) {
  if (use_drag_frame) {
    // Make the frame slightly transparent during the drag operation.
    drag_frame_saved_window_style_ = GetWindowLong(GWL_STYLE);
    drag_frame_saved_window_ex_style_ = GetWindowLong(GWL_EXSTYLE);
    SetWindowLong(GWL_EXSTYLE,
                  drag_frame_saved_window_ex_style_ | WS_EX_LAYERED);
    // Remove the captions tyle so the window doesn't have window controls for a
    // more "transparent" look.
    SetWindowLong(GWL_STYLE, drag_frame_saved_window_style_ & ~WS_CAPTION);
    SetLayeredWindowAttributes(GetNativeWindow(), RGB(0xFF, 0xFF, 0xFF),
                               kDragFrameWindowAlpha, LWA_ALPHA);
  } else {
    SetWindowLong(GWL_STYLE, drag_frame_saved_window_style_);
    SetWindowLong(GWL_EXSTYLE, drag_frame_saved_window_ex_style_);
  }
}

bool NativeWidgetWin::IsAccessibleWidget() const {
  return screen_reader_active_;
}

void NativeWidgetWin::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   int operation) {
  scoped_refptr<ui::DragSource> drag_source(new ui::DragSource);
  DWORD effects;
  DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data), drag_source,
             ui::DragDropTypes::DragOperationToDropEffect(operation), &effects);
}

void NativeWidgetWin::SchedulePaintInRect(const gfx::Rect& rect) {
  if (use_layered_buffer_) {
    // We must update the back-buffer immediately, since Windows' handling of
    // invalid rects is somewhat mysterious.
    invalid_rect_ = invalid_rect_.Union(rect);

    // In some situations, such as drag and drop, when Windows itself runs a
    // nested message loop our message loop appears to be starved and we don't
    // receive calls to DidProcessMessage(). This only seems to affect layered
    // windows, so we schedule a redraw manually using a task, since those never
    // seem to be starved. Also, wtf.
    if (!paint_layered_window_factory_.HasWeakPtrs()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&NativeWidgetWin::RedrawLayeredWindowContents,
                     paint_layered_window_factory_.GetWeakPtr()));
    }
  } else {
    // InvalidateRect() expects client coordinates.
    RECT r = rect.ToRECT();
    InvalidateRect(hwnd(), &r, FALSE);
  }
}

void NativeWidgetWin::SetCursor(gfx::NativeCursor cursor) {
  if (cursor) {
    previous_cursor_ = ::SetCursor(cursor);
  } else if (previous_cursor_) {
    ::SetCursor(previous_cursor_);
    previous_cursor_ = NULL;
  }
}

void NativeWidgetWin::ClearNativeFocus() {
  ::SetFocus(GetNativeView());
}

void NativeWidgetWin::FocusNativeView(gfx::NativeView native_view) {
  // Only reset focus if hwnd is not already focused.
  if (native_view && ::GetFocus() != native_view)
    ::SetFocus(native_view);
}

bool NativeWidgetWin::ConvertPointFromAncestor(
    const Widget* ancestor, gfx::Point* point) const {
  NOTREACHED();
  return false;
}

gfx::Rect NativeWidgetWin::GetWorkAreaBoundsInScreen() const {
  return gfx::Screen::GetMonitorWorkAreaNearestWindow(GetNativeView());
}

void NativeWidgetWin::SetInactiveRenderingDisabled(bool value) {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, MessageLoop::Observer implementation:

base::EventStatus NativeWidgetWin::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void NativeWidgetWin::DidProcessEvent(const base::NativeEvent& event) {
  RedrawInvalidRect();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, WindowImpl overrides:

HICON NativeWidgetWin::GetDefaultWindowIcon() const {
  if (ViewsDelegate::views_delegate)
    return ViewsDelegate::views_delegate->GetDefaultWindowIcon();
  return NULL;
}

LRESULT NativeWidgetWin::OnWndProc(UINT message, WPARAM w_param,
                                   LPARAM l_param) {
  HWND window = hwnd();
  LRESULT result = 0;

  // First allow messages sent by child controls to be processed directly by
  // their associated views. If such a view is present, it will handle the
  // message *instead of* this NativeWidgetWin.
  if (ProcessChildWindowMessage(message, w_param, l_param, &result))
    return result;

  // Otherwise we handle everything else.
  if (!ProcessWindowMessage(window, message, w_param, l_param, result))
    result = DefWindowProc(window, message, w_param, l_param);
  if (message == WM_NCDESTROY) {
    MessageLoopForUI::current()->RemoveObserver(this);
    OnFinalMessage(window);
  }

  // Only top level widget should store/restore focus.
  if (message == WM_ACTIVATE && GetWidget()->is_top_level())
    PostProcessActivateMessage(this, LOWORD(w_param));
  if (message == WM_ENABLE && restore_focus_when_enabled_) {
    // This path should be executed only for top level as
    // restore_focus_when_enabled_ is set in PostProcessActivateMessage.
    DCHECK(GetWidget()->is_top_level());
    restore_focus_when_enabled_ = false;
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, protected:

// Message handlers ------------------------------------------------------------

void NativeWidgetWin::OnActivate(UINT action, BOOL minimized, HWND window) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnActivateApp(BOOL active, DWORD thread_id) {
  if (GetWidget()->non_client_view() && !active &&
      thread_id != GetCurrentThreadId()) {
    // Another application was activated, we should reset any state that
    // disables inactive rendering now.
    delegate_->EnableInactiveRendering();
    // Also update the native frame if it is rendering the non-client area.
    if (GetWidget()->ShouldUseNativeFrame())
      DefWindowProcWithRedrawLock(WM_NCACTIVATE, FALSE, 0);
  }
}

LRESULT NativeWidgetWin::OnAppCommand(HWND window,
                                      short app_command,
                                      WORD device,
                                      int keystate) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  BOOL is_handled = (GetWidget()->widget_delegate() &&
      GetWidget()->widget_delegate()->ExecuteWindowsCommand(app_command)) ?
      TRUE : FALSE;
  SetMsgHandled(is_handled);
  // Make sure to return TRUE if the event was handled or in some cases the
  // system will execute the default handler which can cause bugs like going
  // forward or back two pages instead of one.
  return is_handled;
}

void NativeWidgetWin::OnCancelMode() {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnCaptureChanged(HWND hwnd) {
  delegate_->OnMouseCaptureLost();
}

void NativeWidgetWin::OnClose() {
  GetWidget()->Close();
}

void NativeWidgetWin::OnCommand(UINT notification_code,
                                int command_id,
                                HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 ||
      GetWidget()->widget_delegate()->ExecuteWindowsCommand(command_id)) {
    SetMsgHandled(FALSE);
  }
}

LRESULT NativeWidgetWin::OnCreate(CREATESTRUCT* create_struct) {
  SetNativeWindowProperty(kNativeWidgetKey, this);
  CHECK_EQ(this, GetNativeWidgetForNativeView(hwnd()));

  use_layered_buffer_ = !!(window_ex_style() & WS_EX_LAYERED);

  // Attempt to detect screen readers by sending an event with our custom id.
  if (!IsAccessibleWidget())
    NotifyWinEvent(EVENT_SYSTEM_ALERT, hwnd(), kCustomObjectID, CHILDID_SELF);

  props_.push_back(ui::SetWindowSupportsRerouteMouseWheel(hwnd()));

  drop_target_ = new DropTargetWin(
      static_cast<internal::RootView*>(GetWidget()->GetRootView()));

  // We need to add ourselves as a message loop observer so that we can repaint
  // aggressively if the contents of our window become invalid. Unfortunately
  // WM_PAINT messages are starved and we get flickery redrawing when resizing
  // if we do not do this.
  MessageLoopForUI::current()->AddObserver(this);

  // Windows special DWM window frame requires a special tooltip manager so
  // that window controls in Chrome windows don't flicker when you move your
  // mouse over them. See comment in aero_tooltip_manager.h.
  Widget* widget = GetWidget()->GetTopLevelWidget();
  if (widget && widget->ShouldUseNativeFrame()) {
    tooltip_manager_.reset(new AeroTooltipManager(GetWidget()));
  } else {
    tooltip_manager_.reset(new TooltipManagerWin(GetWidget()));
  }
  if (!tooltip_manager_->Init()) {
    // There was a problem creating the TooltipManager. Common error is 127.
    // See 82193 for details.
    LOG_GETLASTERROR(WARNING) << "tooltip creation failed, disabling tooltips";
    tooltip_manager_.reset();
  }

  // This message initializes the window so that focus border are shown for
  // windows.
  SendMessage(
      hwnd(), WM_CHANGEUISTATE, MAKELPARAM(UIS_CLEAR, UISF_HIDEFOCUS), 0);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ImmAssociateContextEx(hwnd(), NULL, 0);

  // We need to allow the delegate to size its contents since the window may not
  // receive a size notification when its initial bounds are specified at window
  // creation time.
  ClientAreaSizeChanged();

#if defined(VIEWS_COMPOSITOR)
  if (View::get_use_acceleration_when_possible()) {
    if (ui::Compositor::compositor_factory()) {
      compositor_ = (*Widget::compositor_factory())(this);
    } else {
      CRect window_rect;
      GetClientRect(&window_rect);
      compositor_ = ui::Compositor::Create(this,
          hwnd(),
          gfx::Size(window_rect.Width(), window_rect.Height()));
    }
    if (compositor_.get()) {
      delegate_->AsWidget()->GetRootView()->SetPaintToLayer(true);
      compositor_->SetRootLayer(delegate_->AsWidget()->GetRootView()->layer());
    }
  }
#endif

  delegate_->OnNativeWidgetCreated();

  // Get access to a modifiable copy of the system menu.
  GetSystemMenu(hwnd(), false);
  return 0;
}

void NativeWidgetWin::OnDestroy() {
  delegate_->OnNativeWidgetDestroying();
  if (drop_target_.get()) {
    RevokeDragDrop(hwnd());
    drop_target_ = NULL;
  }
}

void NativeWidgetWin::OnDisplayChange(UINT bits_per_pixel, CSize screen_size) {
  GetWidget()->widget_delegate()->OnDisplayChanged();
}

LRESULT NativeWidgetWin::OnDwmCompositionChanged(UINT msg,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  if (!GetWidget()->non_client_view()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // For some reason, we need to hide the window while we're changing the frame
  // type only when we're changing it in response to WM_DWMCOMPOSITIONCHANGED.
  // If we don't, the client area will be filled with black. I'm suspecting
  // something skia-ey.
  // Frame type toggling caused by the user (e.g. switching theme) doesn't seem
  // to have this requirement.
  FrameTypeChanged();
  return 0;
}

void NativeWidgetWin::OnEndSession(BOOL ending, UINT logoff) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnEnterSizeMove() {
  delegate_->OnNativeWidgetBeginUserBoundsChange();
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnEraseBkgnd(HDC dc) {
  // This is needed for magical win32 flicker ju-ju.
  return 1;
}

void NativeWidgetWin::OnExitMenuLoop(BOOL is_track_popup_menu) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnExitSizeMove() {
  delegate_->OnNativeWidgetEndUserBoundsChange();
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnGetObject(UINT uMsg,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  LRESULT reference_result = static_cast<LRESULT>(0L);

  // Accessibility readers will send an OBJID_CLIENT message
  if (OBJID_CLIENT == l_param) {
    // Retrieve MSAA dispatch object for the root view.
    base::win::ScopedComPtr<IAccessible> root(
        GetWidget()->GetRootView()->GetNativeViewAccessible());

    // Create a reference that MSAA will marshall to the client.
    reference_result = LresultFromObject(IID_IAccessible, w_param,
        static_cast<IAccessible*>(root.Detach()));
  }

  if (kCustomObjectID == l_param) {
    // An MSAA client requestes our custom id. Assume that we have detected an
    // active windows screen reader.
    OnScreenReaderDetected();

    // Return with failure.
    return static_cast<LRESULT>(0L);
  }

  return reference_result;
}

void NativeWidgetWin::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  gfx::Size min_window_size(delegate_->GetMinimumSize());
  // Add the native frame border size to the minimum size if the view reports
  // its size as the client size.
  if (WidgetSizeIsClientSize()) {
    CRect client_rect, window_rect;
    GetClientRect(&client_rect);
    GetWindowRect(&window_rect);
    window_rect -= client_rect;
    min_window_size.Enlarge(window_rect.Width(), window_rect.Height());
  }
  minmax_info->ptMinTrackSize.x = min_window_size.width();
  minmax_info->ptMinTrackSize.y = min_window_size.height();
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnHScroll(int scroll_type,
                                short position,
                                HWND scrollbar) {
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnImeMessages(UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();
  if (!input_method || input_method->IsMock()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  InputMethodWin* ime_win = static_cast<InputMethodWin*>(input_method);
  BOOL handled = FALSE;
  LRESULT result = ime_win->OnImeMessages(message, w_param, l_param, &handled);

  SetMsgHandled(handled);
  return result;
}

void NativeWidgetWin::OnInitMenu(HMENU menu) {
  bool is_fullscreen = IsFullscreen();
  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_fullscreen && !is_minimized && !is_maximized;

  ScopedRedrawLock lock(this);
  EnableMenuItem(menu, SC_RESTORE, is_minimized || is_maximized);
  EnableMenuItem(menu, SC_MOVE, is_restored);
  EnableMenuItem(menu, SC_SIZE,
                 GetWidget()->widget_delegate()->CanResize() && is_restored);
  EnableMenuItem(menu, SC_MAXIMIZE,
                 GetWidget()->widget_delegate()->CanMaximize() &&
                     !is_fullscreen && !is_maximized);
  EnableMenuItem(menu, SC_MINIMIZE,
                 GetWidget()->widget_delegate()->CanMaximize() &&
                     !is_minimized);
}

void NativeWidgetWin::OnInitMenuPopup(HMENU menu,
                                      UINT position,
                                      BOOL is_system_menu) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnInputLangChange(DWORD character_set,
                                        HKL input_language_id) {
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();

  if (input_method && !input_method->IsMock()) {
    static_cast<InputMethodWin*>(input_method)->OnInputLangChange(
        character_set, input_language_id);
  }
}

LRESULT NativeWidgetWin::OnKeyEvent(UINT message,
                                    WPARAM w_param,
                                    LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param };
  KeyEvent key(msg);
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();
  if (input_method)
    input_method->DispatchKeyEvent(key);
  else
    DispatchKeyEventPostIME(key);
  return 0;
}

void NativeWidgetWin::OnKillFocus(HWND focused_window) {
  delegate_->OnNativeBlur(focused_window);
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();
  if (input_method)
    input_method->OnBlur();
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnMouseActivate(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  // TODO(beng): resolve this with the GetWindowLong() check on the subsequent
  //             line.
  if (GetWidget()->non_client_view())
    return delegate_->CanActivate() ? MA_ACTIVATE : MA_NOACTIVATEANDEAT;
  if (GetWindowLong(GWL_EXSTYLE) & WS_EX_NOACTIVATE)
    return MA_NOACTIVATE;
  SetMsgHandled(FALSE);
  return MA_ACTIVATE;
}

LRESULT NativeWidgetWin::OnMouseRange(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  if (message == WM_RBUTTONUP && is_right_mouse_pressed_on_caption_) {
    is_right_mouse_pressed_on_caption_ = false;
    ReleaseCapture();
    // |point| is in window coordinates, but WM_NCHITTEST and TrackPopupMenu()
    // expect screen coordinates.
    CPoint screen_point(l_param);
    MapWindowPoints(GetNativeView(), HWND_DESKTOP, &screen_point, 1);
    w_param = SendMessage(GetNativeView(), WM_NCHITTEST, 0,
                          MAKELPARAM(screen_point.x, screen_point.y));
    if (w_param == HTCAPTION || w_param == HTSYSMENU) {
      ui::ShowSystemMenu(GetNativeView(), screen_point.x, screen_point.y);
      return 0;
    }
  } else if (message == WM_NCLBUTTONDOWN &&
             !GetWidget()->ShouldUseNativeFrame()) {
    switch (w_param) {
      case HTCLOSE:
      case HTMINBUTTON:
      case HTMAXBUTTON: {
        // When the mouse is pressed down in these specific non-client areas,
        // we need to tell the RootView to send the mouse pressed event (which
        // sets capture, allowing subsequent WM_LBUTTONUP (note, _not_
        // WM_NCLBUTTONUP) to fire so that the appropriate WM_SYSCOMMAND can be
        // sent by the applicable button's ButtonListener. We _have_ to do this
        // way rather than letting Windows just send the syscommand itself (as
        // would happen if we never did this dance) because for some insane
        // reason DefWindowProc for WM_NCLBUTTONDOWN also renders the pressed
        // window control button appearance, in the Windows classic style, over
        // our view! Ick! By handling this message we prevent Windows from
        // doing this undesirable thing, but that means we need to roll the
        // sys-command handling ourselves.
        // Combine |w_param| with common key state message flags.
        w_param |= ((GetKeyState(VK_CONTROL) & 0x80) == 0x80)? MK_CONTROL : 0;
        w_param |= ((GetKeyState(VK_SHIFT) & 0x80) == 0x80)? MK_SHIFT : 0;
      }
    }
  } else if (message == WM_NCRBUTTONDOWN &&
      (w_param == HTCAPTION || w_param == HTSYSMENU)) {
    is_right_mouse_pressed_on_caption_ = true;
    // We SetMouseCapture() to ensure we only show the menu when the button
    // down and up are both on the caption. Note: this causes the button up to
    // be WM_RBUTTONUP instead of WM_NCRBUTTONUP.
    SetMouseCapture();
  }

  MSG msg = { hwnd(), message, w_param, l_param, 0,
              { GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param) } };
  MouseEvent event(msg);

  if (!(event.flags() & ui::EF_IS_NON_CLIENT))
    if (tooltip_manager_.get())
      tooltip_manager_->OnMouse(message, w_param, l_param);

  if (event.type() == ui::ET_MOUSE_MOVED && !HasMouseCapture()) {
    // Windows only fires WM_MOUSELEAVE events if the application begins
    // "tracking" mouse events for a given HWND during WM_MOUSEMOVE events.
    // We need to call |TrackMouseEvents| to listen for WM_MOUSELEAVE.
    TrackMouseEvents((message == WM_NCMOUSEMOVE) ?
        TME_NONCLIENT | TME_LEAVE : TME_LEAVE);
  } else if (event.type() == ui::ET_MOUSE_EXITED) {
    // Reset our tracking flags so future mouse movement over this
    // NativeWidgetWin results in a new tracking session. Fall through for
    // OnMouseEvent.
    active_mouse_tracking_flags_ = 0;
  } else if (event.type() == ui::ET_MOUSEWHEEL) {
    // Reroute the mouse wheel to the window under the pointer if applicable.
    return (ui::RerouteMouseWheel(hwnd(), w_param, l_param) ||
            delegate_->OnMouseEvent(MouseWheelEvent(msg))) ? 0 : 1;
  }

  bool handled = delegate_->OnMouseEvent(event);

  if (!handled && message == WM_NCLBUTTONDOWN && w_param != HTSYSMENU &&
      !GetWidget()->ShouldUseNativeFrame()) {
    // TODO(msw): Eliminate undesired painting, or re-evaluate this workaround.
    // DefWindowProc for WM_NCLBUTTONDOWN does weird non-client painting, so we
    // need to call it inside a ScopedRedrawLock. This may cause other negative
    // side-effects (ex/ stifling non-client mouse releases).
    DefWindowProcWithRedrawLock(message, w_param, l_param);
    handled = true;
  }

  SetMsgHandled(handled);
  return 0;
}

void NativeWidgetWin::OnMove(const CPoint& point) {
  // TODO(beng): move to Widget.
  GetWidget()->widget_delegate()->OnWidgetMove();
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnMoving(UINT param, const LPRECT new_bounds) {
  // TODO(beng): move to Widget.
  GetWidget()->widget_delegate()->OnWidgetMove();
}

LRESULT NativeWidgetWin::OnNCActivate(BOOL active) {
  if (delegate_->CanActivate())
    delegate_->OnNativeWidgetActivationChanged(!!active);

  if (!GetWidget()->non_client_view()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  if (!delegate_->CanActivate())
    return TRUE;

  // The frame may need to redraw as a result of the activation change.
  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (IsVisible())
    GetWidget()->non_client_view()->SchedulePaint();

  if (!GetWidget()->ShouldUseNativeFrame()) {
    // TODO(beng, et al): Hack to redraw this window and child windows
    //     synchronously upon activation. Not all child windows are redrawing
    //     themselves leading to issues like http://crbug.com/74604
    //     We redraw out-of-process HWNDs asynchronously to avoid hanging the
    //     whole app if a child HWND belonging to a hung plugin is encountered.
    RedrawWindow(GetNativeView(), NULL, NULL,
                 RDW_NOCHILDREN | RDW_INVALIDATE | RDW_UPDATENOW);
    EnumChildWindows(GetNativeView(), EnumChildWindowsForRedraw, NULL);
  }

  // If we're active again, we should be allowed to render as inactive, so
  // tell the non-client view.
  bool inactive_rendering_disabled = delegate_->IsInactiveRenderingDisabled();
  if (IsActive())
    delegate_->EnableInactiveRendering();

  // Avoid DefWindowProc non-client rendering over our custom frame.
  if (!GetWidget()->ShouldUseNativeFrame()) {
    SetMsgHandled(TRUE);
    return TRUE;
  }

  return DefWindowProcWithRedrawLock(WM_NCACTIVATE,
                                     inactive_rendering_disabled || active, 0);
}

LRESULT NativeWidgetWin::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only override the default handling if we need to specify a custom
  // non-client edge width. Note that in most cases "no insets" means no
  // custom width, but in fullscreen mode we want a custom width of 0.
  gfx::Insets insets = GetClientAreaInsets();
  if (insets.empty() && !IsFullscreen()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  RECT* client_rect = mode ?
      &(reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0]) :
      reinterpret_cast<RECT*>(l_param);
  client_rect->left += insets.left();
  client_rect->top += insets.top();
  client_rect->bottom -= insets.bottom();
  client_rect->right -= insets.right();
  if (IsMaximized()) {
    // Find all auto-hide taskbars along the screen edges and adjust in by the
    // thickness of the auto-hide taskbar on each such edge, so the window isn't
    // treated as a "fullscreen app", which would cause the taskbars to
    // disappear.
    HMONITOR monitor = MonitorFromWindow(GetNativeView(),
                                         MONITOR_DEFAULTTONULL);
    if (!monitor) {
      // We might end up here if the window was previously minimized and the
      // user clicks on the taskbar button to restore it in the previously
      // maximized position. In that case WM_NCCALCSIZE is sent before the
      // window coordinates are restored to their previous values, so our
      // (left,top) would probably be (-32000,-32000) like all minimized
      // windows. So the above MonitorFromWindow call fails, but if we check
      // the window rect given with WM_NCCALCSIZE (which is our previous
      // restored window position) we will get the correct monitor handle.
      monitor = MonitorFromRect(client_rect, MONITOR_DEFAULTTONULL);
      if (!monitor) {
        // This is probably an extreme case that we won't hit, but if we don't
        // intersect any monitor, let us not adjust the client rect since our
        // window will not be visible anyway.
        return 0;
      }
    }
    if (GetTopmostAutoHideTaskbarForEdge(ABE_LEFT, monitor))
      client_rect->left += kAutoHideTaskbarThicknessPx;
    if (GetTopmostAutoHideTaskbarForEdge(ABE_TOP, monitor)) {
      if (GetWidget()->ShouldUseNativeFrame()) {
        // Tricky bit.  Due to a bug in DwmDefWindowProc()'s handling of
        // WM_NCHITTEST, having any nonclient area atop the window causes the
        // caption buttons to draw onscreen but not respond to mouse
        // hover/clicks.
        // So for a taskbar at the screen top, we can't push the
        // client_rect->top down; instead, we move the bottom up by one pixel,
        // which is the smallest change we can make and still get a client area
        // less than the screen size. This is visibly ugly, but there seems to
        // be no better solution.
        --client_rect->bottom;
      } else {
        client_rect->top += kAutoHideTaskbarThicknessPx;
      }
    }
    if (GetTopmostAutoHideTaskbarForEdge(ABE_RIGHT, monitor))
      client_rect->right -= kAutoHideTaskbarThicknessPx;
    if (GetTopmostAutoHideTaskbarForEdge(ABE_BOTTOM, monitor))
      client_rect->bottom -= kAutoHideTaskbarThicknessPx;

    // We cannot return WVR_REDRAW when there is nonclient area, or Windows
    // exhibits bugs where client pixels and child HWNDs are mispositioned by
    // the width/height of the upper-left nonclient area.
    return 0;
  }

  // If the window bounds change, we're going to relayout and repaint anyway.
  // Returning WVR_REDRAW avoids an extra paint before that of the old client
  // pixels in the (now wrong) location, and thus makes actions like resizing a
  // window from the left edge look slightly less broken.
  // We special case when left or top insets are 0, since these conditions
  // actually require another repaint to correct the layout after glass gets
  // turned on and off.
  if (insets.left() == 0 || insets.top() == 0)
    return 0;
  return mode ? WVR_REDRAW : 0;
}

LRESULT NativeWidgetWin::OnNCHitTest(const CPoint& point) {
  if (!GetWidget()->non_client_view()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // If the DWM is rendering the window controls, we need to give the DWM's
  // default window procedure first chance to handle hit testing.
  if (GetWidget()->ShouldUseNativeFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(GetNativeView(), WM_NCHITTEST, 0,
                         MAKELPARAM(point.x, point.y), &result)) {
      return result;
    }
  }

  // First, give the NonClientView a chance to test the point to see if it
  // provides any of the non-client area.
  POINT temp = point;
  MapWindowPoints(HWND_DESKTOP, GetNativeView(), &temp, 1);
  int component = delegate_->GetNonClientComponent(gfx::Point(temp));
  if (component != HTNOWHERE)
    return component;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnNCPaint(HRGN rgn) {
  // We only do non-client painting if we're not using the native frame.
  // It's required to avoid some native painting artifacts from appearing when
  // the window is resized.
  if (!GetWidget()->non_client_view() || GetWidget()->ShouldUseNativeFrame()) {
    SetMsgHandled(FALSE);
    return;
  }

  // We have an NC region and need to paint it. We expand the NC region to
  // include the dirty region of the root view. This is done to minimize
  // paints.
  CRect window_rect;
  GetWindowRect(&window_rect);

  if (window_rect.Width() != GetWidget()->GetRootView()->width() ||
      window_rect.Height() != GetWidget()->GetRootView()->height()) {
    // If the size of the window differs from the size of the root view it
    // means we're being asked to paint before we've gotten a WM_SIZE. This can
    // happen when the user is interactively resizing the window. To avoid
    // mass flickering we don't do anything here. Once we get the WM_SIZE we'll
    // reset the region of the window which triggers another WM_NCPAINT and
    // all is well.
    return;
  }

  CRect dirty_region;
  // A value of 1 indicates paint all.
  if (!rgn || rgn == reinterpret_cast<HRGN>(1)) {
    dirty_region = CRect(0, 0, window_rect.Width(), window_rect.Height());
  } else {
    RECT rgn_bounding_box;
    GetRgnBox(rgn, &rgn_bounding_box);
    if (!IntersectRect(&dirty_region, &rgn_bounding_box, &window_rect))
      return;  // Dirty region doesn't intersect window bounds, bale.

    // rgn_bounding_box is in screen coordinates. Map it to window coordinates.
    OffsetRect(&dirty_region, -window_rect.left, -window_rect.top);
  }

  // In theory GetDCEx should do what we want, but I couldn't get it to work.
  // In particular the docs mentiond DCX_CLIPCHILDREN, but as far as I can tell
  // it doesn't work at all. So, instead we get the DC for the window then
  // manually clip out the children.
  HDC dc = GetWindowDC(GetNativeView());
  ClipState clip_state;
  clip_state.x = window_rect.left;
  clip_state.y = window_rect.top;
  clip_state.parent = GetNativeView();
  clip_state.dc = dc;
  EnumChildWindows(GetNativeView(), &ClipDCToChild,
                   reinterpret_cast<LPARAM>(&clip_state));

  gfx::Rect old_paint_region = invalid_rect();

  if (!old_paint_region.IsEmpty()) {
    // The root view has a region that needs to be painted. Include it in the
    // region we're going to paint.

    CRect old_paint_region_crect = old_paint_region.ToRECT();
    CRect tmp = dirty_region;
    UnionRect(&dirty_region, &tmp, &old_paint_region_crect);
  }

  GetWidget()->GetRootView()->SchedulePaintInRect(gfx::Rect(dirty_region));

  // gfx::CanvasSkiaPaint's destructor does the actual painting. As such, wrap
  // the following in a block to force paint to occur so that we can release
  // the dc.
  {
    gfx::CanvasSkiaPaint canvas(dc, true, dirty_region.left,
                                dirty_region.top, dirty_region.Width(),
                                dirty_region.Height());
    delegate_->OnNativeWidgetPaint(&canvas);
  }

  ReleaseDC(GetNativeView(), dc);
  // When using a custom frame, we want to avoid calling DefWindowProc() since
  // that may render artifacts.
  SetMsgHandled(!GetWidget()->ShouldUseNativeFrame());
}

LRESULT NativeWidgetWin::OnNCUAHDrawCaption(UINT msg,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(!GetWidget()->ShouldUseNativeFrame());
  return 0;
}

LRESULT NativeWidgetWin::OnNCUAHDrawFrame(UINT msg,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(!GetWidget()->ShouldUseNativeFrame());
  return 0;
}

LRESULT NativeWidgetWin::OnNotify(int w_param, NMHDR* l_param) {
  // We can be sent this message before the tooltip manager is created, if a
  // subclass overrides OnCreate and creates some kind of Windows control there
  // that sends WM_NOTIFY messages.
  if (tooltip_manager_.get()) {
    bool handled;
    LRESULT result = tooltip_manager_->OnNotify(w_param, l_param, &handled);
    SetMsgHandled(handled);
    return result;
  }
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnPaint(HDC dc) {
  RECT dirty_rect;
  // Try to paint accelerated first.
  if (GetUpdateRect(hwnd(), &dirty_rect, FALSE) &&
      !IsRectEmpty(&dirty_rect)) {
    if (delegate_->OnNativeWidgetPaintAccelerated(
        gfx::Rect(dirty_rect))) {
      ValidateRect(hwnd(), NULL);
    } else {
      scoped_ptr<gfx::CanvasPaint> canvas(
          gfx::CanvasPaint::CreateCanvasPaint(hwnd()));
      delegate_->OnNativeWidgetPaint(canvas->AsCanvas());
    }
  } else {
    // TODO(msw): Find a better solution for this crbug.com/93530 workaround.
    // Some scenarios otherwise fail to validate minimized app/popup windows.
    ValidateRect(hwnd(), NULL);
  }
}

LRESULT NativeWidgetWin::OnPowerBroadcast(DWORD power_event, DWORD data) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessWmPowerBroadcastMessage(power_event);
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnReflectedMessage(UINT msg,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT NativeWidgetWin::OnSetCursor(UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  // Using ScopedRedrawLock here frequently allows content behind this window to
  // paint in front of this window, causing glaring rendering artifacts.
  // If omitting ScopedRedrawLock here triggers caption rendering artifacts via
  // DefWindowProc message handling, we'll need to find a better solution.
  SetMsgHandled(FALSE);
  return 0;
}

void NativeWidgetWin::OnSetFocus(HWND focused_window) {
  delegate_->OnNativeFocus(focused_window);
  InputMethod* input_method = GetWidget()->GetInputMethodDirect();
  if (input_method)
    input_method->OnFocus();
  SetMsgHandled(FALSE);
}

LRESULT NativeWidgetWin::OnSetIcon(UINT size_type, HICON new_icon) {
  // This shouldn't hurt even if we're using the native frame.
  return DefWindowProcWithRedrawLock(WM_SETICON, size_type,
                                     reinterpret_cast<LPARAM>(new_icon));
}

LRESULT NativeWidgetWin::OnSetText(const wchar_t* text) {
  // This shouldn't hurt even if we're using the native frame.
  return DefWindowProcWithRedrawLock(WM_SETTEXT, NULL,
                                     reinterpret_cast<LPARAM>(text));
}

void NativeWidgetWin::OnSettingChange(UINT flags, const wchar_t* section) {
  if (!GetParent() && (flags == SPI_SETWORKAREA) &&
      !GetWidget()->widget_delegate()->WillProcessWorkAreaChange()) {
    // Fire a dummy SetWindowPos() call, so we'll trip the code in
    // OnWindowPosChanging() below that notices work area changes.
    ::SetWindowPos(GetNativeView(), 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
        SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetMsgHandled(TRUE);
  } else {
    // TODO(beng): move to Widget.
    if (flags == SPI_SETWORKAREA)
      GetWidget()->widget_delegate()->OnWorkAreaChanged();
    SetMsgHandled(FALSE);
  }
}

void NativeWidgetWin::OnSize(UINT param, const CSize& size) {
  RedrawWindow(GetNativeView(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  ResetWindowRegion(false);
}

void NativeWidgetWin::OnSysCommand(UINT notification_code, CPoint click) {
  if (!GetWidget()->non_client_view())
    return;

  // Windows uses the 4 lower order bits of |notification_code| for type-
  // specific information so we must exclude this when comparing.
  static const int sc_mask = 0xFFF0;
  // Ignore size/move/maximize in fullscreen mode.
  if (IsFullscreen() &&
      (((notification_code & sc_mask) == SC_SIZE) ||
       ((notification_code & sc_mask) == SC_MOVE) ||
       ((notification_code & sc_mask) == SC_MAXIMIZE)))
    return;
  if (!GetWidget()->ShouldUseNativeFrame()) {
    if ((notification_code & sc_mask) == SC_MINIMIZE ||
        (notification_code & sc_mask) == SC_MAXIMIZE ||
        (notification_code & sc_mask) == SC_RESTORE) {
      GetWidget()->non_client_view()->ResetWindowControls();
    } else if ((notification_code & sc_mask) == SC_MOVE ||
               (notification_code & sc_mask) == SC_SIZE) {
      if (!IsVisible()) {
        // Circumvent ScopedRedrawLocks and force visibility before entering a
        // resize or move modal loop to get continuous sizing/moving feedback.
        SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_VISIBLE);
      }
    }
  }

  // Handle SC_KEYMENU, which means that the user has pressed the ALT
  // key and released it, so we should focus the menu bar.
  if ((notification_code & sc_mask) == SC_KEYMENU && click.x == 0) {
    // Retrieve the status of shift and control keys to prevent consuming
    // shift+alt keys, which are used by Windows to change input languages.
    ui::Accelerator accelerator(ui::KeyboardCodeForWindowsKeyCode(VK_MENU),
                                !!(GetKeyState(VK_SHIFT) & 0x8000),
                                !!(GetKeyState(VK_CONTROL) & 0x8000),
                                false);
    GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
    return;
  }

  // If the delegate can't handle it, the system implementation will be called.
  if (!delegate_->ExecuteCommand(notification_code)) {
    DefWindowProc(GetNativeView(), WM_SYSCOMMAND, notification_code,
                  MAKELPARAM(click.x, click.y));
  }
}

void NativeWidgetWin::OnThemeChanged() {
  // Notify NativeThemeWin.
  gfx::NativeThemeWin::instance()->CloseHandles();
}

void NativeWidgetWin::OnVScroll(int scroll_type,
                                short position,
                                HWND scrollbar) {
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  if (ignore_window_pos_changes_) {
    // If somebody's trying to toggle our visibility, change the nonclient area,
    // change our Z-order, or activate us, we should probably let it go through.
    if (!(window_pos->flags & ((IsVisible() ? SWP_HIDEWINDOW : SWP_SHOWWINDOW) |
        SWP_FRAMECHANGED)) &&
        (window_pos->flags & (SWP_NOZORDER | SWP_NOACTIVATE))) {
      // Just sizing/moving the window; ignore.
      window_pos->flags |= SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW;
      window_pos->flags &= ~(SWP_SHOWWINDOW | SWP_HIDEWINDOW);
    }
  } else if (!GetParent()) {
    CRect window_rect;
    HMONITOR monitor;
    gfx::Rect monitor_rect, work_area;
    if (GetWindowRect(&window_rect) &&
        GetMonitorAndRects(window_rect, &monitor, &monitor_rect, &work_area)) {
      if (monitor && (monitor == last_monitor_) &&
          (IsFullscreen() || ((monitor_rect == last_monitor_rect_) &&
              (work_area != last_work_area_)))) {
        // A rect for the monitor we're on changed.  Normally Windows notifies
        // us about this (and thus we're reaching here due to the SetWindowPos()
        // call in OnSettingChange() above), but with some software (e.g.
        // nVidia's nView desktop manager) the work area can change asynchronous
        // to any notification, and we're just sent a SetWindowPos() call with a
        // new (frequently incorrect) position/size.  In either case, the best
        // response is to throw away the existing position/size information in
        // |window_pos| and recalculate it based on the new work rect.
        gfx::Rect new_window_rect;
        if (IsFullscreen()) {
          new_window_rect = monitor_rect;
        } else if (IsZoomed()) {
          new_window_rect = work_area;
          int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
          new_window_rect.Inset(-border_thickness, -border_thickness);
        } else {
          new_window_rect = gfx::Rect(window_rect).AdjustToFit(work_area);
        }
        window_pos->x = new_window_rect.x();
        window_pos->y = new_window_rect.y();
        window_pos->cx = new_window_rect.width();
        window_pos->cy = new_window_rect.height();
        // WARNING!  Don't set SWP_FRAMECHANGED here, it breaks moving the child
        // HWNDs for some reason.
        window_pos->flags &= ~(SWP_NOSIZE | SWP_NOMOVE | SWP_NOREDRAW);
        window_pos->flags |= SWP_NOCOPYBITS;

        // Now ignore all immediately-following SetWindowPos() changes.  Windows
        // likes to (incorrectly) recalculate what our position/size should be
        // and send us further updates.
        ignore_window_pos_changes_ = true;
        DCHECK(!ignore_pos_changes_factory_.HasWeakPtrs());
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&NativeWidgetWin::StopIgnoringPosChanges,
                       ignore_pos_changes_factory_.GetWeakPtr()));
      }
      last_monitor_ = monitor;
      last_monitor_rect_ = monitor_rect;
      last_work_area_ = work_area;
    }
  }

  if (force_hidden_count_) {
    // Prevent the window from being made visible if we've been asked to do so.
    // See comment in header as to why we might want this.
    window_pos->flags &= ~SWP_SHOWWINDOW;
  }

  // When WM_WINDOWPOSCHANGING message is handled by DefWindowProc, it will
  // enforce (cx, cy) not to be smaller than (6, 6) for any non-popup window.
  // We work around this by changing cy back to our intended value.
  if (!GetParent() && ~(window_pos->flags & SWP_NOSIZE) && window_pos->cy < 6) {
    LONG old_cy = window_pos->cy;
    DefWindowProc(GetNativeView(), WM_WINDOWPOSCHANGING, 0,
        reinterpret_cast<LPARAM>(window_pos));
    window_pos->cy = old_cy;
    SetMsgHandled(TRUE);
    return;
  }

  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (DidClientAreaSizeChange(window_pos))
    ClientAreaSizeChanged();
  if (window_pos->flags & SWP_SHOWWINDOW)
    delegate_->OnNativeWidgetVisibilityChanged(true);
  else if (window_pos->flags & SWP_HIDEWINDOW)
    delegate_->OnNativeWidgetVisibilityChanged(false);
  SetMsgHandled(FALSE);
}

void NativeWidgetWin::OnFinalMessage(HWND window) {
  // We don't destroy props in WM_DESTROY as we may still get messages after
  // WM_DESTROY that assume the properties are still valid (such as WM_CLOSE).
  props_.reset();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, protected:

int NativeWidgetWin::GetShowState() const {
  return SW_SHOWNORMAL;
}

gfx::Insets NativeWidgetWin::GetClientAreaInsets() const {
  // Returning an empty Insets object causes the default handling in
  // NativeWidgetWin::OnNCCalcSize() to be invoked.
  if (!has_non_client_view_ || GetWidget()->ShouldUseNativeFrame())
    return gfx::Insets();

  if (IsMaximized()) {
    // Windows automatically adds a standard width border to all sides when a
    // window is maximized.
    int border_thickness = GetSystemMetrics(SM_CXSIZEFRAME);
    return gfx::Insets(border_thickness, border_thickness, border_thickness,
                       border_thickness);
  }
  // This is weird, but highly essential. If we don't offset the bottom edge
  // of the client rect, the window client area and window area will match,
  // and when returning to glass rendering mode from non-glass, the client
  // area will not paint black as transparent. This is because (and I don't
  // know why) the client area goes from matching the window rect to being
  // something else. If the client area is not the window rect in both
  // modes, the blackness doesn't occur. Because of this, we need to tell
  // the RootView to lay out to fit the window rect, rather than the client
  // rect when using the opaque frame.
  // Note: this is only required for non-fullscreen windows. Note that
  // fullscreen windows are in restored state, not maximized.
  return gfx::Insets(0, 0, IsFullscreen() ? 0 : 1, 0);
}

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

void NativeWidgetWin::OnScreenReaderDetected() {
  screen_reader_active_ = true;
}

void NativeWidgetWin::SetInitialFocus() {
  if (!GetWidget()->SetInitialFocus() &&
      !(GetWindowLong(GWL_EXSTYLE) & WS_EX_TRANSPARENT) &&
      !(GetWindowLong(GWL_EXSTYLE) & WS_EX_NOACTIVATE)) {
    // The window does not get keyboard messages unless we focus it.
    SetFocus(GetNativeView());
  }
}

void NativeWidgetWin::ExecuteSystemMenuCommand(int command) {
  if (command)
    SendMessage(GetNativeView(), WM_SYSCOMMAND, command, 0);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin, private:

// static
void NativeWidgetWin::PostProcessActivateMessage(NativeWidgetWin* widget,
                                                 int activation_state) {
  DCHECK(widget->GetWidget()->is_top_level());
  FocusManager* focus_manager = widget->GetWidget()->GetFocusManager();
  if (WA_INACTIVE == activation_state) {
    // We might get activated/inactivated without being enabled, so we need to
    // clear restore_focus_when_enabled_.
    widget->restore_focus_when_enabled_ = false;
    focus_manager->StoreFocusedView();
  } else {
    // We must restore the focus after the message has been DefProc'ed as it
    // does set the focus to the last focused HWND.
    // Note that if the window is not enabled, we cannot restore the focus as
    // calling ::SetFocus on a child of the non-enabled top-window would fail.
    // This is the case when showing a modal dialog (such as 'open file',
    // 'print'...) from a different thread.
    // In that case we delay the focus restoration to when the window is enabled
    // again.
    if (!IsWindowEnabled(widget->GetNativeView())) {
      DCHECK(!widget->restore_focus_when_enabled_);
      widget->restore_focus_when_enabled_ = true;
      return;
    }
    focus_manager->RestoreFocusedView();
  }
}

void NativeWidgetWin::SetInitParams(const Widget::InitParams& params) {
  // Set non-style attributes.
  ownership_ = params.ownership;

  DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  DWORD ex_style = 0;
  DWORD class_style = CS_DBLCLKS;

  // Set type-independent style attributes.
  if (params.child)
    style |= WS_CHILD;
  if (params.show_state == ui::SHOW_STATE_MAXIMIZED)
    style |= WS_MAXIMIZE;
  if (params.show_state == ui::SHOW_STATE_MINIMIZED)
    style |= WS_MINIMIZE;
  if (!params.accept_events)
    ex_style |= WS_EX_TRANSPARENT;
  if (!params.can_activate)
    ex_style |= WS_EX_NOACTIVATE;
  if (params.keep_on_top)
    ex_style |= WS_EX_TOPMOST;
  if (params.mirror_origin_in_rtl)
    ex_style |= l10n_util::GetExtendedTooltipStyles();
  if (params.transparent)
    ex_style |= WS_EX_LAYERED;
  if (params.has_dropshadow) {
    class_style |= (base::win::GetVersion() < base::win::VERSION_XP) ?
        0 : CS_DROPSHADOW;
  }

  // Set type-dependent style attributes.
  switch (params.type) {
    case Widget::InitParams::TYPE_WINDOW: {
      style |= WS_SYSMENU | WS_CAPTION;
      bool can_resize = GetWidget()->widget_delegate()->CanResize();
      bool can_maximize = GetWidget()->widget_delegate()->CanMaximize();
      if (can_maximize) {
        style |= WS_OVERLAPPEDWINDOW;
      } else if (can_resize) {
        style |= WS_OVERLAPPED | WS_THICKFRAME;
      }
      if (delegate_->IsDialogBox()) {
        style |= DS_MODALFRAME;
        // NOTE: Turning this off means we lose the close button, which is bad.
        // Turning it on though means the user can maximize or size the window
        // from the system menu, which is worse. We may need to provide our own
        // menu to get the close button to appear properly.
        // style &= ~WS_SYSMENU;

        // Set the WS_POPUP style for modal dialogs. This ensures that the owner
        // window is activated on destruction. This style should not be set for
        // non-modal non-top-level dialogs like constrained windows.
        style |= delegate_->IsModal() ? WS_POPUP : 0;
      }
      ex_style |= delegate_->IsDialogBox() ? WS_EX_DLGMODALFRAME : 0;
      break;
    }
    case Widget::InitParams::TYPE_CONTROL:
      style |= WS_VISIBLE;
      break;
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
      style |= WS_POPUP;
      break;
    case Widget::InitParams::TYPE_BUBBLE:
      style |= WS_POPUP;
      style |= WS_CLIPCHILDREN;
      break;
    case Widget::InitParams::TYPE_POPUP:
      style |= WS_POPUP;
      ex_style |= WS_EX_TOOLWINDOW;
      break;
    case Widget::InitParams::TYPE_MENU:
      style |= WS_POPUP;
      break;
    default:
      NOTREACHED();
  }

  set_initial_class_style(class_style);
  set_window_style(window_style() | style);
  set_window_ex_style(window_ex_style() | ex_style);

  has_non_client_view_ = Widget::RequiresNonClientView(params.type);
}

void NativeWidgetWin::RedrawInvalidRect() {
  if (!use_layered_buffer_) {
    RECT r = { 0, 0, 0, 0 };
    if (GetUpdateRect(hwnd(), &r, FALSE) && !IsRectEmpty(&r)) {
      RedrawWindow(hwnd(), &r, NULL,
                   RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);
    }
  }
}

void NativeWidgetWin::RedrawLayeredWindowContents() {
  if (invalid_rect_.IsEmpty())
    return;

  // We need to clip to the dirty rect ourselves.
  layered_window_contents_->sk_canvas()->save(SkCanvas::kClip_SaveFlag);
  layered_window_contents_->ClipRect(invalid_rect_);
  GetWidget()->GetRootView()->Paint(layered_window_contents_.get());
  layered_window_contents_->sk_canvas()->restore();

  RECT wr;
  GetWindowRect(&wr);
  SIZE size = {wr.right - wr.left, wr.bottom - wr.top};
  POINT position = {wr.left, wr.top};
  HDC dib_dc = skia::BeginPlatformPaint(layered_window_contents_->sk_canvas());
  POINT zero = {0, 0};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, layered_alpha_, AC_SRC_ALPHA};
  UpdateLayeredWindow(hwnd(), NULL, &position, &size, dib_dc, &zero,
                      RGB(0xFF, 0xFF, 0xFF), &blend, ULW_ALPHA);
  invalid_rect_.SetRect(0, 0, 0, 0);
  skia::EndPlatformPaint(layered_window_contents_->sk_canvas());
}

void NativeWidgetWin::LockUpdates() {
  // We skip locked updates when Aero is on for two reasons:
  // 1. Because it isn't necessary
  // 2. Because toggling the WS_VISIBLE flag may occur while the GPU process is
  //    attempting to present a child window's backbuffer onscreen. When these
  //    two actions race with one another, the child window will either flicker
  //    or will simply stop updating entirely.
  if (!IsAeroGlassEnabled() && ++lock_updates_count_ == 1) {
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) & ~WS_VISIBLE);
  }
  // TODO(msw): Remove nested LockUpdates VLOG info for crbug.com/93530.
  VLOG_IF(1, (lock_updates_count_ > 1)) << "Nested LockUpdates call: "
      << lock_updates_count_ << " locks for widget " << this;
}

void NativeWidgetWin::UnlockUpdates() {
  // TODO(msw): Remove nested LockUpdates VLOG info for crbug.com/93530.
  VLOG_IF(1, (lock_updates_count_ > 1)) << "Nested UnlockUpdates call: "
      << lock_updates_count_ << " locks for widget " << this;
  if (!IsAeroGlassEnabled() && --lock_updates_count_ <= 0) {
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_VISIBLE);
    lock_updates_count_ = 0;
  }
}

bool NativeWidgetWin::WidgetSizeIsClientSize() const {
  const Widget* widget = GetWidget()->GetTopLevelWidget();
  return IsZoomed() || (widget && widget->ShouldUseNativeFrame());
}

void NativeWidgetWin::ClientAreaSizeChanged() {
  RECT r;
  if (WidgetSizeIsClientSize())
    GetClientRect(&r);
  else
    GetWindowRect(&r);
  gfx::Size s(std::max(0, static_cast<int>(r.right - r.left)),
              std::max(0, static_cast<int>(r.bottom - r.top)));
  if (compositor_.get())
    compositor_->WidgetSizeChanged(s);
  delegate_->OnNativeWidgetSizeChanged(s);
  if (use_layered_buffer_) {
    layered_window_contents_.reset(
        new gfx::CanvasSkia(s.width(), s.height(), false));
  }
}

void NativeWidgetWin::ResetWindowRegion(bool force) {
  // A native frame uses the native window region, and we don't want to mess
  // with it.
  if (GetWidget()->ShouldUseNativeFrame() || !GetWidget()->non_client_view()) {
    if (force)
      SetWindowRgn(NULL, TRUE);
    return;
  }

  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  HRGN current_rgn = CreateRectRgn(0, 0, 0, 0);
  int current_rgn_result = GetWindowRgn(GetNativeView(), current_rgn);

  CRect window_rect;
  GetWindowRect(&window_rect);
  HRGN new_region;
  if (IsMaximized()) {
    HMONITOR monitor =
        MonitorFromWindow(GetNativeView(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof mi;
    GetMonitorInfo(monitor, &mi);
    CRect work_rect = mi.rcWork;
    work_rect.OffsetRect(-window_rect.left, -window_rect.top);
    new_region = CreateRectRgnIndirect(&work_rect);
  } else {
    gfx::Path window_mask;
    GetWidget()->non_client_view()->GetWindowMask(
        gfx::Size(window_rect.Width(), window_rect.Height()), &window_mask);
    new_region = window_mask.CreateNativeRegion();
  }

  if (current_rgn_result == ERROR || !EqualRgn(current_rgn, new_region)) {
    // SetWindowRgn takes ownership of the HRGN created by CreateNativeRegion.
    SetWindowRgn(new_region, TRUE);
  } else {
    DeleteObject(new_region);
  }

  DeleteObject(current_rgn);
}

LRESULT NativeWidgetWin::DefWindowProcWithRedrawLock(UINT message,
                                                     WPARAM w_param,
                                                     LPARAM l_param) {
  ScopedRedrawLock lock(this);
  // The Widget and HWND can be destroyed in the call to DefWindowProc, so use
  // the |destroyed_| flag to avoid unlocking (and crashing) after destruction.
  bool destroyed = false;
  destroyed_ = &destroyed;
  LRESULT result = DefWindowProc(GetNativeView(), message, w_param, l_param);
  if (destroyed)
    lock.CancelUnlockOperation();
  else
    destroyed_ = NULL;
  return result;
}

void NativeWidgetWin::RestoreEnabledIfNecessary() {
  if (delegate_->IsModal() && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = ::GetWindow(GetNativeView(), GW_OWNER);
    while (start) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

void NativeWidgetWin::DispatchKeyEventPostIME(const KeyEvent& key) {
  SetMsgHandled(delegate_->OnKeyEvent(key));
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::NotifyLocaleChanged() {
  NOTIMPLEMENTED();
}

namespace {
BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam) {
  Widget* widget = Widget::GetWidgetForNativeView(hwnd);
  if (widget && widget->is_secondary_widget())
    widget->Close();
  return TRUE;
}
}  // namespace

// static
void Widget::CloseAllSecondaryWidgets() {
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, 0);
}

bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  DCHECK(source);
  DCHECK(target);
  DCHECK(rect);

  HWND source_hwnd = source->GetNativeView();
  HWND target_hwnd = target->GetNativeView();
  if (source_hwnd == target_hwnd)
    return true;

  RECT win_rect = rect->ToRECT();
  if (::MapWindowPoints(source_hwnd, target_hwnd,
                        reinterpret_cast<LPPOINT>(&win_rect),
                        sizeof(RECT)/sizeof(POINT))) {
    *rect = win_rect;
    return true;
  }
  return false;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetWin(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return reinterpret_cast<NativeWidgetWin*>(
      ViewProp::GetValue(native_view, kNativeWidgetKey));
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return GetNativeWidgetForNativeView(native_window);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  if (!native_view)
    return NULL;

  // First, check if the top-level window is a Widget.
  HWND root = ::GetAncestor(native_view, GA_ROOT);
  if (!root)
    return NULL;

  NativeWidgetPrivate* widget = GetNativeWidgetForNativeView(root);
  if (widget)
    return widget;

  // Second, try to locate the last Widget window in the parent hierarchy.
  HWND parent_hwnd = native_view;
  // If we fail to find the native widget pointer for the root then it probably
  // means that the root belongs to a different process in which case we walk up
  // the native view chain looking for a parent window which corresponds to a
  // valid native widget. We only do this if we fail to find the native widget
  // for the current native view which means it is being destroyed.
  if (!widget && !GetNativeWidgetForNativeView(native_view)) {
    parent_hwnd = ::GetAncestor(parent_hwnd, GA_PARENT);
    if (!parent_hwnd)
      return NULL;
  }
  NativeWidgetPrivate* parent_widget;
  do {
    parent_widget = GetNativeWidgetForNativeView(parent_hwnd);
    if (parent_widget) {
      widget = parent_widget;
      parent_hwnd = ::GetAncestor(parent_hwnd, GA_PARENT);
    }
  } while (parent_hwnd != NULL && parent_widget != NULL);

  return widget;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  if (!native_view)
    return;

  Widget* widget = Widget::GetWidgetForNativeView(native_view);
  if (widget)
    children->insert(widget);
  EnumChildWindows(native_view, EnumerateChildWindowsForNativeWidgets,
                   reinterpret_cast<LPARAM>(children));
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  if (!native_view)
    return;

  HWND previous_parent = ::GetParent(native_view);
  if (previous_parent == new_parent)
    return;

  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);

  // First notify all the widgets that they are being disassociated
  // from their previous parent.
  for (Widget::Widgets::iterator it = widgets.begin();
       it != widgets.end(); ++it) {
    // TODO(beng): Rename this notification to NotifyNativeViewChanging()
    // and eliminate the bool parameter.
    (*it)->NotifyNativeViewHierarchyChanged(false, previous_parent);
  }

  ::SetParent(native_view, new_parent);

  // And now, notify them that they have a brand new parent.
  for (Widget::Widgets::iterator it = widgets.begin();
       it != widgets.end(); ++it) {
    (*it)->NotifyNativeViewHierarchyChanged(true, new_parent);
  }
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  return (GetKeyState(VK_LBUTTON) & 0x80) ||
    (GetKeyState(VK_RBUTTON) & 0x80) ||
    (GetKeyState(VK_MBUTTON) & 0x80) ||
    (GetKeyState(VK_XBUTTON1) & 0x80) ||
    (GetKeyState(VK_XBUTTON2) & 0x80);
}

}  // namespace internal

}  // namespace views
