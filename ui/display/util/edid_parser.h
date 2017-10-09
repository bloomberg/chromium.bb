// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_UTIL_EDID_PARSER_H_
#define UI_DISPLAY_UTIL_EDID_PARSER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "ui/display/util/display_util_export.h"

namespace gfx {
class Size;
}

struct SkColorSpacePrimaries;

// EDID (Extended Display Identification Data) is a format for monitor
// metadata. This provides a parser for the data.

namespace display {

// Generates the display id and product id for the pair of |edid| and |index|,
// and store in |display_id_out| and |product_id_out|. Returns true if the
// display id is successfully generated, or false otherwise.
DISPLAY_UTIL_EXPORT bool GetDisplayIdFromEDID(const std::vector<uint8_t>& edid,
                                              uint8_t index,
                                              int64_t* display_id_out,
                                              int64_t* product_id_out);

// Parses |edid| as EDID data and stores extracted data into |manufacturer_id|,
// |product_code|, |human_readable_name|, |active_pixel_out| and
// |physical_display_size_out|, then returns true. nullptr can be passed for
// unwanted output parameters.  Some devices (especially internal displays) may
// not have the field for |human_readable_name|, and it will return true in
// that case.
DISPLAY_UTIL_EXPORT bool ParseOutputDeviceData(
    const std::vector<uint8_t>& edid,
    uint16_t* manufacturer_id,
    uint16_t* product_code,
    std::string* human_readable_name,
    gfx::Size* active_pixel_out,
    gfx::Size* physical_display_size_out);

DISPLAY_UTIL_EXPORT bool ParseOutputOverscanFlag(
    const std::vector<uint8_t>& edid,
    bool* flag);

// Extracts from |edid| the |primaries| chromaticity coordinates (CIE xy
// coordinates for Red, Green and Blue channels and for the White Point).
DISPLAY_UTIL_EXPORT bool ParseChromaticityCoordinates(
    const std::vector<uint8_t>& edid,
    SkColorSpacePrimaries* primaries) WARN_UNUSED_RESULT;

// Extracts the gamma value from |edid| and returns it, or returns 0.0.
DISPLAY_UTIL_EXPORT bool ParseGammaValue(const std::vector<uint8_t>& edid,
                                         double* gamma) WARN_UNUSED_RESULT;

// Extracts the bits per channel from |edid| and returns it, or returns 0.
DISPLAY_UTIL_EXPORT bool ParseBitsPerChannel(const std::vector<uint8_t>& edid,
                                             int* bits_per_channel)
    WARN_UNUSED_RESULT;

}  // namespace display

#endif // UI_DISPLAY_UTIL_EDID_PARSER_H_
