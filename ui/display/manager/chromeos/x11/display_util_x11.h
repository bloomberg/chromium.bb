// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_X11_DISPLAY_UTIL_X11_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_X11_DISPLAY_UTIL_X11_H_

#include <string>

#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"

typedef unsigned long XID;
typedef XID RROutput;

namespace display {

// Returns the DisplayConnectionType by matching known type prefixes to |name|.
// Returns DISPLAY_TYPE_UNKNOWN if no valid match.
DISPLAY_MANAGER_EXPORT DisplayConnectionType
GetDisplayConnectionTypeFromName(const std::string& name);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_X11_DISPLAY_UTIL_X11_H_
