// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_win.h"

#include <dwmapi.h>
#include <shellapi.h>

#include "base/i18n/rtl.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/theme_provider.h"
#include "ui/base/win/hwnd_util.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/font.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/path.h"
#include "views/accessibility/native_view_accessibility_win.h"
#include "views/widget/root_view.h"
#include "views/window/client_view.h"
#include "views/window/custom_frame_view.h"
#include "views/window/native_window_delegate.h"
#include "views/window/native_frame_view.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace views {
namespace {

static const int kDragFrameWindowAlpha = 200;

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

// Ensures that the child window stays within the boundaries of the parent
// before setting its bounds. If |parent_window| is NULL, the bounds of the
// parent are assumed to be the bounds of the monitor that |child_window| is
// nearest to. If |child_window| isn't visible yet and |insert_after_window|
// is non-NULL and visible, the monitor |insert_after_window| is on is used
// as the parent bounds instead.
void SetChildBounds(HWND child_window, HWND parent_window,
                    HWND insert_after_window, const gfx::Rect& bounds,
                    int padding, unsigned long flags) {
  DCHECK(IsWindow(child_window));

  // First figure out the bounds of the parent.
  RECT parent_rect = {0};
  if (parent_window) {
    GetClientRect(parent_window, &parent_rect);
  } else {
    // If there is no parent, we consider the bounds of the monitor the window
    // is on to be the parent bounds.

    // If the child_window isn't visible yet and we've been given a valid,
    // visible insert after window, use that window to locate the correct
    // monitor instead.
    HWND window = child_window;
    if (!IsWindowVisible(window) && IsWindow(insert_after_window) &&
        IsWindowVisible(insert_after_window))
      window = insert_after_window;

    POINT window_point = { bounds.x(), bounds.y() };
    HMONITOR monitor = MonitorFromPoint(window_point,
                                        MONITOR_DEFAULTTONEAREST);
    if (monitor) {
      MONITORINFO mi = {0};
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(monitor, &mi);
      parent_rect = mi.rcWork;
    } else {
      NOTREACHED() << "Unable to get default monitor";
    }
  }

  gfx::Rect actual_bounds = bounds;
  internal::EnsureRectIsVisibleInRect(gfx::Rect(parent_rect), &actual_bounds,
                                      padding);

  SetWindowPos(child_window, insert_after_window, actual_bounds.x(),
               actual_bounds.y(), actual_bounds.width(),
               actual_bounds.height(), flags);
}

// Returns true if edge |edge| (one of ABE_LEFT, TOP, RIGHT, or BOTTOM) of
// monitor |monitor| has an auto-hiding taskbar that's always-on-top.
bool EdgeHasTopmostAutoHideTaskbar(UINT edge, HMONITOR monitor) {
  APPBARDATA taskbar_data = { 0 };
  taskbar_data.cbSize = sizeof APPBARDATA;
  taskbar_data.uEdge = edge;
  HWND taskbar = reinterpret_cast<HWND>(SHAppBarMessage(ABM_GETAUTOHIDEBAR,
                                                        &taskbar_data));
  return ::IsWindow(taskbar) && (monitor != NULL) &&
      (MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL) == monitor) &&
      (GetWindowLong(taskbar, GWL_EXSTYLE) & WS_EX_TOPMOST);
}

HWND GetOwner(HWND window) {
  return ::GetWindow(window, GW_OWNER);
}

// Tells the window its frame (non-client area) has changed.
void SendFrameChanged(HWND window) {
  SetWindowPos(window, NULL, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS |
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION |
      SWP_NOSENDCHANGING | SWP_NOSIZE | SWP_NOZORDER);
}

// Callback used to notify child windows that the top level window received a
// DWMCompositionChanged message.
BOOL CALLBACK SendDwmCompositionChanged(HWND window, LPARAM param) {
  SendMessage(window, WM_DWMCOMPOSITIONCHANGED, 0, 0);
  return TRUE;
}

// Enables or disables the menu item for the specified command and menu.
void EnableMenuItem(HMENU menu, UINT command, bool enabled) {
  UINT flags = MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
  EnableMenuItem(menu, command, flags);
}

bool IsDwmRenderingWindowControls(HWND window) {
  DWMNCRENDERINGPOLICY policy;
  DwmGetWindowAttribute(window, DWMWA_NCRENDERING_POLICY, &policy,
                        sizeof(policy));
  return policy == DWMNCRP_ENABLED;
}

