// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/native_window_win.h"

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
#include "ui/gfx/path.h"
#include "views/accessibility/native_view_accessibility_win.h"
#include "views/window/client_view.h"
#include "views/window/native_window_delegate.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace views {
namespace {

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

////////////////////////////////////////////////////////////////////////////////
// NativeWindowWin, public:

NativeWindowWin::NativeWindowWin(internal::NativeWindowDelegate* delegate)
    : NativeWidgetWin(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      restored_enabled_(false),
      ignore_window_pos_changes_(false),
      ignore_pos_changes_factory_(this),
      is_right_mouse_pressed_on_caption_(false),
      last_monitor_(NULL) {
  is_window_ = true;
  // Initialize these values to 0 so that subclasses can override the default
  // behavior before calling Init.
  set_window_style(0);
  set_window_ex_style(0);
}

NativeWindowWin::~NativeWindowWin() {
}

// static
gfx::Font NativeWindowWin::GetWindowTitleFont() {
  NONCLIENTMETRICS ncm;
  base::win::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  base::win::ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
}

///////////////////////////////////////////////////////////////////////////////
// NativeWindowWin, NativeWidgetWin overrides:

void NativeWindowWin::InitNativeWidget(const Widget::InitParams& params) {
  if (window_style() == 0)
    set_window_style(CalculateWindowStyle());
  if (window_ex_style() == 0)
    set_window_ex_style(CalculateWindowExStyle());

  GetMonitorAndRects(params.bounds.ToRECT(), &last_monitor_,
                     &last_monitor_rect_, &last_work_area_);

  NativeWidgetWin::InitNativeWidget(params);
}

void NativeWindowWin::OnDestroy() {
  RestoreEnabledIfNecessary();
  NativeWidgetWin::OnDestroy();
}

LRESULT NativeWindowWin::OnMouseRange(UINT message,
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
      UINT flags = TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD;
      if (base::i18n::IsRTL())
        flags |= TPM_RIGHTALIGN;
      HMENU system_menu = GetSystemMenu(GetNativeView(), FALSE);
      int id = TrackPopupMenu(system_menu, flags, screen_point.x,
                              screen_point.y, 0, GetNativeView(), NULL);
      ExecuteSystemMenuCommand(id);
      return 0;
    }
  } else if (message == WM_NCLBUTTONDOWN &&
             !GetWindow()->ShouldUseNativeFrame()) {
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

  /*
  TODO(beng): This fixes some situations where the windows-classic appearance
              non-client area is rendered over our custom frame, however it
              causes mouse-releases to the non-client area to be eaten, so it
              can't be enabled.
  if (message == WM_NCLBUTTONDOWN) {
    // NativeWindowWin::OnNCLButtonDown set the message as un-handled. This
    // normally means NativeWidgetWin::ProcessWindowMessage will pass it to
    // DefWindowProc. Sadly, DefWindowProc for WM_NCLBUTTONDOWN does weird
    // non-client painting, so we need to call it directly here inside a
    // scoped update lock.
    ScopedRedrawLock lock(this);
    NativeWidgetWin::OnMouseRange(message, w_param, l_param);
    DefWindowProc(GetNativeView(), WM_NCLBUTTONDOWN, w_param, l_param);
    SetMsgHandled(TRUE);
  }
  */

  NativeWidgetWin::OnMouseRange(message, w_param, l_param);
  return 0;
}

LRESULT NativeWindowWin::OnNCCalcSize(BOOL mode, LPARAM l_param) {
  // We only override the default handling if we need to specify a custom
  // non-client edge width. Note that in most cases "no insets" means no
  // custom width, but in fullscreen mode we want a custom width of 0.
  gfx::Insets insets = GetClientAreaInsets();
  if (insets.empty() && !IsFullscreen())
    return NativeWidgetWin::OnNCCalcSize(mode, l_param);

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
      if (GetWindow()->ShouldUseNativeFrame()) {
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

namespace {
struct ClipState {
  // The window being painted.
  HWND parent;

  // DC painting to.
  HDC dc;

  // Origin of the window in terms of the screen.
  int x;
  int y;
};

// See comments in OnNCPaint for details of this function.
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
}  // namespace

void NativeWindowWin::OnNCPaint(HRGN rgn) {
  // We only do non-client painting if we're not using the native frame.
  // It's required to avoid some native painting artifacts from appearing when
  // the window is resized.
  if (GetWindow()->ShouldUseNativeFrame()) {
    NativeWidgetWin::OnNCPaint(rgn);
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
    delegate_->AsNativeWidgetDelegate()->OnNativeWidgetPaint(&canvas);
  }

  ReleaseDC(GetNativeView(), dc);
  // When using a custom frame, we want to avoid calling DefWindowProc() since
  // that may render artifacts.
  SetMsgHandled(!GetWindow()->ShouldUseNativeFrame());
}

void NativeWindowWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
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
            &NativeWindowWin::StopIgnoringPosChanges));
      }
      last_monitor_ = monitor;
      last_monitor_rect_ = monitor_rect;
      last_work_area_ = work_area;
    }
  }

  NativeWidgetWin::OnWindowPosChanging(window_pos);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowWin, NativeWindow implementation:

Window* NativeWindowWin::GetWindow() {
  return delegate_->AsWindow();
}

const Window* NativeWindowWin::GetWindow() const {
  return delegate_->AsWindow();
}

NativeWidget* NativeWindowWin::AsNativeWidget() {
  return this;
}

const NativeWidget* NativeWindowWin::AsNativeWidget() const {
  return this;
}

void NativeWindowWin::BecomeModal() {
  // We implement modality by crawling up the hierarchy of windows starting
  // at the owner, disabling all of them so that they don't receive input
  // messages.
  HWND start = GetOwner(GetNativeView());
  while (start) {
    ::EnableWindow(start, FALSE);
    start = ::GetParent(start);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowWin, private:

void NativeWindowWin::RestoreEnabledIfNecessary() {
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

DWORD NativeWindowWin::CalculateWindowStyle() {
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

DWORD NativeWindowWin::CalculateWindowExStyle() {
  return delegate_->IsDialogBox() ? WS_EX_DLGMODALFRAME : 0;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindow, public:

// static
NativeWindow* NativeWindow::CreateNativeWindow(
    internal::NativeWindowDelegate* delegate) {
  return new NativeWindowWin(delegate);
}

}  // namespace views
