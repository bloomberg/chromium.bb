// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/base_event_utils.h"

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"

namespace ui {

namespace {

#if defined(OS_CHROMEOS)
// Determines whether touch events are enabled or disabled on ChromeOS only.
bool touch_events_enabled = true;

const int kSystemKeyModifierMask = EF_ALT_DOWN | EF_COMMAND_DOWN;
#else
const int kSystemKeyModifierMask = EF_ALT_DOWN;
#endif  // defined(OS_CHROMEOS)

}  // namespace

base::StaticAtomicSequenceNumber g_next_event_id;

uint32 GetNextTouchEventId() {
  // Set the first touch event ID to 1 because we set id to 0 for other types
  // of events.
  uint32 id = g_next_event_id.GetNext();
  if (id == 0)
    id = g_next_event_id.GetNext();
  DCHECK_NE(0U, id);
  return id;
}

bool IsSystemKeyModifier(int flags) {
  // AltGr modifier is used to type alternative keys on certain keyboard layouts
  // so we don't consider keys with the AltGr modifier as a system key.
  return (kSystemKeyModifierMask & flags) != 0 &&
         (EF_ALTGR_DOWN & flags) == 0;
}

#if defined(OS_CHROMEOS)

void SetTouchEventsCrOsMasterSwitch(bool enabled) {
  touch_events_enabled = enabled;
}

bool GetTouchEventsCrOsMasterSwitch() {
  return touch_events_enabled;
}

#endif  // defined(OS_CHROMEOS)

}  // namespace ui