// If the hung renderer warning doesn't fit on screen, the amount of padding to
// be left between the edge of the window and the edge of the nearest monitor,
// after the window is nudged back on screen. Pixels.
const int kMonitorEdgePadding = 10;

}  // namespace

namespace internal {

void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding) {
  DCHECK(child_rect);

  // We use padding here because it allows some of the original web page to
  // bleed through around the edges.
  int twice_padding = padding * 2;

  // FIRST, clamp width and height so we don't open child windows larger than
  // the containing parent.
  if (child_rect->width() > (parent_rect.width() + twice_padding))
    child_rect->set_width(std::max(0, parent_rect.width() - twice_padding));
  if (child_rect->height() > parent_rect.height() + twice_padding)
    child_rect->set_height(std::max(0, parent_rect.height() - twice_padding));

  // SECOND, clamp x,y position to padding,padding so we don't position child
  // windows in hyperspace.
  // TODO(mpcomplete): I don't see what the second check in each 'if' does that
  // isn't handled by the LAST set of 'ifs'.  Maybe we can remove it.
  if (child_rect->x() < parent_rect.x() ||
      child_rect->x() > parent_rect.right()) {
    child_rect->set_x(parent_rect.x() + padding);
  }
  if (child_rect->y() < parent_rect.y() ||
      child_rect->y() > parent_rect.bottom()) {
    child_rect->set_y(parent_rect.y() + padding);
  }

  // LAST, nudge the window back up into the client area if its x,y position is
  // within the parent bounds but its width/height place it off-screen.
  if (child_rect->bottom() > parent_rect.bottom())
    child_rect->set_y(parent_rect.bottom() - child_rect->height() - padding);
  if (child_rect->right() > parent_rect.right())
    child_rect->set_x(parent_rect.right() - child_rect->width() - padding);
}

}  // namespace internal

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
// IMPORTANT: Do not use this scoping object for large scopes or periods of
//            time! IT WILL PREVENT THE WINDOW FROM BEING REDRAWN! (duh).
//
// I would love to hear Raymond Chen's explanation for all this. And maybe a
// list of other messages that this applies to ;-)
class WindowWin::ScopedRedrawLock {
 public:
  explicit ScopedRedrawLock(WindowWin* window) : window_(window) {
    window_->LockUpdates();
  }

  ~ScopedRedrawLock() {
    window_->UnlockUpdates();
  }

 private:
  // The window having its style changed.
  WindowWin* window_;
};

HCURSOR WindowWin::resize_cursors_[6];

////////////////////////////////////////////////////////////////////////////////
// WindowWin, public:

WindowWin::~WindowWin() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  WindowWin* window = new WindowWin(window_delegate);
  window->GetWindow()->non_client_view()->SetFrameView(
      window->CreateFrameViewForWindow());
  window->Init(parent, bounds);
  return window;
}

void WindowWin::Show(int show_state) {
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

void WindowWin::PushForceHidden() {
  if (force_hidden_count_++ == 0)
    Hide();
}

void WindowWin::PopForceHidden() {
  if (--force_hidden_count_ <= 0) {
    force_hidden_count_ = 0;
    ShowWindow(SW_SHOW);
  }
}

// static
gfx::Font WindowWin::GetWindowTitleFont() {
  NONCLIENTMETRICS ncm;
  base::win::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  base::win::ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
}

///////////////////////////////////////////////////////////////////////////////
// WindowWin, protected:

WindowWin::WindowWin(WindowDelegate* window_delegate)
    : WidgetWin(),
      Window(window_delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)),
      focus_on_creation_(true),
      restored_enabled_(false),
      fullscreen_(false),
      is_active_(false),
      lock_updates_(false),
      saved_window_style_(0),
      ignore_window_pos_changes_(false),
      ignore_pos_changes_factory_(this),
      force_hidden_count_(0),
      is_right_mouse_pressed_on_caption_(false),
      last_monitor_(NULL) {
  set_native_window(this);
  is_window_ = true;
  InitClass();
  // Initialize these values to 0 so that subclasses can override the default
  // behavior before calling Init.
  set_window_style(0);
  set_window_ex_style(0);
}

