// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window_gtk.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "views/events/event.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
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

WindowGtk::~WindowGtk() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  WindowGtk* window = new WindowGtk(window_delegate);
  window->non_client_view()->SetFrameView(window->CreateFrameViewForWindow());
  window->InitWindow(parent, bounds);
  return window;
}

// static
void Window::CloseAllSecondaryWindows() {
  GList* windows = gtk_window_list_toplevels();
  for (GList* window = windows; window;
       window = g_list_next(window)) {
    Window::CloseSecondaryWidget(
        NativeWidget::GetNativeWidgetForNativeView(
            GTK_WIDGET(window->data))->GetWidget());
  }
  g_list_free(windows);
}

Window* WindowGtk::AsWindow() {
  return this;
}

const Window* WindowGtk::AsWindow() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, WidgetGtk overrides:

gboolean WindowGtk::OnButtonPress(GtkWidget* widget, GdkEventButton* event) {
  int x = 0, y = 0;
  GetContainedWidgetEventCoordinates(event, &x, &y);

  int hittest_code =
      GetWindow()->non_client_view()->NonClientHitTest(gfx::Point(x, y));
  switch (hittest_code) {
    case HTCAPTION: {
      MouseEvent mouse_pressed(ui::ET_MOUSE_PRESSED, event->x, event->y,
                               WidgetGtk::GetFlagsForEventButton(*event));
      // Start dragging if the mouse event is a single click and *not* a right
      // click. If it is a right click, then pass it through to
      // WidgetGtk::OnButtonPress so that View class can show ContextMenu upon a
      // mouse release event. We only start drag on single clicks as we get a
      // crash in Gtk on double/triple clicks.
      if (event->type == GDK_BUTTON_PRESS &&
          !mouse_pressed.IsOnlyRightMouseButton()) {
        gfx::Point screen_point(event->x, event->y);
        View::ConvertPointToScreen(GetRootView(), &screen_point);
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
      View::ConvertPointToScreen(GetRootView(), &screen_point);
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
  return WidgetGtk::OnButtonPress(widget, event);
}

gboolean WindowGtk::OnConfigureEvent(GtkWidget* widget,
                                     GdkEventConfigure* event) {
  SaveWindowPosition();
  return FALSE;
}

gboolean WindowGtk::OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
  int x = 0, y = 0;
  GetContainedWidgetEventCoordinates(event, &x, &y);

  // Update the cursor for the screen edge.
  int hittest_code =
      GetWindow()->non_client_view()->NonClientHitTest(gfx::Point(x, y));
  if (hittest_code != HTCLIENT) {
    GdkCursorType cursor_type = HitTestCodeToGdkCursorType(hittest_code);
    gdk_window_set_cursor(widget->window, gfx::GetCursor(cursor_type));
  }

  return WidgetGtk::OnMotionNotify(widget, event);
}

void WindowGtk::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  WidgetGtk::OnSizeAllocate(widget, allocation);

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

gboolean WindowGtk::OnWindowStateEvent(GtkWidget* widget,
                                       GdkEventWindowState* event) {
  window_state_ = event->new_window_state;
  if (!(window_state_ & GDK_WINDOW_STATE_WITHDRAWN))
    SaveWindowPosition();
  return FALSE;
}

gboolean WindowGtk::OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
  gdk_window_set_cursor(widget->window, gfx::GetCursor(GDK_LEFT_PTR));

  return WidgetGtk::OnLeaveNotify(widget, event);
}

void WindowGtk::IsActiveChanged() {
  WidgetGtk::IsActiveChanged();
  delegate_->OnNativeWindowActivationChanged(IsActive());
}

void WindowGtk::SetInitialFocus() {
  View* v = GetWindow()->window_delegate()->GetInitiallyFocusedView();
  if (v) {
    v->RequestFocus();
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, NativeWindow implementation:

gfx::Rect WindowGtk::GetRestoredBounds() const {
  // We currently don't support tiling, so this doesn't matter.
  return GetWindowScreenBounds();
}

void WindowGtk::ShowNativeWindow(ShowState state) {
  // No concept of maximization (yet) on ChromeOS.
  gtk_widget_show(GetNativeView());
}

void WindowGtk::BecomeModal() {
  gtk_window_set_modal(GetNativeWindow(), true);
}

void WindowGtk::CenterWindow(const gfx::Size& size) {
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

void WindowGtk::GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                 bool* maximized) const {
  // Do nothing for now. ChromeOS isn't yet saving window placement.
}

void WindowGtk::EnableClose(bool enable) {
  gtk_window_set_deletable(GetNativeWindow(), enable);
}

void WindowGtk::SetWindowTitle(const std::wstring& title) {
  // We don't have a window title on ChromeOS (right now).
}

void WindowGtk::SetWindowIcons(const SkBitmap& window_icon,
                               const SkBitmap& app_icon) {
  // We don't have window icons on ChromeOS.
}

void WindowGtk::SetAccessibleName(const std::wstring& name) {
}

void WindowGtk::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
}

void WindowGtk::SetAccessibleState(ui::AccessibilityTypes::State state) {
}

NativeWidget* WindowGtk::AsNativeWidget() {
  return this;
}

const NativeWidget* WindowGtk::AsNativeWidget() const {
  return this;
}

Window* WindowGtk::GetWindow() {
  return this;
}

void WindowGtk::SetWindowBounds(const gfx::Rect& bounds,
                                gfx::NativeWindow other_window) {
  // TODO: need to deal with other_window.
  WidgetGtk::SetBounds(bounds);
}

void WindowGtk::HideWindow() {
  Hide();
}

void WindowGtk::Activate() {
  gtk_window_present(GTK_WINDOW(GetNativeView()));
}

void WindowGtk::Deactivate() {
  gdk_window_lower(GTK_WIDGET(GetNativeView())->window);
}

void WindowGtk::Maximize() {
  gtk_window_maximize(GetNativeWindow());
}

void WindowGtk::Minimize() {
  gtk_window_iconify(GetNativeWindow());
}

void WindowGtk::Restore() {
  if (IsMaximized())
    gtk_window_unmaximize(GetNativeWindow());
  else if (IsMinimized())
    gtk_window_deiconify(GetNativeWindow());
  else if (IsFullscreen())
    SetFullscreen(false);
}

bool WindowGtk::IsActive() const {
  return WidgetGtk::IsActive();
}

bool WindowGtk::IsVisible() const {
  return GTK_WIDGET_VISIBLE(GetNativeView());
}

bool WindowGtk::IsMaximized() const {
  return window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
}

bool WindowGtk::IsMinimized() const {
  return window_state_ & GDK_WINDOW_STATE_ICONIFIED;
}

void WindowGtk::SetFullscreen(bool fullscreen) {
  if (fullscreen)
    gtk_window_fullscreen(GetNativeWindow());
  else
    gtk_window_unfullscreen(GetNativeWindow());
}

bool WindowGtk::IsFullscreen() const {
  return window_state_ & GDK_WINDOW_STATE_FULLSCREEN;
}

void WindowGtk::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void WindowGtk::SetAlwaysOnTop(bool always_on_top) {
  gtk_window_set_keep_above(GetNativeWindow(), always_on_top);
}

bool WindowGtk::IsAppWindow() const {
  return false;
}

NonClientFrameView* WindowGtk::CreateFrameViewForWindow() {
  // TODO(erg): Always use a custom frame view? Are there cases where we let
  // the window manager deal with the X11 equivalent of the "non-client" area?
  return new CustomFrameView(this);
}

void WindowGtk::UpdateFrameAfterFrameChange() {
  // We currently don't support different frame types on Gtk, so we don't
  // need to implement this.
  NOTIMPLEMENTED();
}

gfx::NativeWindow WindowGtk::GetNativeWindow() const {
  return GTK_WINDOW(GetNativeView());
}

bool WindowGtk::ShouldUseNativeFrame() const {
  return false;
}

void WindowGtk::FrameTypeChanged() {
  // This is called when the Theme has changed, so forward the event to the root
  // widget.
  ThemeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, protected:

WindowGtk::WindowGtk(WindowDelegate* window_delegate)
    : WidgetGtk(TYPE_WINDOW),
      Window(window_delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)),
      window_state_(GDK_WINDOW_STATE_WITHDRAWN),
      window_closed_(false) {
  set_native_window(this);
  is_window_ = true;
}

void WindowGtk::InitWindow(GtkWindow* parent, const gfx::Rect& bounds) {
  if (parent)
    make_transient_to_parent();
  WidgetGtk::Init(GTK_WIDGET(parent), bounds);
  delegate_->OnNativeWindowCreated(bounds);

  g_signal_connect(G_OBJECT(GetNativeWindow()), "configure-event",
                   G_CALLBACK(CallConfigureEvent), this);
  g_signal_connect(G_OBJECT(GetNativeWindow()), "window-state-event",
                   G_CALLBACK(CallWindowStateEvent), this);
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, private:

// static
gboolean WindowGtk::CallConfigureEvent(GtkWidget* widget,
                                       GdkEventConfigure* event,
                                       WindowGtk* window_gtk) {
  return window_gtk->OnConfigureEvent(widget, event);
}

// static
gboolean WindowGtk::CallWindowStateEvent(GtkWidget* widget,
                                         GdkEventWindowState* event,
                                         WindowGtk* window_gtk) {
  return window_gtk->OnWindowStateEvent(widget, event);
}

void WindowGtk::SaveWindowPosition() {
  // The delegate may have gone away on us.
  if (!GetWindow()->window_delegate())
    return;

  bool maximized = window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
  GetWindow()->window_delegate()->SaveWindowPlacement(GetBounds(), maximized);
}

void WindowGtk::OnDestroy(GtkWidget* widget) {
  delegate_->OnNativeWindowDestroying();
  WidgetGtk::OnDestroy(widget);
  delegate_->OnNativeWindowDestroyed();
}

}  // namespace views
