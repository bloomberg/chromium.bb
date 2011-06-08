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

gboolean NativeWindowGtk::OnLeaveNotify(GtkWidget* widget,
                                        GdkEventCrossing* event) {
  gdk_window_set_cursor(widget->window, gfx::GetCursor(GDK_LEFT_PTR));

  return NativeWidgetGtk::OnLeaveNotify(widget, event);
}

void NativeWindowGtk::InitNativeWidget(const Widget::InitParams& params) {
  NativeWidgetGtk::InitNativeWidget(params);

  g_signal_connect(G_OBJECT(GetNativeWindow()), "configure-event",
                   G_CALLBACK(CallConfigureEvent), this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, NativeWindow implementation:

NativeWidget* NativeWindowGtk::AsNativeWidget() {
  return this;
}

const NativeWidget* NativeWindowGtk::AsNativeWidget() const {
  return this;
}

void NativeWindowGtk::BecomeModal() {
  gtk_window_set_modal(GetNativeWindow(), true);
}

Window* NativeWindowGtk::GetWindow() {
  return delegate_->AsWindow();
}

const Window* NativeWindowGtk::GetWindow() const {
  return delegate_->AsWindow();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, NativeWidgetGtk overrides:

gboolean NativeWindowGtk::OnWindowStateEvent(GtkWidget* widget,
                                             GdkEventWindowState* event) {
  if (!(event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN))
    SaveWindowPosition();
  return NativeWidgetGtk::OnWindowStateEvent(widget, event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, private:

// static
gboolean NativeWindowGtk::CallConfigureEvent(GtkWidget* widget,
                                             GdkEventConfigure* event,
                                             NativeWindowGtk* window_gtk) {
  return window_gtk->OnConfigureEvent(widget, event);
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

////////////////////////////////////////////////////////////////////////////////
// NativeWindow, public:

// static
NativeWindow* NativeWindow::CreateNativeWindow(
    internal::NativeWindowDelegate* delegate) {
  return new NativeWindowGtk(delegate);
}

}  // namespace views