void WindowWin::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  if (window_style() == 0)
    set_window_style(CalculateWindowStyle());
  if (window_ex_style() == 0)
    set_window_ex_style(CalculateWindowExStyle());

  GetMonitorAndRects(bounds.ToRECT(), &last_monitor_, &last_monitor_rect_,
                     &last_work_area_);

  WidgetWin::Init(parent, bounds);

  ui::SetWindowUserData(GetNativeView(), this);
  delegate_->OnNativeWindowCreated(bounds);
}

gfx::Insets WindowWin::GetClientAreaInsets() const {
  // Returning an empty Insets object causes the default handling in
  // WidgetWin::OnNCCalcSize() to be invoked.
  if (delegate_->IsUsingNativeFrame())
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

int WindowWin::GetShowState() const {
  return SW_SHOWNORMAL;
}

///////////////////////////////////////////////////////////////////////////////
// WindowWin, WidgetWin overrides:

void WindowWin::OnActivateApp(BOOL active, DWORD thread_id) {
  if (!active && thread_id != GetCurrentThreadId()) {
    // Another application was activated, we should reset any state that
    // disables inactive rendering now.
    delegate_->EnableInactiveRendering();
    // Update the native frame too, since it could be rendering the non-client
    // area.
    CallDefaultNCActivateHandler(FALSE);
  }
}

LRESULT WindowWin::OnAppCommand(HWND window, short app_command, WORD device,
                                int keystate) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  if (!GetWindow()->window_delegate()->ExecuteWindowsCommand(app_command))
    return WidgetWin::OnAppCommand(window, app_command, device, keystate);
  return 0;
}

void WindowWin::OnClose() {
  GetWindow()->CloseWindow();
}

void WindowWin::OnCommand(UINT notification_code, int command_id, HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 ||
      GetWindow()->window_delegate()->ExecuteWindowsCommand(command_id)) {
    WidgetWin::OnCommand(notification_code, command_id, window);
  }
}

void WindowWin::OnDestroy() {
  delegate_->OnNativeWindowDestroying();

  RestoreEnabledIfNecessary();

  // If the user activates another app after opening us, then comes back and
  // closes us, we want our owner to gain activation.  But only if the owner
  // is visible. If we don't manually force that here, the other app will
  // regain activation instead.
  HWND owner = GetOwner(GetNativeView());
  if (owner && GetNativeView() == GetForegroundWindow() &&
      IsWindowVisible(owner)) {
    SetForegroundWindow(owner);
  }

  WidgetWin::OnDestroy();
}

LRESULT WindowWin::OnDwmCompositionChanged(UINT msg, WPARAM w_param,
                                           LPARAM l_param) {
  // For some reason, we need to hide the window while we're changing the frame
  // type only when we're changing it in response to WM_DWMCOMPOSITIONCHANGED.
  // If we don't, the client area will be filled with black. I'm suspecting
  // something skia-ey.
  // Frame type toggling caused by the user (e.g. switching theme) doesn't seem
  // to have this requirement.
  FrameTypeChanged();
  return 0;
}

void WindowWin::OnEnterSizeMove() {
  WidgetWin::OnEnterSizeMove();
  delegate_->OnNativeWindowBeginUserBoundsChange();
}

void WindowWin::OnExitSizeMove() {
  WidgetWin::OnExitSizeMove();
  delegate_->OnNativeWindowEndUserBoundsChange();
}

void WindowWin::OnFinalMessage(HWND window) {
  delegate_->OnNativeWindowDestroyed();
  WidgetWin::OnFinalMessage(window);
}

void WindowWin::OnGetMinMaxInfo(MINMAXINFO* minmax_info) {
  gfx::Size min_window_size(delegate_->GetMinimumSize());
  minmax_info->ptMinTrackSize.x = min_window_size.width();
  minmax_info->ptMinTrackSize.y = min_window_size.height();
  WidgetWin::OnGetMinMaxInfo(minmax_info);
}

