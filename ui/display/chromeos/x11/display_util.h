// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_DISPLAY_UTIL_H_
#define UI_DISPLAY_CHROMEOS_DISPLAY_UTIL_H_

#include <string>

#include "ui/display/display_constants.h"
#include "ui/display/display_export.h"

typedef unsigned long RROutput;

namespace ui {

// Returns the OutputType by matching known type prefixes to |name|. Returns
// OUTPUT_TYPE_UNKNOWN if no valid match.
DISPLAY_EXPORT OutputType GetOutputTypeFromName(const std::string& name);

// Generate the human readable string from EDID obtained from |output|.
DISPLAY_EXPORT std::string GetDisplayName(RROutput output);

// Gets the overscan flag from |output| and stores to |flag|. Returns true if
// the flag is found. Otherwise returns false and doesn't touch |flag|. The
// output will produce overscan if |flag| is set to true, but the output may
// still produce overscan even though it returns true and |flag| is set to
// false.
DISPLAY_EXPORT bool GetOutputOverscanFlag(RROutput output, bool* flag);

DISPLAY_EXPORT bool ParseOutputOverscanFlag(const unsigned char* prop,
                                            unsigned long nitems,
                                            bool* flag);

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_DISPLAY_UTIL_H_
