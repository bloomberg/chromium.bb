// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EVENT_FILTER_H_
#define UI_AURA_EVENT_FILTER_H_

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/base/events.h"

namespace ui {
class GestureEvent;
class KeyEvent;
class MouseEvent;
class TouchEvent;
}

namespace aura {

class Window;

// An object that filters events sent to an owner window. The filter can stop
// further processing of events.
//
// When the Desktop receives an event, it determines the "target window" for
// the event, this is typically the visible window whose bounds most tightly
// enclose the event coordinates, in the case of mouse events, or the focused
// window, in the case of key events.
//
// The Desktop then walks up the hierarchy from the target to its own window,
// collecting a list of EventFilters. This list is notified in reverse order
// (i.e. descending, from the Desktop's own event filter). Each filter gets a
// chance to prevent further processing of the event and/or take other actions.
class AURA_EXPORT EventFilter {
 public:
  virtual ~EventFilter() {}

  // Parameters: a |target| Window and the |event|. The target window is the
  // window the event was targeted at. If |event| is a LocatedEvent, its
  // coordinates are relative to |target|.

  // For all PreHandle*() functions that return a bool, return true if the
  // filter consumes the event and further processing (by the target, for
  // example) is not performed. Return false if the filter does not consume the
  // event and further processing is performed. Note that in this case the
  // filter may still perform some action, the return value simply indicates
  // that further processing can occur.

  virtual bool PreHandleKeyEvent(Window* target, ui::KeyEvent* event);
  virtual bool PreHandleMouseEvent(Window* target, ui::MouseEvent* event);

  // Returns a value other than ui::TOUCH_STATUS_UNKNOWN if the event is
  // consumed.
  virtual ui::TouchStatus PreHandleTouchEvent(Window* target,
                                              ui::TouchEvent* event);

  // Returns a value other than ui::GESTURE_STATUS_UNKNOWN if the gesture is
  // consumed.
  virtual ui::GestureStatus PreHandleGestureEvent(Window* target,
                                                  ui::GestureEvent* event);

  // The following methods are called to handle events _after_ they have been
  // handled by their target window's delegate. They will only be called if the
  // window delegate handlers did not consume the event (i.e. they returned
  // false). The return status of these methods is propagated back to
  // RootWindow.
  virtual bool PostHandleKeyEvent(Window* target, ui::KeyEvent* event);
  virtual bool PostHandleMouseEvent(Window* target, ui::MouseEvent* event);
  virtual ui::TouchStatus PostHandleTouchEvent(Window* target,
                                               ui::TouchEvent* event);
  virtual ui::GestureStatus PostHandleGestureEvent(Window* target,
                                                   ui::GestureEvent* event);
};

}  // namespace aura

#endif  // UI_AURA_EVENT_FILTER_H_