void WindowWin::OnInitMenu(HMENU menu) {
  // We only need to manually enable the system menu if we're not using a native
  // frame.
  if (delegate_->IsUsingNativeFrame())
    WidgetWin::OnInitMenu(menu);

  bool is_fullscreen = IsFullscreen();
  bool is_minimized = IsMinimized();
  bool is_maximized = IsMaximized();
  bool is_restored = !is_fullscreen && !is_minimized && !is_maximized;

  ScopedRedrawLock lock(this);
  EnableMenuItem(menu, SC_RESTORE, is_minimized || is_maximized);
  EnableMenuItem(menu, SC_MOVE, is_restored);
  EnableMenuItem(menu, SC_SIZE,
                 GetWindow()->window_delegate()->CanResize() && is_restored);
  EnableMenuItem(menu, SC_MAXIMIZE,
                 GetWindow()->window_delegate()->CanMaximize() &&
                     !is_fullscreen && !is_maximized);
  EnableMenuItem(menu, SC_MINIMIZE,
                 GetWindow()->window_delegate()->CanMaximize() &&
                     !is_minimized);
}

LRESULT WindowWin::OnMouseActivate(UINT message, WPARAM w_param,
                                   LPARAM l_param) {
  return delegate_->CanActivate() ? MA_ACTIVATE : MA_NOACTIVATEANDEAT;
}

LRESULT WindowWin::OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param) {
  if (message == WM_RBUTTONUP) {
    if (is_right_mouse_pressed_on_caption_) {
      is_right_mouse_pressed_on_caption_ = false;
      ReleaseCapture();
      // |point| is in window coordinates, but WM_NCHITTEST and TrackPopupMenu()
      // expect screen coordinates.
      CPoint screen_point(l_param);
      MapWindowPoints(GetNativeView(), HWND_DESKTOP, &screen_point, 1);
      w_param = SendMessage(GetNativeView(), WM_NCHITTEST, 0,
                            MAKELPARAM(screen_point.x, screen_point.y));
      if (w_param == HTCAPTION || w_param == HTSYSMENU) {
        UINT flags = TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD;
        if (base::i18n::IsRTL())
          flags |= TPM_RIGHTALIGN;
        HMENU system_menu = GetSystemMenu(GetNativeView(), FALSE);
        int id = TrackPopupMenu(system_menu, flags, screen_point.x,
                                screen_point.y, 0, GetNativeView(), NULL);
        ExecuteSystemMenuCommand(id);
        return 0;
      }
    }
  }

  WidgetWin::OnMouseRange(message, w_param, l_param);
  return 0;
}


LRESULT WindowWin::OnNCActivate(BOOL active) {
  if (!delegate_->CanActivate())
    return TRUE;

  is_active_ = !!active;
  delegate_->OnNativeWindowActivationChanged(is_active_);

  // The frame may need to redraw as a result of the activation change.
  // We can get WM_NCACTIVATE before we're actually visible. If we're not
  // visible, no need to paint.
  if (IsVisible())
    GetWindow()->non_client_view()->SchedulePaint();

  // If we're active again, we should be allowed to render as inactive, so
  // tell the non-client view.
  bool inactive_rendering_disabled = delegate_->IsInactiveRenderingDisabled();
  if (IsActive())
    delegate_->EnableInactiveRendering();

  return CallDefaultNCActivateHandler(inactive_rendering_disabled || active);
}

