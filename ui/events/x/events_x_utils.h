// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/x/x11_types.h"

typedef union _XEvent XEvent;

namespace ui {

// Initializes a XEvent that holds XKeyEvent for testing. Note that ui::EF_
// flags should be passed as |flags|, not the native ones in <X11/X.h>.
EVENTS_EXPORT void InitXKeyEventForTesting(EventType type,
                                           KeyboardCode key_code,
                                           int flags,
                                           XEvent* event);

// Initializes a XEvent that holds XButtonEvent for testing. Note that ui::EF_
// flags should be passed as |flags|, not the native ones in <X11/X.h>.
EVENTS_EXPORT void InitXButtonEventForTesting(EventType type,
                                              int flags,
                                              XEvent* event);

// Initializes an XEvent for an Aura MouseWheelEvent. The underlying native
// event is an XButtonEvent.
EVENTS_EXPORT void InitXMouseWheelEventForTesting(int wheel_delta,
                                                  int flags,
                                                  XEvent* event);

}  // namespace ui
