// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_X_EVENTS_X_UTILS_H_
#define UI_EVENTS_X_EVENTS_X_UTILS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

typedef union _XEvent XEvent;

namespace ui {

// Get the EventType from a XEvent.
EventType EventTypeFromXEvent(const XEvent& xev);

// Get the EventFlags from a XEvent.
int EventFlagsFromXEvent(const XEvent& xev);

// Get the timestamp from a XEvent.
base::TimeDelta EventTimeFromXEvent(const XEvent& xev);

// Get the location from a XEvent.  The coordinate system of the resultant
// |Point| has the origin at top-left of the "root window".  The nature of
// this "root window" and how it maps to platform-specific drawing surfaces is
// defined in ui/aura/root_window.* and ui/aura/window_tree_host*.
gfx::Point EventLocationFromXEvent(const XEvent& xev);

// Gets the location in native system coordinate space.
gfx::Point EventSystemLocationFromXEvent(const XEvent& xev);

// Returns the 'real' button for an event. The button reported in slave events
// does not take into account any remapping (e.g. using xmodmap), while the
// button reported in master events do. This is a utility function to always
// return the mapped button.
int EventButtonFromXEvent(const XEvent& xev);

// Returns the flags of the button that changed during a press/release.
int GetChangedMouseButtonFlagsFromXEvent(const XEvent& xev);

// Returns the detailed pointer information for mouse events.
PointerDetails GetMousePointerDetailsFromXEvent(const XEvent& xev);

// Gets the mouse wheel offsets from a XEvent.
gfx::Vector2d GetMouseWheelOffsetFromXEvent(const XEvent& xev);

// Clear the touch id from bookkeeping if it is a release/cancel event.
void ClearTouchIdIfReleasedFromXEvent(const XEvent& xev);

// Gets the touch id from a XEvent.
int GetTouchIdFromXEvent(const XEvent& xev);

// Gets the radius along the X/Y axis from a XEvent. Default is 1.0.
float GetTouchRadiusXFromXEvent(const XEvent& xev);
float GetTouchRadiusYFromXEvent(const XEvent& xev);

// Gets the angle of the major axis away from the X axis. Default is 0.0.
float GetTouchAngleFromXEvent(const XEvent& xev);

// Gets the force from a native_event. Normalized to be [0, 1]. Default is 0.0.
float GetTouchForceFromXEvent(const XEvent& xev);

// Returns whether this is a scroll event and optionally gets the amount to be
// scrolled. |x_offset|, |y_offset| and |finger_count| can be NULL.
bool GetScrollOffsetsFromXEvent(const XEvent& xev,
                                float* x_offset,
                                float* y_offset,
                                float* x_offset_ordinal,
                                float* y_offset_ordinal,
                                int* finger_count);

// Gets the fling velocity from a XEvent. is_cancel is set to true if
// this was a tap down, intended to stop an ongoing fling.
bool GetFlingDataFromXEvent(const XEvent& xev,
                            float* vx,
                            float* vy,
                            float* vx_ordinal,
                            float* vy_ordinal,
                            bool* is_cancel);

}  // namespace ui

#endif  // UI_EVENTS_X_EVENTS_X_UTILS_H_
