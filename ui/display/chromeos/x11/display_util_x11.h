// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_DISPLAY_UTIL_X11_H_
#define UI_DISPLAY_CHROMEOS_DISPLAY_UTIL_X11_H_

#include <string>

#include "ui/display/display_constants.h"
#include "ui/display/display_export.h"

typedef unsigned long XID;
typedef XID RROutput;

namespace ui {

// Returns the OutputType by matching known type prefixes to |name|. Returns
// OUTPUT_TYPE_UNKNOWN if no valid match.
DISPLAY_EXPORT OutputType GetOutputTypeFromName(const std::string& name);

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_DISPLAY_UTIL_X11_H_