LRESULT WindowWin::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only override the default handling if we need to specify a custom
  // non-client edge width. Note that in most cases "no insets" means no
  // custom width, but in fullscreen mode we want a custom width of 0.
  gfx::Insets insets = GetClientAreaInsets();
  if (insets.empty() && !IsFullscreen())
    return WidgetWin::OnNCCalcSize(mode, l_param);

  RECT* client_rect = mode ?
      &reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0] :
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
    if (EdgeHasTopmostAutoHideTaskbar(ABE_LEFT, monitor))
      client_rect->left += kAutoHideTaskbarThicknessPx;
    if (EdgeHasTopmostAutoHideTaskbar(ABE_TOP, monitor)) {
      if (delegate_->IsUsingNativeFrame()) {
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
    if (EdgeHasTopmostAutoHideTaskbar(ABE_RIGHT, monitor))
      client_rect->right -= kAutoHideTaskbarThicknessPx;
    if (EdgeHasTopmostAutoHideTaskbar(ABE_BOTTOM, monitor))
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

LRESULT WindowWin::OnNCHitTest(const CPoint& point) {
  // If the DWM is rendering the window controls, we need to give the DWM's
  // default window procedure first chance to handle hit testing.
  if (IsDwmRenderingWindowControls(GetNativeView())) {
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
  return WidgetWin::OnNCHitTest(point);
}

LRESULT WindowWin::OnNCMouseRange(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) {
  // When we're using a native frame, window controls work without us
  // interfering.
  if (message == WM_NCLBUTTONDOWN && !delegate_->IsUsingNativeFrame()) {
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
        ProcessMousePressed(message, w_param, l_param);
        return 0;
      }
    }
  } else if (message == WM_NCRBUTTONDOWN &&
      (w_param == HTCAPTION || w_param == HTSYSMENU)) {
    is_right_mouse_pressed_on_caption_ = true;
    // We SetCapture() to ensure we only show the menu when the button down and
    // up are both on the caption.  Note: this causes the button up to be
    // WM_RBUTTONUP instead of WM_NCRBUTTONUP.
    SetCapture();
  }

  WidgetWin::OnNCMouseRange(message, w_param, l_param);

  /* TODO(beng): Fix the standard non-client over-painting bug. This code
                 doesn't work but identifies the problem.
  if (message == WM_NCLBUTTONDOWN && !IsMsgHandled()) {
    // WindowWin::OnNCLButtonDown set the message as unhandled. This normally
    // means WidgetWin::ProcessWindowMessage will pass it to
    // DefWindowProc. Sadly, DefWindowProc for WM_NCLBUTTONDOWN does weird
    // non-client painting, so we need to call it directly here inside a
    // scoped update lock.
    ScopedRedrawLock lock(this);
    DefWindowProc(GetNativeView(), WM_NCLBUTTONDOWN, w_param, l_param);
    SetMsgHandled(TRUE);
  }
  */

  return 0;
}

void WindowWin::OnNCPaint(HRGN rgn) {
  // When using a custom frame, we want to avoid calling DefWindowProc() since
  // that may render artifacts.
  SetMsgHandled(!delegate_->IsUsingNativeFrame());
}

LRESULT WindowWin::OnNCUAHDrawCaption(UINT msg, WPARAM w_param,
                                      LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(!delegate_->IsUsingNativeFrame());
  return 0;
}

LRESULT WindowWin::OnNCUAHDrawFrame(UINT msg, WPARAM w_param,
                                    LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(!delegate_->IsUsingNativeFrame());
  return 0;
}

LRESULT WindowWin::OnSetIcon(UINT size_type, HICON new_icon) {
  // This shouldn't hurt even if we're using the native frame.
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetNativeView(), WM_SETICON, size_type,
                       reinterpret_cast<LPARAM>(new_icon));
}

LRESULT WindowWin::OnSetText(const wchar_t* text) {
  // This shouldn't hurt even if we're using the native frame.
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetNativeView(), WM_SETTEXT, NULL,
                       reinterpret_cast<LPARAM>(text));
}

