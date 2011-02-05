// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_MOUSE_WATCHER_H_
#define VIEWS_MOUSE_WATCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ui/gfx/insets.h"

namespace views {

class View;

// MouseWatcherListener is notified when the mouse moves outside the view.
class MouseWatcherListener {
 public:
  virtual void MouseMovedOutOfView() = 0;

 protected:
  virtual ~MouseWatcherListener();
};

// MouseWatcher is used to watch mouse movement and notify its listener when the
// mouse moves outside the bounds of a view.
class MouseWatcher {
 public:
  // Creates a new MouseWatcher. |hot_zone_insets| is added to the bounds of
  // the view to determine the active zone. For example, if
  // |hot_zone_insets.bottom()| is 10, then the listener is not notified if
  // the y coordinate is between the origin of the view and height of the view
  // plus 10.
  MouseWatcher(views::View* host,
               MouseWatcherListener* listener,
               const gfx::Insets& hot_zone_insets);
  ~MouseWatcher();

  // Sets the amount to delay before notifying the listener when the mouse exits
  // the view by way of going to another window.
  void set_notify_on_exit_time_ms(int time) { notify_on_exit_time_ms_ = time; }

  // Starts watching mouse movements. When the mouse moves outside the bounds of
  // the view the listener is notified. |Start| may be invoked any number of
  // times. If the mouse moves outside the bounds of the view the listener is
  // notified and watcher stops watching events.
  void Start();

  // Stops watching mouse events.
  void Stop();

 private:
  class Observer;

  // Are we currently observing events?
  bool is_observing() const { return observer_.get() != NULL; }

  // Notifies the listener and stops watching events.
  void NotifyListener();

  // View we're listening for events over.
  View* host_;

  // Our listener.
  MouseWatcherListener* listener_;

  // Insets added to the bounds of the view.
  const gfx::Insets hot_zone_insets_;

  // Does the actual work of listening for mouse events.
  scoped_ptr<Observer> observer_;

  // See description above setter.
  int notify_on_exit_time_ms_;

  DISALLOW_COPY_AND_ASSIGN(MouseWatcher);
};

}  // namespace views

#endif  // VIEWS_MOUSE_WATCHER_H_
