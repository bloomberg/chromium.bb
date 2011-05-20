// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/native_window_gtk.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "views/events/event.h"
#include "views/screen.h"
#include "views/window/custom_frame_view.h"
#include "views/window/hit_test.h"
#include "views/window/native_window_delegate.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace {

// Converts a Windows-style hit test result code into a GDK window edge.
GdkWindowEdge HitTestCodeToGDKWindowEdge(int hittest_code) {
  switch (hittest_code) {
    case HTBOTTOM:
      return GDK_WINDOW_EDGE_SOUTH;
    case HTBOTTOMLEFT:
      return GDK_WINDOW_EDGE_SOUTH_WEST;
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
      return GDK_WINDOW_EDGE_SOUTH_EAST;
    case HTLEFT:
      return GDK_WINDOW_EDGE_WEST;
    case HTRIGHT:
      return GDK_WINDOW_EDGE_EAST;
    case HTTOP:
      return GDK_WINDOW_EDGE_NORTH;
    case HTTOPLEFT:
      return GDK_WINDOW_EDGE_NORTH_WEST;
    case HTTOPRIGHT:
      return GDK_WINDOW_EDGE_NORTH_EAST;
    default:
      NOTREACHED();
      break;
  }
  // Default to something defaultish.
  return HitTestCodeToGDKWindowEdge(HTGROWBOX);
}

// Converts a Windows-style hit test result code into a GDK cursor type.
GdkCursorType HitTestCodeToGdkCursorType(int hittest_code) {
  switch (hittest_code) {
    case HTBOTTOM:
      return GDK_BOTTOM_SIDE;
    case HTBOTTOMLEFT:
      return GDK_BOTTOM_LEFT_CORNER;
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
      return GDK_BOTTOM_RIGHT_CORNER;
    case HTLEFT:
      return GDK_LEFT_SIDE;
    case HTRIGHT:
      return GDK_RIGHT_SIDE;
    case HTTOP:
      return GDK_TOP_SIDE;
    case HTTOPLEFT:
      return GDK_TOP_LEFT_CORNER;
    case HTTOPRIGHT:
      return GDK_TOP_RIGHT_CORNER;
    default:
      break;
  }
  // Default to something defaultish.
  return GDK_LEFT_PTR;
}

}  // namespace

