// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_UTIL_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_UTIL_H_

#include <string>
#include <vector>

#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"

namespace ui {

class DisplaySnapshot;

// Returns a string describing |state|.
std::string DisplayPowerStateToString(chromeos::DisplayPowerState state);

// Returns a string describing |state|.
std::string MultipleDisplayStateToString(MultipleDisplayState state);

// Returns the number of displays in |displays| that should be turned on, per
// |state|.  If |display_power| is non-NULL, it is updated to contain the
// on/off state of each corresponding entry in |displays|.
int DISPLAY_MANAGER_EXPORT
GetDisplayPower(const std::vector<DisplaySnapshot*>& displays,
                chromeos::DisplayPowerState state,
                std::vector<bool>* display_power);

// Returns whether the DisplayConnectionType |type| is a physically connected
// display. Currently DISPLAY_CONNECTION_TYPE_VIRTUAL and
// DISPLAY_CONNECTION_TYPE_NETWORK return false. All other types return true.
bool IsPhysicalDisplayType(ui::DisplayConnectionType type);

}  // namespace ui

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_DISPLAY_UTIL_H_
