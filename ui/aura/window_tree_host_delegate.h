// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_DELEGATE_H_
#define UI_AURA_WINDOW_TREE_HOST_DELEGATE_H_

#include "ui/aura/aura_export.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace ui {
class Event;
class EventProcessor;
class KeyEvent;
class MouseEvent;
class ScrollEvent;
class TouchEvent;
}

namespace aura {

class RootWindow;

// A private interface used by WindowTreeHost implementations to communicate
// with their owning RootWindow.
class AURA_EXPORT WindowTreeHostDelegate {
 public:
  virtual void OnHostCancelMode() = 0;

  // Called when the windowing system activates the window.
  virtual void OnHostActivated() = 0;

  // Called when system focus is changed to another window.
  virtual void OnHostLostWindowCapture() = 0;

  // Called when the windowing system has mouse grab because it's performing a
  // window move on our behalf, but we should still paint as if we're active.
  virtual void OnHostLostMouseGrab() = 0;

  virtual void OnHostMoved(const gfx::Point& origin) = 0;
  virtual void OnHostResized(const gfx::Size& size) = 0;

  virtual void OnCursorMovedToRootLocation(const gfx::Point& root_location) = 0;

  virtual WindowEventDispatcher* AsDispatcher() = 0;
  virtual const WindowEventDispatcher* AsDispatcher() const = 0;

  virtual ui::EventProcessor* GetEventProcessor() = 0;

 protected:
  virtual ~WindowTreeHostDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_DELEGATE_H_
