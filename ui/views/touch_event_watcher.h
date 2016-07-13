// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCH_EVENT_WATCHER_H_
#define UI_VIEWS_TOUCH_EVENT_WATCHER_H_

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace ui {
class LocatedEvent;
}

namespace views {
class Widget;

// An interface for read-only observation of touch events (in particular, the
// events cannot be marked as handled). Only touch pointer kind are supported.
// The |target| is the top-level widget that will receive the event, if any.
// NOTE: On mus this allows observation of events outside of windows owned
// by the current process, in which case the |target| will be null. On mus
// event.target() is always null.
class VIEWS_EXPORT TouchEventWatcher {
 public:
  TouchEventWatcher() {}
  virtual void OnTouchEventObserved(const ui::LocatedEvent& event,
                                    Widget* target) = 0;

 protected:
  virtual ~TouchEventWatcher() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEventWatcher);
};

}  // namespace views

#endif  // UI_VIEWS_TOUCH_EVENT_WATCHER_H_