namespace views {

NativeWindowGtk::NativeWindowGtk(internal::NativeWindowDelegate* delegate)
    : NativeWidgetGtk(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      window_state_(GDK_WINDOW_STATE_WITHDRAWN),
      window_closed_(false) {
  is_window_ = true;
}

NativeWindowGtk::~NativeWindowGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, NativeWidgetGtk overrides:

gboolean NativeWindowGtk::OnButtonPress(GtkWidget* widget,
                                        GdkEventButton* event) {
  GdkEventButton transformed_event = *event;
  MouseEvent mouse_event(TransformEvent(&transformed_event));

  int hittest_code =
      GetWindow()->non_client_view()->NonClientHitTest(mouse_event.location());
  switch (hittest_code) {
    case HTCAPTION: {
      // Start dragging if the mouse event is a single click and *not* a right
      // click. If it is a right click, then pass it through to
      // NativeWidgetGtk::OnButtonPress so that View class can show ContextMenu
      // upon a mouse release event. We only start drag on single clicks as we
      // get a crash in Gtk on double/triple clicks.
      if (event->type == GDK_BUTTON_PRESS &&
          !mouse_event.IsOnlyRightMouseButton()) {
        gfx::Point screen_point(event->x, event->y);
        View::ConvertPointToScreen(GetWindow()->GetRootView(), &screen_point);
        gtk_window_begin_move_drag(GetNativeWindow(), event->button,
                                   screen_point.x(), screen_point.y(),
                                   event->time);
        return TRUE;
      }
      break;
    }
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTGROWBOX:
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT: {
      gfx::Point screen_point(event->x, event->y);
      View::ConvertPointToScreen(GetWindow()->GetRootView(), &screen_point);
      // TODO(beng): figure out how to get a good minimum size.
      gtk_widget_set_size_request(GetNativeView(), 100, 100);
      gtk_window_begin_resize_drag(GetNativeWindow(),
                                   HitTestCodeToGDKWindowEdge(hittest_code),
                                   event->button, screen_point.x(),
                                   screen_point.y(), event->time);
      return TRUE;
    }
    default:
      // Everything else falls into standard client event handling...
      break;
  }
  return NativeWidgetGtk::OnButtonPress(widget, event);
}

gboolean NativeWindowGtk::OnConfigureEvent(GtkWidget* widget,
                                           GdkEventConfigure* event) {
  SaveWindowPosition();
  return FALSE;
}

gboolean NativeWindowGtk::OnMotionNotify(GtkWidget* widget,
                                         GdkEventMotion* event) {
  GdkEventMotion transformed_event = *event;
  TransformEvent(&transformed_event);
  gfx::Point translated_location(transformed_event.x, transformed_event.y);

  // Update the cursor for the screen edge.
  int hittest_code =
      GetWindow()->non_client_view()->NonClientHitTest(translated_location);
  if (hittest_code != HTCLIENT) {
    GdkCursorType cursor_type = HitTestCodeToGdkCursorType(hittest_code);
    gdk_window_set_cursor(widget->window, gfx::GetCursor(cursor_type));
  }

  return NativeWidgetGtk::OnMotionNotify(widget, event);
}

void NativeWindowGtk::OnSizeAllocate(GtkWidget* widget,
                                     GtkAllocation* allocation) {
  NativeWidgetGtk::OnSizeAllocate(widget, allocation);

  // The Window's NonClientView may provide a custom shape for the Window.
  gfx::Path window_mask;
  GetWindow()->non_client_view()->GetWindowMask(gfx::Size(allocation->width,
                                                          allocation->height),
                                                &window_mask);
  GdkRegion* mask_region = window_mask.CreateNativeRegion();
  gdk_window_shape_combine_region(GetNativeView()->window, mask_region, 0, 0);
  if (mask_region)
    gdk_region_destroy(mask_region);

  SaveWindowPosition();
}

gboolean NativeWindowGtk::OnWindowStateEvent(GtkWidget* widget,
                                             GdkEventWindowState* event) {
  window_state_ = event->new_window_state;
  if (!(window_state_ & GDK_WINDOW_STATE_WITHDRAWN))
    SaveWindowPosition();
  return FALSE;
}

gboolean NativeWindowGtk::OnLeaveNotify(GtkWidget* widget,
                                        GdkEventCrossing* event) {
  gdk_window_set_cursor(widget->window, gfx::GetCursor(GDK_LEFT_PTR));

  return NativeWidgetGtk::OnLeaveNotify(widget, event);
}

void NativeWindowGtk::IsActiveChanged() {
  NativeWidgetGtk::IsActiveChanged();
  delegate_->OnNativeWindowActivationChanged(IsActive());
}

void NativeWindowGtk::InitNativeWidget(const Widget::InitParams& params) {
  NativeWidgetGtk::InitNativeWidget(params);

  g_signal_connect(G_OBJECT(GetNativeWindow()), "configure-event",
                   G_CALLBACK(CallConfigureEvent), this);
  g_signal_connect(G_OBJECT(GetNativeWindow()), "window-state-event",
                   G_CALLBACK(CallWindowStateEvent), this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, NativeWindow implementation:

NativeWidget* NativeWindowGtk::AsNativeWidget() {
  return this;
}

const NativeWidget* NativeWindowGtk::AsNativeWidget() const {
  return this;
}

gfx::Rect NativeWindowGtk::GetRestoredBounds() const {
  // We currently don't support tiling, so this doesn't matter.
  return GetWindowScreenBounds();
}

void NativeWindowGtk::ShowNativeWindow(ShowState state) {
  // No concept of maximization (yet) on ChromeOS.
  if (state == NativeWindow::SHOW_INACTIVE)
    gtk_window_set_focus_on_map(GetNativeWindow(), false);
  gtk_widget_show(GetNativeView());
}

void NativeWindowGtk::BecomeModal() {
  gtk_window_set_modal(GetNativeWindow(), true);
}

void NativeWindowGtk::CenterWindow(const gfx::Size& size) {
  gfx::Rect center_rect;

  GtkWindow* parent = gtk_window_get_transient_for(GetNativeWindow());
  if (parent) {
    // We have a parent window, center over it.
    gint parent_x = 0;
    gint parent_y = 0;
    gtk_window_get_position(parent, &parent_x, &parent_y);
    gint parent_w = 0;
    gint parent_h = 0;
    gtk_window_get_size(parent, &parent_w, &parent_h);
    center_rect = gfx::Rect(parent_x, parent_y, parent_w, parent_h);
  } else {
    // We have no parent window, center over the screen.
    center_rect = Screen::GetMonitorWorkAreaNearestWindow(GetNativeView());
  }
  gfx::Rect bounds(center_rect.x() + (center_rect.width() - size.width()) / 2,
                   center_rect.y() + (center_rect.height() - size.height()) / 2,
                   size.width(), size.height());
  SetWindowBounds(bounds, NULL);
}

void NativeWindowGtk::GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                       bool* maximized) const {
  // Do nothing for now. ChromeOS isn't yet saving window placement.
}

void NativeWindowGtk::EnableClose(bool enable) {
  gtk_window_set_deletable(GetNativeWindow(), enable);
}

void NativeWindowGtk::SetWindowTitle(const std::wstring& title) {
  // We don't have a window title on ChromeOS (right now).
}

void NativeWindowGtk::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
  // We don't have window icons on ChromeOS.
}

void NativeWindowGtk::SetAccessibleName(const std::wstring& name) {
}

void NativeWindowGtk::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void NativeWindowGtk::SetAccessibleState(ui::AccessibilityTypes::State state) {
}

Window* NativeWindowGtk::GetWindow() {
  return delegate_->AsWindow();
}

const Window* NativeWindowGtk::GetWindow() const {
  return delegate_->AsWindow();
}

void NativeWindowGtk::SetWindowBounds(const gfx::Rect& bounds,
                                gfx::NativeWindow other_window) {
  // TODO: need to deal with other_window.
  NativeWidgetGtk::SetBounds(bounds);
}

void NativeWindowGtk::HideWindow() {
  GetWindow()->Hide();
}

void NativeWindowGtk::Activate() {
  gtk_window_present(GTK_WINDOW(GetNativeView()));
}

void NativeWindowGtk::Deactivate() {
  gdk_window_lower(GTK_WIDGET(GetNativeView())->window);
}

void NativeWindowGtk::Maximize() {
  gtk_window_maximize(GetNativeWindow());
}

void NativeWindowGtk::Minimize() {
  gtk_window_iconify(GetNativeWindow());
}

void NativeWindowGtk::Restore() {
  if (IsMaximized())
    gtk_window_unmaximize(GetNativeWindow());
  else if (IsMinimized())
    gtk_window_deiconify(GetNativeWindow());
  else if (IsFullscreen())
    SetFullscreen(false);
}

bool NativeWindowGtk::IsActive() const {
  return NativeWidgetGtk::IsActive();
}

bool NativeWindowGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(GetNativeView());
}

bool NativeWindowGtk::IsMaximized() const {
  return window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
}

bool NativeWindowGtk::IsMinimized() const {
  return window_state_ & GDK_WINDOW_STATE_ICONIFIED;
}

void NativeWindowGtk::SetFullscreen(bool fullscreen) {
  if (fullscreen)
    gtk_window_fullscreen(GetNativeWindow());
  else
    gtk_window_unfullscreen(GetNativeWindow());
}

bool NativeWindowGtk::IsFullscreen() const {
  return window_state_ & GDK_WINDOW_STATE_FULLSCREEN;
}

void NativeWindowGtk::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

NonClientFrameView* NativeWindowGtk::CreateFrameViewForWindow() {
  return new CustomFrameView(delegate_->AsWindow());
}

void NativeWindowGtk::SetAlwaysOnTop(bool always_on_top) {
  gtk_window_set_keep_above(GetNativeWindow(), always_on_top);
}

void NativeWindowGtk::UpdateFrameAfterFrameChange() {
  // We currently don't support different frame types on Gtk, so we don't
  // need to implement this.
  NOTIMPLEMENTED();
}

gfx::NativeWindow NativeWindowGtk::GetNativeWindow() const {
  return GTK_WINDOW(GetNativeView());
}

bool NativeWindowGtk::ShouldUseNativeFrame() const {
  return false;
}

void NativeWindowGtk::FrameTypeChanged() {
  // This is called when the Theme has changed, so forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, private:

// static
gboolean NativeWindowGtk::CallConfigureEvent(GtkWidget* widget,
                                             GdkEventConfigure* event,
                                             NativeWindowGtk* window_gtk) {
  return window_gtk->OnConfigureEvent(widget, event);
}

// static
gboolean NativeWindowGtk::CallWindowStateEvent(GtkWidget* widget,
                                               GdkEventWindowState* event,
                                               NativeWindowGtk* window_gtk) {
  return window_gtk->OnWindowStateEvent(widget, event);
}

void NativeWindowGtk::SaveWindowPosition() {
  // The delegate may have gone away on us.
  if (!GetWindow()->window_delegate())
    return;

  bool maximized = window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
  GetWindow()->window_delegate()->SaveWindowPlacement(
      GetWidget()->GetWindowScreenBounds(),
      maximized);
}

void NativeWindowGtk::OnDestroy(GtkWidget* widget) {
  delegate_->OnNativeWindowDestroying();
  NativeWidgetGtk::OnDestroy(widget);
  delegate_->OnNativeWindowDestroyed();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindow, public:

// static
NativeWindow* NativeWindow::CreateNativeWindow(
    internal::NativeWindowDelegate* delegate) {
  return new NativeWindowGtk(delegate);
}

}  // namespace views

