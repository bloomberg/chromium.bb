// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_POINTER_WATCHER_H_
#define UI_VIEWS_POINTER_WATCHER_H_

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace ui {
class PointerEvent;
}

namespace views {
class Widget;

// An interface for read-only observation of pointer events (in particular, the
// events cannot be marked as handled). Only certain event types are supported.
// The |target| is the top-level widget that will receive the event, if any.
// To reduce IPC traffic from the window server, move events are not provided
// unless the app specifically requests them.
// NOTE: On mus this allows observation of events outside of windows owned
// by the current process, in which case the |target| will be null. On mus
// event.target() is always null.
class VIEWS_EXPORT PointerWatcher {
 public:
  PointerWatcher() {}

  virtual void OnPointerEventObserved(const ui::PointerEvent& event,
                                      const gfx::Point& location_in_screen,
                                      Widget* target) = 0;

  // PointerWatcher is informed of capture changes as it's common to create a
  // MouseEvent of type ui::ET_MOUSE_CAPTURE_CHANGED on capture change.
  virtual void OnMouseCaptureChanged() {}

 protected:
  virtual ~PointerWatcher() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerWatcher);
};

}  // namespace views

#endif  // UI_VIEWS_POINTER_WATCHER_H_
