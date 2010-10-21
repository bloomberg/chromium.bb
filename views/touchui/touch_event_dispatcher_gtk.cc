// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a function that receives a message from the message pump
// and dispatches that message to the appropriate root view.  That function is
// 'DispatchEventForTouchUIGtk'.  (Last function in this file.)
//
// The appropriate RootView is determined for each incoming event.  The platform
// specific event is converted to a views event and dispatched directly to the
// appropriate RootView.
//
// This implementation is Gdk specific at the moment, but a future CL will
// provide a dispatcher that handles events from an X Windows message pump.

// TODO(wyck): Make X Windows versions of all GdkEvent functions.
// (See individual TODO's below)
//
// When we switch the message pump from one that gives us GdkEvents to one that
// gives us X Windows events, we will need another version of each function.
// These ones are obviously specific to GdkEvent.
//
// Potential names:
//   Maybe DispatchEventForTouchUIGtk will become DispatchEventForTouchUIX11.
//
// It may not be necessary to filter events with IsTouchEvent in the X version,
// because the message pump may pre-filter the message so that we get only
// touch events and there is nothing to filter out.

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace views {

// gets the RootView associated with the GdkEvent.
//
// TODO(wyck): Make X Windows version of this function. (see earlier comment)
static RootView* FindRootViewForGdkEvent(GdkEvent* event) {
  GdkEventAny* event_any = reinterpret_cast<GdkEventAny*>(event);
  if (!event_any) {
    DLOG(WARNING) << "FindRootViewForGdkEvent was passed a null GdkEvent";
    return NULL;
  }
  GdkWindow* gdk_window = event_any->window;

  // and get the parent
  gpointer data = NULL;
  gdk_window_get_user_data(gdk_window, &data);
  GtkWidget* gtk_widget = reinterpret_cast<GtkWidget*>(data);
  if (!gtk_widget) {
    DLOG(WARNING) << "no GtkWidget found for that GdkWindow";
    return NULL;
  }
  WidgetGtk* widget_gtk = WidgetGtk::GetViewForNative(gtk_widget);

  if (!widget_gtk) {
    DLOG(WARNING) << "no WidgetGtk found for that GtkWidget";
    return NULL;
  }
  return widget_gtk->GetRootView();
}

// Specialized dispatch for GDK_BUTTON_PRESS events
static void DispatchButtonPressGtk(const GdkEventButton& event,
                                   RootView* root_view) {
  // TODO(wyck): may need to remap coordinates:
  // If so, it's like this:
  // gdk_window_get_root_origin(dest, &dest_x, &dest_y);
  // *x = event->x_root - dest_x;
  // *y = event->y_root - dest_y;

  if (event.type == GDK_2BUTTON_PRESS || event.type == GDK_3BUTTON_PRESS) {
    // TODO(wyck): decide what to do about 2 button and 3 button press msgs.
    // You get both a GDK_BUTTON_PRESS and a GDK_2BUTTON_PRESS, so that's
    // a bit weird.
    // I'll ignore these events for now.
    return;
  }

  MouseEvent mouse_pressed(Event::ET_MOUSE_PRESSED, event.x, event.y,
      WidgetGtk::GetFlagsForEventButton(event));
  root_view->OnMousePressed(mouse_pressed);
}

// Specialized dispatch for GDK_BUTTON_RELEASE events
static void DispatchButtonReleaseGtk(const GdkEventButton& event,
                                     RootView* root_view) {
  // TODO(wyck): may need to remap coordinates.
  // If so, it's like this:
  // gdk_window_get_root_origin(dest, &dest_x, &dest_y);
  // *x = event->x_root - dest_x;
  // *y = event->y_root - dest_y;

  MouseEvent mouse_up(Event::ET_MOUSE_RELEASED, event.x, event.y,
      WidgetGtk::GetFlagsForEventButton(event));

  root_view->OnMouseReleased(mouse_up, false);
}

// Specialized dispatch for GDK_MOTION_NOTIFY events
static void DispatchMotionNotifyGtk(const GdkEventMotion& event,
                                    RootView* root_view) {
  // Regarding GDK_POINTER_MOTION_HINT_MASK:
  // GDK_POINTER_MOTION_HINT_MASK may have been used to reduce the number of
  // GDK_MOTION_NOTIFY events received. Normally a GDK_MOTION_NOTIFY event is
  // received each time the mouse moves.  But in the hint case, some events are
  // marked with is_hint TRUE.  Without further action after a hint, no more
  // motion events will be received.
  // To receive more motion events after a motion hint event, the application
  // needs to ask for more by calling gdk_event_request_motions().
  if (event.is_hint) {
    gdk_event_request_motions(&event);
  }

  // TODO(wyck): handle dragging
  // Apparently it's our job to determine the difference between a move and a
  // drag.  We should dispatch OnMouseDragged with ET_MOUSE_DRAGGED instead.
  // It's unclear what constitutes the dragging state.  Which button(s)?
  int flags = Event::GetFlagsFromGdkState(event.state);
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED, event.x, event.y, flags);
  root_view->OnMouseMoved(mouse_move);
}

