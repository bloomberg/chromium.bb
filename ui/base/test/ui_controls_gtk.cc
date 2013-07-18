// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/base/gtk/event_synthesis_gtk.h"
#include "ui/base/gtk/gtk_screen_util.h"
#include "ui/gfx/rect.h"

namespace {
bool g_ui_controls_enabled = false;

// static
guint32 XTimeNow() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

class EventWaiter : public base::MessageLoopForUI::Observer {
 public:
  EventWaiter(const base::Closure& task, GdkEventType type, int count)
      : task_(task),
        type_(type),
        count_(count) {
    base::MessageLoopForUI::current()->AddObserver(this);
  }

  virtual ~EventWaiter() {
    base::MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE {
    if ((event->type == type_) && (--count_ == 0)) {
      // At the time we're invoked the event has not actually been processed.
      // Use PostTask to make sure the event has been processed before
      // notifying.
      // NOTE: if processing a message results in running a nested message
      // loop, then DidProcessEvent isn't immediately sent. As such, we do
      // the processing in WillProcessEvent rather than DidProcessEvent.
      base::MessageLoop::current()->PostTask(FROM_HERE, task_);
      delete this;
    }
  }

  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE {
    // No-op.
  }

 private:
  base::Closure task_;
  GdkEventType type_;
  // The number of events of this type to wait for.
  int count_;
};

void FakeAMouseMotionEvent(gint x, gint y) {
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);

  event->motion.send_event = false;
  event->motion.time = XTimeNow();

  GtkWidget* grab_widget = gtk_grab_get_current();
  if (grab_widget) {
    // If there is a grab, we need to target all events at it regardless of
    // what widget the mouse is over.
    event->motion.window = gtk_widget_get_window(grab_widget);
  } else {
    event->motion.window = gdk_window_at_pointer(&x, &y);
  }
  g_object_ref(event->motion.window);
  gint window_x, window_y;
  gdk_window_get_origin(event->motion.window, &window_x, &window_y);
  event->motion.x = x - window_x;
  event->motion.y = y - window_y;
  event->motion.x_root = x;
  event->motion.y_root = y;

  event->motion.device = gdk_device_get_core_pointer();
  event->type = GDK_MOTION_NOTIFY;

  gdk_event_put(event);
  gdk_event_free(event);
}

}  // namespace

namespace ui_controls {

void EnableUIControls() {
  g_ui_controls_enabled = true;
}

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  CHECK(g_ui_controls_enabled);
  DCHECK(!command);  // No command key on Linux
  GdkWindow* event_window = NULL;
  GtkWidget* grab_widget = gtk_grab_get_current();
  if (grab_widget) {
    // If there is a grab, send all events to the grabbed widget.
    event_window = gtk_widget_get_window(grab_widget);
  } else if (window) {
    event_window = gtk_widget_get_window(GTK_WIDGET(window));
  } else {
    // No target was specified. Send the events to the active toplevel.
    GList* windows = gtk_window_list_toplevels();
    for (GList* element = windows; element; element = g_list_next(element)) {
      GtkWindow* this_window = GTK_WINDOW(element->data);
      if (gtk_window_is_active(this_window)) {
        event_window = gtk_widget_get_window(GTK_WIDGET(this_window));
        break;
      }
    }
    g_list_free(windows);
  }
  if (!event_window) {
    NOTREACHED() << "Window not specified and none is active";
    return false;
  }

  std::vector<GdkEvent*> events;
  ui::SynthesizeKeyPressEvents(event_window, key, control, shift, alt, &events);
  for (std::vector<GdkEvent*>::iterator iter = events.begin();
       iter != events.end(); ++iter) {
    gdk_event_put(*iter);
    // gdk_event_put appends a copy of the event.
    gdk_event_free(*iter);
  }

  return true;
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                const base::Closure& task) {
  CHECK(g_ui_controls_enabled);
  DCHECK(!command);  // No command key on Linux
  int release_count = 1;
  if (control)
    release_count++;
  if (shift)
    release_count++;
  if (alt)
    release_count++;
  // This object will delete itself after running |task|.
  new EventWaiter(task, GDK_KEY_RELEASE, release_count);
  return SendKeyPress(window, key, control, shift, alt, command);
}

bool SendMouseMove(long x, long y) {
  CHECK(g_ui_controls_enabled);
  gdk_display_warp_pointer(gdk_display_get_default(), gdk_screen_get_default(),
                           x, y);
  // Sometimes gdk_display_warp_pointer fails to send back any indication of
  // the move, even though it succesfully moves the server cursor. We fake it in
  // order to get drags to work.
  FakeAMouseMotionEvent(x, y);
  return true;
}

bool SendMouseMoveNotifyWhenDone(long x, long y, const base::Closure& task) {
  CHECK(g_ui_controls_enabled);
  bool rv = SendMouseMove(x, y);
  new EventWaiter(task, GDK_MOTION_NOTIFY, 1);
  return rv;
}

bool SendMouseEvents(MouseButton type, int state) {
  CHECK(g_ui_controls_enabled);
  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);

  event->button.send_event = false;
  event->button.time = XTimeNow();

  gint x, y;
  GtkWidget* grab_widget = gtk_grab_get_current();
  if (grab_widget) {
    // If there is a grab, we need to target all events at it regardless of
    // what widget the mouse is over.
    event->button.window = gtk_widget_get_window(grab_widget);
    gdk_window_get_pointer(event->button.window, &x, &y, NULL);
  } else {
    event->button.window = gdk_window_at_pointer(&x, &y);
    CHECK(event->button.window);
  }

  g_object_ref(event->button.window);
  event->button.x = x;
  event->button.y = y;
  gint origin_x, origin_y;
  gdk_window_get_origin(event->button.window, &origin_x, &origin_y);
  event->button.x_root = x + origin_x;
  event->button.y_root = y + origin_y;

  event->button.axes = NULL;
  GdkModifierType modifier;
  gdk_window_get_pointer(event->button.window, NULL, NULL, &modifier);
  event->button.state = modifier;
  event->button.button = type == LEFT ? 1 : (type == MIDDLE ? 2 : 3);
  event->button.device = gdk_device_get_core_pointer();

  event->button.type = GDK_BUTTON_PRESS;
  if (state & DOWN)
    gdk_event_put(event);

  // Also send a release event.
  GdkEvent* release_event = gdk_event_copy(event);
  release_event->button.type = GDK_BUTTON_RELEASE;
  release_event->button.time++;
  if (state & UP)
    gdk_event_put(release_event);

  gdk_event_free(event);
  gdk_event_free(release_event);

  return false;
}

bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int state,
                                   const base::Closure& task) {
  CHECK(g_ui_controls_enabled);
  bool rv = SendMouseEvents(type, state);
  GdkEventType wait_type;
  if (state & UP) {
    wait_type = GDK_BUTTON_RELEASE;
  } else {
    if (type == LEFT)
      wait_type = GDK_BUTTON_PRESS;
    else if (type == MIDDLE)
      wait_type = GDK_2BUTTON_PRESS;
    else
      wait_type = GDK_3BUTTON_PRESS;
  }
  new EventWaiter(task, wait_type, 1);
  return rv;
}

bool SendMouseClick(MouseButton type) {
  CHECK(g_ui_controls_enabled);
  return SendMouseEvents(type, UP | DOWN);
}

}  // namespace ui_controls