void WindowWin::OnSettingChange(UINT flags, const wchar_t* section) {
  if (!GetParent() && (flags == SPI_SETWORKAREA)) {
    // Fire a dummy SetWindowPos() call, so we'll trip the code in
    // OnWindowPosChanging() below that notices work area changes.
    ::SetWindowPos(GetNativeView(), 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
        SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetMsgHandled(TRUE);
  } else {
    WidgetWin::OnSettingChange(flags, section);
  }
}

void WindowWin::OnSize(UINT size_param, const CSize& new_size) {
  delegate_->OnNativeWindowBoundsChanged();
  RedrawWindow(GetNativeView(), NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);

  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  ResetWindowRegion(false);
}

void WindowWin::OnSysCommand(UINT notification_code, CPoint click) {
  // Windows uses the 4 lower order bits of |notification_code| for type-
  // specific information so we must exclude this when comparing.
  static const int sc_mask = 0xFFF0;
  // Ignore size/move/maximize in fullscreen mode.
  if (IsFullscreen() &&
      (((notification_code & sc_mask) == SC_SIZE) ||
       ((notification_code & sc_mask) == SC_MOVE) ||
       ((notification_code & sc_mask) == SC_MAXIMIZE)))
    return;
  if (!delegate_->IsUsingNativeFrame()) {
    if ((notification_code & sc_mask) == SC_MINIMIZE ||
        (notification_code & sc_mask) == SC_MAXIMIZE ||
        (notification_code & sc_mask) == SC_RESTORE) {
      GetWindow()->non_client_view()->ResetWindowControls();
    } else if ((notification_code & sc_mask) == SC_MOVE ||
               (notification_code & sc_mask) == SC_SIZE) {
      if (lock_updates_) {
        // We were locked, before entering a resize or move modal loop. Now that
        // we've begun to move the window, we need to unlock updates so that the
        // sizing/moving feedback can be continuous.
        UnlockUpdates();
      }
    }
  }

  // Handle SC_KEYMENU, which means that the user has pressed the ALT
  // key and released it, so we should focus the menu bar.
  if ((notification_code & sc_mask) == SC_KEYMENU && click.x == 0) {
    // Retrieve the status of shift and control keys to prevent consuming
    // shift+alt keys, which are used by Windows to change input languages.
    Accelerator accelerator(ui::KeyboardCodeForWindowsKeyCode(VK_MENU),
                            !!(GetKeyState(VK_SHIFT) & 0x8000),
                            !!(GetKeyState(VK_CONTROL) & 0x8000),
                            false);
    GetFocusManager()->ProcessAccelerator(accelerator);
    return;
  }

  // If the delegate can't handle it, the system implementation will be called.
  if (!delegate_->ExecuteCommand(notification_code)) {
    DefWindowProc(GetNativeView(), WM_SYSCOMMAND, notification_code,
                  MAKELPARAM(click.x, click.y));
  }
}

void WindowWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  if (force_hidden_count_) {
    // Prevent the window from being made visible if we've been asked to do so.
    // See comment in header as to why we might want this.
    window_pos->flags &= ~SWP_SHOWWINDOW;
  }

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
        DCHECK(ignore_pos_changes_factory_.empty());
        MessageLoop::current()->PostTask(FROM_HERE,
            ignore_pos_changes_factory_.NewRunnableMethod(
            &WindowWin::StopIgnoringPosChanges));
      }
      last_monitor_ = monitor;
      last_monitor_rect_ = monitor_rect;
      last_work_area_ = work_area;
    }
  }

  WidgetWin::OnWindowPosChanging(window_pos);
}

////////////////////////////////////////////////////////////////////////////////
// WindowWin, NativeWindow implementation:

NativeWidget* WindowWin::AsNativeWidget() {
  return this;
}

const NativeWidget* WindowWin::AsNativeWidget() const {
  return this;
}

gfx::Rect WindowWin::GetRestoredBounds() const {
  // If we're in fullscreen mode, we've changed the normal bounds to the monitor
  // rect, so return the saved bounds instead.
  if (IsFullscreen())
    return gfx::Rect(saved_window_info_.window_rect);

  gfx::Rect bounds;
  GetWindowBoundsAndMaximizedState(&bounds, NULL);
  return bounds;
}

void WindowWin::ShowNativeWindow(ShowState state) {
  Show(state == SHOW_MAXIMIZED ? SW_SHOWMAXIMIZED : GetShowState());
}

void WindowWin::BecomeModal() {
  // We implement modality by crawling up the hierarchy of windows starting
  // at the owner, disabling all of them so that they don't receive input
  // messages.
  HWND start = GetOwner(GetNativeView());
  while (start) {
    ::EnableWindow(start, FALSE);
    start = ::GetParent(start);
  }
}

void WindowWin::CenterWindow(const gfx::Size& size) {
  HWND parent = GetParent();
  if (!IsWindow())
    parent = GetOwner(GetNativeView());
  ui::CenterAndSizeWindow(parent, GetNativeView(), size, false);
}

void WindowWin::GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                 bool* maximized) const {
  WINDOWPLACEMENT wp;
  wp.length = sizeof(wp);
  const bool succeeded = !!GetWindowPlacement(GetNativeView(), &wp);
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

  if (maximized != NULL)
    *maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
}

void WindowWin::EnableClose(bool enable) {
  // Disable the native frame's close button regardless of whether or not the
  // native frame is in use, since this also affects the system menu.
  EnableMenuItem(GetSystemMenu(GetNativeView(), false), SC_CLOSE, enable);
  SendFrameChanged(GetNativeView());
}

void WindowWin::SetWindowTitle(const std::wstring& title) {
  SetWindowText(GetNativeView(), title.c_str());
  SetAccessibleName(title);
}