// Specialized dispatch for GDK_ENTER_NOTIFY events
static void DispatchEnterNotifyGtk(const GdkEventCrossing& event,
                                   RootView* root_view) {
  // TODO(wyck): I'm not sure if this is necessary yet
  int flags = (Event::GetFlagsFromGdkState(event.state) &
               ~(Event::EF_LEFT_BUTTON_DOWN |
                 Event::EF_MIDDLE_BUTTON_DOWN |
                 Event::EF_RIGHT_BUTTON_DOWN));
  MouseEvent mouse_move(Event::ET_MOUSE_MOVED, event.x, event.y, flags);
  root_view->OnMouseMoved(mouse_move);
}

// Specialized dispatch for GDK_LEAVE_NOTIFY events
static void DispatchLeaveNotifyGtk(const GdkEventCrossing& event,
                                   RootView* root_view) {
  // TODO(wyck): I'm not sure if this is necessary yet
  root_view->ProcessOnMouseExited();
}

// Dispatch an input-related GdkEvent to a RootView
static void DispatchEventToRootViewGtk(GdkEvent* event, RootView* root_view) {
  if (!event) {
    DLOG(WARNING) << "DispatchEventToRootView was passed a null GdkEvent";
    return;
  }
  if (!root_view) {
    DLOG(WARNING) << "DispatchEventToRootView was passed a null RootView";
    return;
  }
  switch (event->type) {
    case GDK_BUTTON_PRESS:
      DispatchButtonPressGtk(*reinterpret_cast<GdkEventButton*>(event),
                             root_view);
      break;
    case GDK_BUTTON_RELEASE:
      DispatchButtonReleaseGtk(*reinterpret_cast<GdkEventButton*>(event),
                               root_view);
      break;
    case GDK_MOTION_NOTIFY:
      DispatchMotionNotifyGtk(*reinterpret_cast<GdkEventMotion*>(event),
                              root_view);
      break;
    case GDK_ENTER_NOTIFY:
      DispatchEnterNotifyGtk(*reinterpret_cast<GdkEventCrossing*>(event),
                             root_view);
      break;
    case GDK_LEAVE_NOTIFY:
      DispatchLeaveNotifyGtk(*reinterpret_cast<GdkEventCrossing*>(event),
                             root_view);
      break;
    case GDK_2BUTTON_PRESS:
      DispatchButtonPressGtk(*reinterpret_cast<GdkEventButton*>(event),
                             root_view);
      break;
    case GDK_3BUTTON_PRESS:
      DispatchButtonPressGtk(*reinterpret_cast<GdkEventButton*>(event),
                             root_view);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// Called for input-related events only.  Dispatches them directly to the
// associated RootView.
//
// TODO(wyck): Make X Windows version of this function. (see earlier comment)
static void HijackEventForTouchUIGtk(GdkEvent* event) {
  // TODO(wyck): something like this...
  RootView* root_view = FindRootViewForGdkEvent(event);
  if (!root_view) {
    DLOG(WARNING) << "no RootView found for that GdkEvent";
    return;
  }
  DispatchEventToRootViewGtk(event, root_view);
}

// returns true if the GdkEvent is a touch-related input event.
//
// TODO(wyck): Make X Windows version of this function. (see earlier comment)
//
// If the X Windows events are not-prefiltered, then we can provide a filtering
// function similar to this GdkEvent-specific function.  Otherwise this function
// is not needed at all for the X Windows version.
static bool IsTouchEventGtk(GdkEvent* event) {
  switch (event->type) {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      return true;
    default:
      return false;
  }
}

// This is the public entry point for the touch event dispatcher.
//
// Hijacks input-related events and routes them directly to a widget_gtk.
// Returns false for non-input-related events, in which case the caller is still
// responsible for dispatching the event.
//
// TODO(wyck): Make X Windows version of this function. (see earlier comment)
bool DispatchEventForTouchUIGtk(GdkEvent* event) {
  // is this is an input-related event...
  if (IsTouchEventGtk(event)) {
    HijackEventForTouchUIGtk(event);
    return true;
  } else {
    return false;
  }
}
}  // namespace views
