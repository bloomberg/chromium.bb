// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EVENT_TARGETER_DELEGATE_H_
#define SERVICES_UI_WS_EVENT_TARGETER_DELEGATE_H_

#include <stdint.h>

namespace gfx {
class Point;
}

namespace viz {
class HitTestQuery;
}

namespace ui {
namespace ws {
class ServerWindow;

// Used by EventTargeter to talk to WindowManagerState.
class EventTargeterDelegate {
 public:
  // Calls EventDispatcherDelegate::GetRootWindowContaining, see
  // event_dispatcher_delegate.h for details.
  virtual ServerWindow* GetRootWindowContaining(gfx::Point* location_in_display,
                                                int64_t* display_id) = 0;

  // Calls EventDispatcherDelegate::ProcessNextAvailableEvent, see
  // event_dispatcher_delegate.h for details.
  virtual void ProcessNextAvailableEvent() = 0;

  // Returns null if there's no display with |display_id|.
  virtual viz::HitTestQuery* GetHitTestQueryForDisplay(int64_t display_id) = 0;

  // |frame_sink_id| must be valid. Returns null if there's no window
  // associated with |frame_sink_id|.
  virtual ServerWindow* GetWindowFromFrameSinkId(
      const viz::FrameSinkId& frame_sink_id) = 0;

 protected:
  virtual ~EventTargeterDelegate() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EVENT_TARGETER_DELEGATE_H_
