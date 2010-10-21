// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TOUCHUI_TOUCH_EVENT_DISPATCHER_GTK_H_
#define VIEWS_TOUCHUI_TOUCH_EVENT_DISPATCHER_GTK_H_

namespace views {

// Dispatches a GdkEvent to the appropriate RootView.
// Returns true if the function dispatched (handled) the event.
// Returns false if the caller should dispatch the event.
//   FYI: That would typically be done with gtk_main_do_event(event)
bool DispatchEventForTouchUIGtk(GdkEvent* gdk_event);

}  // namespace views

#endif  // VIEWS_TOUCHUI_TOUCH_EVENT_DISPATCHER_GTK_H_
