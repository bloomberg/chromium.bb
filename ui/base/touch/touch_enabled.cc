// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_enabled.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_switches.h"

namespace ui {

namespace {

bool ComputeTouchStatus() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const std::string touch_enabled_switch =
      command_line->HasSwitch(switches::kTouchEvents) ?
          command_line->GetSwitchValueASCII(switches::kTouchEvents) :
          switches::kTouchEventsAuto;

  if (touch_enabled_switch.empty() ||
      touch_enabled_switch == switches::kTouchEventsEnabled) {
    return true;
  }

  if (touch_enabled_switch == switches::kTouchEventsAuto)
    return IsTouchDevicePresent();

  DLOG_IF(ERROR, touch_enabled_switch != switches::kTouchEventsDisabled) <<
      "Invalid --touch-events option: " << touch_enabled_switch;
  return false;
}

}  // namespace

bool AreTouchEventsEnabled() {
  static bool touch_status = ComputeTouchStatus();

#if defined(OS_CHROMEOS)
  return touch_status && GetTouchEventsCrOsMasterSwitch();
#else
  return touch_status;
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace ui

