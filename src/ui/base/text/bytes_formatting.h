// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEXT_BYTES_FORMATTING_H_
#define UI_BASE_TEXT_BYTES_FORMATTING_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Simple API ------------------------------------------------------------------

// Simple call to return a byte quantity as a string in human-readable format.
// Ex: FormatBytes(512) => "512 B"
// Ex: FormatBytes(101479) => "99.1 kB"
UI_BASE_EXPORT base::string16 FormatBytes(int64_t bytes);

// Simple call to return a speed as a string in human-readable format.
// Ex: FormatSpeed(512) => "512 B/s"
// Ex: FormatSpeed(101479) => "99.1 kB/s"
UI_BASE_EXPORT base::string16 FormatSpeed(int64_t bytes);

// Less-Simple API -------------------------------------------------------------

enum DataUnits {
  DATA_UNITS_BYTE = 0,
  DATA_UNITS_KIBIBYTE,
  DATA_UNITS_MEBIBYTE,
  DATA_UNITS_GIBIBYTE,
  DATA_UNITS_TEBIBYTE,
  DATA_UNITS_PEBIBYTE
};

// Return the unit type that is appropriate for displaying the amount of bytes
// passed in. Most of the time, an explicit call to this isn't necessary; just
// use FormatBytes()/FormatSpeed() above.
UI_BASE_EXPORT DataUnits GetByteDisplayUnits(int64_t bytes);

// Return a byte quantity as a string in human-readable format with an optional
// unit suffix. Specify in the |units| argument the units to be used.
// Ex: FormatBytes(512, DATA_UNITS_KIBIBYTE, true) => "0.5 kB"
// Ex: FormatBytes(10*1024, DATA_UNITS_MEBIBYTE, false) => "0.1"
UI_BASE_EXPORT base::string16 FormatBytesWithUnits(int64_t bytes,
                                                   DataUnits units,
                                                   bool show_units);

// As above, but with "/s" units for speed values.
// Ex: FormatSpeed(512, DATA_UNITS_KIBIBYTE, true) => "0.5 kB/s"
// Ex: FormatSpeed(10*1024, DATA_UNITS_MEBIBYTE, false) => "0.1"
base::string16 FormatSpeedWithUnits(int64_t bytes,
                                    DataUnits units,
                                    bool show_units);

}  // namespace ui

#endif  // UI_BASE_TEXT_BYTES_FORMATTING_H_
