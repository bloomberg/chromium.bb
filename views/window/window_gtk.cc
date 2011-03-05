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
  window->GetNonClientView()->SetFrameView(window->CreateFrameViewForWindow());
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

gfx::Rect WindowGtk::GetBounds() const {
  return GetWindowScreenBounds();
}

gfx::Rect WindowGtk::GetNormalBounds() const {
  // We currently don't support tiling, so this doesn't matter.
  return GetBounds();
}

void WindowGtk::SetWindowBounds(const gfx::Rect& bounds,
                                gfx::NativeWindow other_window) {
  // TODO: need to deal with other_window.
  WidgetGtk::SetBounds(bounds);
}

void WindowGtk::Show() {
  gtk_widget_show(GetNativeView());
}

void WindowGtk::HideWindow() {
  Hide();
}

void WindowGtk::SetNativeWindowProperty(const char* name, void* value) {
  WidgetGtk::SetNativeWindowProperty(name, value);
}

void* WindowGtk::GetNativeWindowProperty(const char* name) {
  return WidgetGtk::GetNativeWindowProperty(name);
}

void WindowGtk::Activate() {
  gtk_window_present(GTK_WINDOW(GetNativeView()));
}

void WindowGtk::Deactivate() {
  gdk_window_lower(GTK_WIDGET(GetNativeView())->window);
}

void WindowGtk::Close() {
  if (window_closed_) {
    // Don't do anything if we've already been closed.
    return;
  }

  if (non_client_view_->CanClose()) {
    WidgetGtk::Close();
    window_closed_ = true;
  }
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

void WindowGtk::EnableClose(bool enable) {
  gtk_window_set_deletable(GetNativeWindow(), enable);
}

void WindowGtk::UpdateWindowTitle() {
  // ChromeOS doesn't use a window title, so don't update them.
#if !defined(OS_CHROMEOS)

  // If the non-client view is rendering its own title, it'll need to relayout
  // now.
  non_client_view_->Layout();

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  std::wstring window_title = window_delegate_->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&window_title);

  gtk_window_set_title(GetNativeWindow(), WideToUTF8(window_title).c_str());
#endif
}

void WindowGtk::UpdateWindowIcon() {
  // Doesn't matter for chrome os.
}

void WindowGtk::SetIsAlwaysOnTop(bool always_on_top) {
  gtk_window_set_keep_above(GetNativeWindow(), always_on_top);
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

WindowDelegate* WindowGtk::GetDelegate() const {
  return window_delegate_;
}

NonClientView* WindowGtk::GetNonClientView() const {
  return non_client_view_;
}

ClientView* WindowGtk::GetClientView() const {
  return non_client_view_->client_view();
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
      non_client_view_->NonClientHitTest(gfx::Point(x, y));
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
      non_client_view_->NonClientHitTest(gfx::Point(x, y));
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
  non_client_view_->GetWindowMask(gfx::Size(allocation->width,
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

void WindowGtk::SetInitialFocus() {
  View* v = window_delegate_->GetInitiallyFocusedView();
  if (v) {
    v->RequestFocus();
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowGtk, protected:

WindowGtk::WindowGtk(WindowDelegate* window_delegate)
    : WidgetGtk(TYPE_WINDOW),
      ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)),
      is_modal_(false),
      window_delegate_(window_delegate),
      non_client_view_(new NonClientView(this)),
      window_state_(GDK_WINDOW_STATE_WITHDRAWN),
      window_closed_(false) {
  set_native_window(this);
  is_window_ = true;
  DCHECK(!window_delegate_->window_);
  window_delegate_->window_ = this;
}

void WindowGtk::InitWindow(GtkWindow* parent, const gfx::Rect& bounds) {
  if (parent)
    make_transient_to_parent();
  WidgetGtk::Init(GTK_WIDGET(parent), bounds);

  // We call this after initializing our members since our implementations of
  // assorted WidgetWin functions may be called during initialization.
  is_modal_ = window_delegate_->IsModal();
  if (is_modal_)
    gtk_window_set_modal(GetNativeWindow(), true);

  g_signal_connect(G_OBJECT(GetNativeWindow()), "configure-event",
                   G_CALLBACK(CallConfigureEvent), this);
  g_signal_connect(G_OBJECT(GetNativeWindow()), "window-state-event",
                   G_CALLBACK(CallWindowStateEvent), this);

  // Create the ClientView, add it to the NonClientView and add the
  // NonClientView to the RootView. This will cause everything to be parented.
  non_client_view_->set_client_view(window_delegate_->CreateClientView(this));
  WidgetGtk::SetContentsView(non_client_view_);

  UpdateWindowTitle();
  SetInitialBounds(parent, bounds);

  // if (!IsAppWindow()) {
  //   notification_registrar_.Add(
  //       this,
  //       NotificationType::ALL_APPWINDOWS_CLOSED,
  //       NotificationService::AllSources());
  // }

  // ResetWindowRegion(false);
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
  if (!window_delegate_)
    return;

  bool maximized = window_state_ & GDK_WINDOW_STATE_MAXIMIZED;
  window_delegate_->SaveWindowPlacement(GetBounds(), maximized);
}

void WindowGtk::SetInitialBounds(GtkWindow* parent,
                                 const gfx::Rect& create_bounds) {
  gfx::Rect saved_bounds(create_bounds.ToGdkRectangle());
  if (window_delegate_->GetSavedWindowBounds(&saved_bounds)) {
    if (!window_delegate_->ShouldRestoreWindowSize())
      saved_bounds.set_size(non_client_view_->GetPreferredSize());
    WidgetGtk::SetBounds(saved_bounds);
  } else {
    if (create_bounds.IsEmpty()) {
      SizeWindowToDefault(parent);
    } else {
      SetWindowBounds(create_bounds, NULL);
    }
  }
}

void WindowGtk::SizeWindowToDefault(GtkWindow* parent) {
  gfx::Rect center_rect;

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
  gfx::Size size = non_client_view_->GetPreferredSize();
  gfx::Rect bounds(center_rect.x() + (center_rect.width() - size.width()) / 2,
                   center_rect.y() + (center_rect.height() - size.height()) / 2,
                   size.width(), size.height());
  SetWindowBounds(bounds, NULL);
}

void WindowGtk::OnDestroy(GtkWidget* widget) {
  non_client_view_->WindowClosing();
  WidgetGtk::OnDestroy(widget);
  window_delegate_->DeleteDelegate();
  window_delegate_ = NULL;
}

}  // namespace views
