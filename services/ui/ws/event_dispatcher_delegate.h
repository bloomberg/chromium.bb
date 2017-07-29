// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EVENT_DISPATCHER_DELEGATE_H_
#define SERVICES_UI_WS_EVENT_DISPATCHER_DELEGATE_H_

#include <stdint.h>

#include "services/ui/common/types.h"

namespace gfx {
class Point;
}

namespace ui {

class Event;

namespace ws {

class Accelerator;
class ServerWindow;

// Used by EventDispatcher for mocking in tests.
class EventDispatcherDelegate {
 public:
  enum class AcceleratorPhase {
    PRE,
    POST,
  };

  virtual void OnAccelerator(uint32_t accelerator,
                             int64_t display_id,
                             const ui::Event& event,
                             AcceleratorPhase phase) = 0;

  virtual void SetFocusedWindowFromEventDispatcher(ServerWindow* window) = 0;
  virtual ServerWindow* GetFocusedWindowForEventDispatcher(
      int64_t display_id) = 0;

  // Called when capture should be set on the native display. |window| is the
  // window capture is being set on.
  virtual void SetNativeCapture(ServerWindow* window) = 0;

  // Called when the native display is having capture released. There is no
  // longer a ServerWindow holding capture.
  virtual void ReleaseNativeCapture() = 0;

  // Called when EventDispatcher has a new value for the cursor and our
  // delegate should perform the native updates.
  virtual void UpdateNativeCursorFromDispatcher() = 0;

  // Called when |window| has lost capture. The native display may still be
  // holding capture. The delegate should not change native display capture.
  // ReleaseNativeCapture() is invoked if appropriate.
  virtual void OnCaptureChanged(ServerWindow* new_capture,
                                ServerWindow* old_capture) = 0;

  virtual void OnMouseCursorLocationChanged(const gfx::Point& point,
                                            int64_t display_id) = 0;

  virtual void OnEventChangesCursorVisibility(const ui::Event& event,
                                              bool visible) = 0;

  virtual void OnEventChangesCursorTouchVisibility(const ui::Event& event,
                                                   bool visible) = 0;

  // Dispatches an event to the specific client.
  virtual void DispatchInputEventToWindow(ServerWindow* target,
                                          ClientSpecificId client_id,
                                          int64_t display_id,
                                          const ui::Event& event,
                                          Accelerator* accelerator) = 0;

  // Starts processing the next event in the event queue.
  virtual void ProcessNextAvailableEvent() = 0;

  // Returns the id of the client to send events to. |in_nonclient_area| is
  // true if the event occurred in the non-client area of the window.
  virtual ClientSpecificId GetEventTargetClientId(const ServerWindow* window,
                                                  bool in_nonclient_area) = 0;

  // Returns the window to start searching from at the specified location, or
  // null if there is a no window containing |location_in_display|.
  // |location_in_display| is in display coordinates and in pixels.
  // |location_in_display| and |display_id| are updated if the window we
  // found is on a different display than the originated display.
  // TODO(riajiang): No need to update |location_in_display| and |display_id|
  // after ozone drm can tell us the right display the cursor is on for
  // drag-n-drop events. crbug.com/726470
  virtual ServerWindow* GetRootWindowContaining(gfx::Point* location_in_display,
                                                int64_t* display_id) = 0;

  // Called when event dispatch could not find a target. OnAccelerator may still
  // be called.
  virtual void OnEventTargetNotFound(const ui::Event& event,
                                     int64_t display_id) = 0;

 protected:
  virtual ~EventDispatcherDelegate() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EVENT_DISPATCHER_DELEGATE_H_
