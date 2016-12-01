// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/display_util.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/display/types/display_snapshot.h"

namespace ui {

std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "ALL_ON";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "ALL_OFF";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "INTERNAL_OFF_EXTERNAL_ON";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "INTERNAL_ON_EXTERNAL_OFF";
    default:
      return "unknown (" + base::IntToString(state) + ")";
  }
}

std::string MultipleDisplayStateToString(MultipleDisplayState state) {
  switch (state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      return "INVALID";
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      return "HEADLESS";
    case MULTIPLE_DISPLAY_STATE_SINGLE:
      return "SINGLE";
    case MULTIPLE_DISPLAY_STATE_DUAL_MIRROR:
      return "DUAL_MIRROR";
    case MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED:
      return "DUAL_EXTENDED";
    case MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED:
      return "MULTI_EXTENDED";
  }
  NOTREACHED() << "Unknown state " << state;
  return "INVALID";
}

int GetDisplayPower(const std::vector<DisplaySnapshot*>& displays,
                    chromeos::DisplayPowerState state,
                    std::vector<bool>* display_power) {
  int num_on_displays = 0;
  if (display_power)
    display_power->resize(displays.size());

  for (size_t i = 0; i < displays.size(); ++i) {
    bool internal = displays[i]->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool on =
        displays[i]->type() == DISPLAY_CONNECTION_TYPE_VIRTUAL ||
        state == chromeos::DISPLAY_POWER_ALL_ON ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON &&
         !internal) ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (display_power)
      (*display_power)[i] = on;
    if (on)
      num_on_displays++;
  }
  return num_on_displays;
}

bool IsPhysicalDisplayType(ui::DisplayConnectionType type) {
  return !(type &
           (DISPLAY_CONNECTION_TYPE_NETWORK | DISPLAY_CONNECTION_TYPE_VIRTUAL));
}

}  // namespace ui