void WindowWin::SetWindowIcons(const SkBitmap& window_icon,
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

void WindowWin::SetAccessibleName(const std::wstring& name) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(name.c_str());
    hr = pAccPropServices->SetHwndProp(GetNativeView(), OBJID_CLIENT,
                                       CHILDID_SELF, PROPID_ACC_NAME, var);
  }
}

void WindowWin::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
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

void WindowWin::SetAccessibleState(ui::AccessibilityTypes::State state) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    if (state) {
      var.lVal = NativeViewAccessibilityWin::MSAAState(state);
      hr = pAccPropServices->SetHwndProp(GetNativeView(), OBJID_CLIENT,
                                         CHILDID_SELF, PROPID_ACC_STATE, var);
    }
  }
}

void WindowWin::SetWindowBounds(const gfx::Rect& bounds,
                                gfx::NativeWindow other_window) {
  SetChildBounds(GetNativeView(), GetParent(), other_window, bounds,
                 kMonitorEdgePadding, 0);
}

void WindowWin::HideWindow() {
  // We can just call the function implemented by the widget.
  Hide();
}

void WindowWin::Activate() {
  if (IsMinimized())
    ::ShowWindow(GetNativeView(), SW_RESTORE);
  ::SetWindowPos(GetNativeView(), HWND_TOP, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(GetNativeView());
}

void WindowWin::Deactivate() {
  HWND hwnd = ::GetNextWindow(GetNativeView(), GW_HWNDNEXT);
  if (hwnd)
    ::SetForegroundWindow(hwnd);
}

void WindowWin::Maximize() {
  ExecuteSystemMenuCommand(SC_MAXIMIZE);
}

void WindowWin::Minimize() {
  ExecuteSystemMenuCommand(SC_MINIMIZE);
}

void WindowWin::Restore() {
  ExecuteSystemMenuCommand(SC_RESTORE);
}

bool WindowWin::IsActive() const {
  return is_active_;
}

bool WindowWin::IsVisible() const {
  return !!::IsWindowVisible(GetNativeView());
}

bool WindowWin::IsMaximized() const {
  return !!::IsZoomed(GetNativeView());
}

bool WindowWin::IsMinimized() const {
  return !!::IsIconic(GetNativeView());
}

void WindowWin::SetFullscreen(bool fullscreen) {
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

  // Toggle fullscreen mode.
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

bool WindowWin::IsFullscreen() const {
  return fullscreen_;
}

void WindowWin::SetAlwaysOnTop(bool always_on_top) {
  ::SetWindowPos(GetNativeView(), always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

bool WindowWin::IsAppWindow() const {
  return false;
}

void WindowWin::SetUseDragFrame(bool use_drag_frame) {
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

NonClientFrameView* WindowWin::CreateFrameViewForWindow() {
  if (ShouldUseNativeFrame())
    return new NativeFrameView(this);
  return new CustomFrameView(this);
}

void WindowWin::UpdateFrameAfterFrameChange() {
  // We've either gained or lost a custom window region, so reset it now.
  ResetWindowRegion(true);
}

gfx::NativeWindow WindowWin::GetNativeWindow() const {
  return GetNativeView();
}

bool WindowWin::ShouldUseNativeFrame() const {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (!tp)
    return WidgetWin::IsAeroGlassEnabled();
  return tp->ShouldUseNativeFrame();
}

void WindowWin::FrameTypeChanged() {
  // Called when the frame type could possibly be changing (theme change or
  // DWM composition change).
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // We need to toggle the rendering policy of the DWM/glass frame as we
    // change from opaque to glass. "Non client rendering enabled" means that
    // the DWM's glass non-client rendering is enabled, which is why
    // DWMNCRP_ENABLED is used for the native frame case. _DISABLED means the
    // DWM doesn't render glass, and so is used in the custom frame case.
    DWMNCRENDERINGPOLICY policy =
        delegate_->IsUsingNativeFrame() ? DWMNCRP_ENABLED : DWMNCRP_DISABLED;
    DwmSetWindowAttribute(GetNativeView(), DWMWA_NCRENDERING_POLICY,
                          &policy, sizeof(DWMNCRENDERINGPOLICY));
  }

  // Send a frame change notification, since the non-client metrics have
  // changed.
  SendFrameChanged(GetNativeView());

  // Update the non-client view with the correct frame view for the active frame
  // type.
  GetWindow()->non_client_view()->UpdateFrame();

  // WM_DWMCOMPOSITIONCHANGED is only sent to top level windows, however we want
  // to notify our children too, since we can have MDI child windows who need to
  // update their appearance.
  EnumChildWindows(GetNativeView(), &SendDwmCompositionChanged, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// WindowWin, private:

void WindowWin::SetInitialFocus() {
  if (!focus_on_creation_)
    return;

  View* v = GetWindow()->window_delegate()->GetInitiallyFocusedView();
  if (v) {
    v->RequestFocus();
  } else {
    // The window does not get keyboard messages unless we focus it, not sure
    // why.
    SetFocus(GetNativeView());
  }
}

void WindowWin::RestoreEnabledIfNecessary() {
  if (delegate_->IsModal() && !restored_enabled_) {
    restored_enabled_ = true;
    // If we were run modally, we need to undo the disabled-ness we inflicted on
    // the owner's parent hierarchy.
    HWND start = GetOwner(GetNativeView());
    while (start) {
      ::EnableWindow(start, TRUE);
      start = ::GetParent(start);
    }
  }
}

DWORD WindowWin::CalculateWindowStyle() {
  DWORD window_styles =
      WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SYSMENU | WS_CAPTION;
  bool can_resize = GetWindow()->window_delegate()->CanResize();
  bool can_maximize = GetWindow()->window_delegate()->CanMaximize();
  if (can_maximize) {
    window_styles |= WS_OVERLAPPEDWINDOW;
  } else if (can_resize) {
    window_styles |= WS_OVERLAPPED | WS_THICKFRAME;
  }
  if (delegate_->IsDialogBox()) {
    window_styles |= DS_MODALFRAME;
    // NOTE: Turning this off means we lose the close button, which is bad.
    // Turning it on though means the user can maximize or size the window
    // from the system menu, which is worse. We may need to provide our own
    // menu to get the close button to appear properly.
    // window_styles &= ~WS_SYSMENU;
  }
  return window_styles;
}

DWORD WindowWin::CalculateWindowExStyle() {
  return delegate_->IsDialogBox() ? WS_EX_DLGMODALFRAME : 0;
}

void WindowWin::LockUpdates() {
  lock_updates_ = true;
  saved_window_style_ = GetWindowLong(GWL_STYLE);
  SetWindowLong(GWL_STYLE, saved_window_style_ & ~WS_VISIBLE);
}

void WindowWin::UnlockUpdates() {
  SetWindowLong(GWL_STYLE, saved_window_style_);
  lock_updates_ = false;
}

void WindowWin::ResetWindowRegion(bool force) {
  // A native frame uses the native window region, and we don't want to mess
  // with it.
  if (delegate_->IsUsingNativeFrame()) {
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
    GetWindow()->non_client_view()->GetWindowMask(
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

LRESULT WindowWin::CallDefaultNCActivateHandler(BOOL active) {
  // The DefWindowProc handling for WM_NCACTIVATE renders the classic-look
  // window title bar directly, so we need to use a redraw lock here to prevent
  // it from doing so.
  ScopedRedrawLock lock(this);
  return DefWindowProc(GetNativeView(), WM_NCACTIVATE, active, 0);
}

void WindowWin::ExecuteSystemMenuCommand(int command) {
  if (command)
    SendMessage(GetNativeView(), WM_SYSCOMMAND, command, 0);
}

void WindowWin::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    resize_cursors_[RC_NORMAL] = LoadCursor(NULL, IDC_ARROW);
    resize_cursors_[RC_VERTICAL] = LoadCursor(NULL, IDC_SIZENS);
    resize_cursors_[RC_HORIZONTAL] = LoadCursor(NULL, IDC_SIZEWE);
    resize_cursors_[RC_NESW] = LoadCursor(NULL, IDC_SIZENESW);
    resize_cursors_[RC_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
    initialized = true;
  }
}

namespace {
BOOL CALLBACK WindowCallbackProc(HWND hwnd, LPARAM lParam) {
  NativeWidget* native_widget =
      NativeWidget::GetNativeWidgetForNativeView(hwnd);
  if (native_widget)
    Window::CloseSecondaryWidget(native_widget->GetWidget());
  return TRUE;
}
}  // namespace

void Window::CloseAllSecondaryWindows() {
  EnumThreadWindows(GetCurrentThreadId(), WindowCallbackProc, 0);
}

}  // namespace views
