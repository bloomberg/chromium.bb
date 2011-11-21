// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/native_control_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

#if defined(TOUCH_UI)
namespace {

GdkEvent* MouseButtonEvent(const views::MouseEvent& mouseev,
                           const views::NativeViewHost* source) {
  GdkEvent* event = NULL;
  switch (mouseev.type()) {
    case ui::ET_MOUSE_PRESSED:
      event = gdk_event_new(GDK_BUTTON_PRESS);
      break;
    case ui::ET_MOUSE_RELEASED:
      event = gdk_event_new(GDK_BUTTON_RELEASE);
      break;
    default:
      NOTREACHED();
      return NULL;
  }
  event->button.send_event = false;
  event->button.time = GDK_CURRENT_TIME;

  // Ideally using native_view()->window should work, but it doesn't.
  event->button.window = gdk_window_at_pointer(NULL, NULL);
  g_object_ref(event->button.window);
  event->button.x = mouseev.location().x();
  event->button.y = mouseev.location().y();

  gfx::Point point = mouseev.location();
  views::View::ConvertPointToScreen(source, &point);
  event->button.x_root = point.x();
  event->button.y_root = point.y();

  event->button.axes = NULL;

  // Ideally, this should reconstruct the state from mouseev.flags().
  GdkModifierType modifier;
  gdk_window_get_pointer(event->button.window, NULL, NULL, &modifier);
  event->button.state = modifier;

  event->button.button = (mouseev.flags() & ui::EF_LEFT_BUTTON_DOWN) ? 1 :
                         (mouseev.flags() & ui::EF_RIGHT_BUTTON_DOWN) ? 3 :
                         2;
  event->button.device = gdk_device_get_core_pointer();
  return event;
}

GdkEvent* MouseMoveEvent(const views::MouseEvent& mouseev,
                         const views::NativeViewHost* source) {
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
  event->motion.send_event = false;
  event->motion.time = GDK_CURRENT_TIME;

  event->motion.window = source->native_view()->window;
  g_object_ref(event->motion.window);
  event->motion.x = mouseev.location().x();
  event->motion.y = mouseev.location().y();

  gfx::Point point = mouseev.location();
  views::View::ConvertPointToScreen(source, &point);
  event->motion.x_root = point.x();
  event->motion.y_root = point.y();

  event->motion.device = gdk_device_get_core_pointer();
  return event;
}

GdkEvent* MouseCrossEvent(const views::MouseEvent& mouseev,
                          const views::NativeViewHost* source) {
  GdkEvent* event = NULL;
  switch (mouseev.type()) {
    case ui::ET_MOUSE_ENTERED:
      event = gdk_event_new(GDK_ENTER_NOTIFY);
      break;
    case ui::ET_MOUSE_EXITED:
      event = gdk_event_new(GDK_LEAVE_NOTIFY);
      break;
    default:
      NOTREACHED();
      return NULL;
  }
  event->crossing.send_event = false;
  event->crossing.time = GDK_CURRENT_TIME;

  event->crossing.window = source->native_view()->window;
  g_object_ref(event->crossing.window);

  event->crossing.x = event->crossing.y = 0;
  event->crossing.x_root = event->crossing.y_root = 0;
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
  event->crossing.focus = false;
  event->crossing.state = 0;

  return event;
}

}  // namespace
#endif  // defined(TOUCH_UI)

namespace views {

#if defined(TOUCH_UI)
void NativeControlGtk::FakeNativeMouseEvent(const MouseEvent& mouseev) {
  GdkEvent* event = NULL;
  switch (mouseev.type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
      event = MouseButtonEvent(mouseev, this);
      break;
    case ui::ET_MOUSE_MOVED:
      event = MouseMoveEvent(mouseev, this);
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      event = MouseCrossEvent(mouseev, this);
      break;
    default:
      NOTREACHED();
      return;
  }

  if (event) {
    // Do not gdk_event_put, since that will end up going through the
    // message-loop and turn into an infinite loop.
    gtk_widget_event(native_view(), event);
    gdk_event_free(event);
  }
}
#endif  // defined(TOUCH_UI)

NativeControlGtk::NativeControlGtk() {
}

NativeControlGtk::~NativeControlGtk() {
  if (native_view())
    gtk_widget_destroy(native_view());
}

////////////////////////////////////////////////////////////////////////////////
// NativeControlGtk, View overrides:

void NativeControlGtk::OnEnabledChanged() {
  View::OnEnabledChanged();
  if (native_view())
    gtk_widget_set_sensitive(native_view(), IsEnabled());
}

void NativeControlGtk::ViewHierarchyChanged(bool is_add, View* parent,
                                            View* child) {
  // Call the base class to hide the view if we're being removed.
  NativeViewHost::ViewHierarchyChanged(is_add, parent, child);

  if (!is_add && child == this && native_view()) {
    Detach();
  } else if (is_add && GetWidget() && !native_view()) {
    // Create the widget when we're added to a valid Widget. Many
    // controls need a parent widget to function properly.
    CreateNativeControl();
  }
}

void NativeControlGtk::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!native_view()) {
    if (GetWidget())
      CreateNativeControl();
  } else {
    // The view becomes visible after native control is created.
    // Layout now.
    Layout();
  }
}

void NativeControlGtk::OnFocus() {
  DCHECK(native_view());
  gtk_widget_grab_focus(native_view());
  GetWidget()->NotifyAccessibilityEvent(
      parent(), ui::AccessibilityTypes::EVENT_FOCUS, true);
}

#if defined(TOUCH_UI)
bool NativeControlGtk::OnMousePressed(const MouseEvent& mouseev) {
  FakeNativeMouseEvent(mouseev);
  return true;
}

void NativeControlGtk::OnMouseReleased(const MouseEvent& mouseev) {
  FakeNativeMouseEvent(mouseev);
}

void NativeControlGtk::OnMouseMoved(const MouseEvent& mouseev) {
  FakeNativeMouseEvent(mouseev);
}

void NativeControlGtk::OnMouseEntered(const MouseEvent& mouseev) {
  FakeNativeMouseEvent(mouseev);
}

void NativeControlGtk::OnMouseExited(const MouseEvent& mouseev) {
  FakeNativeMouseEvent(mouseev);
}
#endif  // defined(TOUCH_UI)

void NativeControlGtk::NativeControlCreated(GtkWidget* native_control) {
  Attach(native_control);

  // Update the newly created GtkWidget with any resident enabled state.
  gtk_widget_set_sensitive(native_view(), IsEnabled());

  // Listen for focus change event to update the FocusManager focused view.
  g_signal_connect(native_control, "focus-in-event",
                   G_CALLBACK(CallFocusIn), this);
}

// static
gboolean NativeControlGtk::CallFocusIn(GtkWidget* gtk_widget,
                                       GdkEventFocus* event,
                                       NativeControlGtk* control) {
  Widget* widget = Widget::GetTopLevelWidgetForNativeView(gtk_widget);
  FocusManager* focus_manager = widget ? widget->GetFocusManager() : NULL;
  if (!focus_manager) {
    // TODO(jcampan): http://crbug.com/21378 Reenable this NOTREACHED() when the
    // options page is only based on views.
    // NOTREACHED();
    NOTIMPLEMENTED();
    return false;
  }
  focus_manager->SetFocusedView(control->focus_view());
  return false;
}

}  // namespace views
