// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BASE_EVENT_UTILS_H_
#define UI_EVENTS_BASE_EVENT_UTILS_H_

#include "base/basictypes.h"
#include "ui/events/events_base_export.h"

// Common functions to be used for all platforms.
namespace ui {

// Generate an unique identifier for events.
EVENTS_BASE_EXPORT uint32 GetNextTouchEventId();

// Checks if |flags| contains system key modifiers.
EVENTS_BASE_EXPORT bool IsSystemKeyModifier(int flags);

#if defined(OS_CHROMEOS)
// Sets the status of touch events to |enabled| on ChromeOS only. Non-ChromeOS
// platforms depend on the state of the |kTouchEvents| flags.
EVENTS_BASE_EXPORT void SetTouchEventsEnabled(bool enabled);
#endif  // defined(OS_CHROMEOS)

// Returns true if the touch events are enabled. On non-ChromeOS platforms, this
// depends on the state of the |kTouchEvents| flags.
EVENTS_BASE_EXPORT bool AreTouchEventsEnabled();

}  // namespace ui

#endif  // UI_EVENTS_BASE_EVENT_UTILS_H_
