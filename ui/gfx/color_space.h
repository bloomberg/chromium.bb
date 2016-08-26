// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace gfx {

class ICCProfile;
class ColorSpaceToColorSpaceTransform;

// Used to represet a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class GFX_EXPORT ColorSpace {
 public:
  enum class PrimaryID : uint16_t {
    // The first 0-255 values should match the H264 specification.
    RESERVED0 = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    RESERVED = 3,
    BT470M = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    FILM = 8,
    BT2020 = 9,
    SMPTEST428_1 = 10,
    SMPTEST431_2 = 11,
    SMPTEST432_1 = 12,

    // Chrome-specific values start at 1000.
    XYZ_D50 = 1000,
    // TODO(hubbe): We need to store the primaries.
    CUSTOM = 1001,
    LAST = CUSTOM
  };

  enum class TransferID : uint16_t {
    // The first 0-255 values should match the H264 specification.
    RESERVED0 = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    RESERVED = 3,
    GAMMA22 = 4,
    GAMMA28 = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    LINEAR = 8,
    LOG = 9,
    LOG_SQRT = 10,
    IEC61966_2_4 = 11,
    BT1361_ECG = 12,
    IEC61966_2_1 = 13,
    BT2020_10 = 14,
    BT2020_12 = 15,
    SMPTEST2084 = 16,
    SMPTEST428_1 = 17,

    // Chrome-specific values start at 1000.
    GAMMA24 = 1000,

    // TODO(hubbe): Need to store an approximation of the gamma function(s).
    CUSTOM = 1001,
    LAST = CUSTOM,
  };

  enum class MatrixID : int16_t {
    // The first 0-255 values should match the H264 specification.
    RGB = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    RESERVED = 3,
    FCC = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    YCOCG = 8,
    BT2020_NCL = 9,
    BT2020_CL = 10,
    YDZDX = 11,

    // Chrome-specific values start at 1000
    LAST = YDZDX,
  };

  // The h264 spec declares this as bool, so only the the first two values
  // correspond to the h264 spec. Chrome-specific values can start at 2.
  // We use an enum instead of a bool becuase different bit depths may have
  // different definitions of what "limited" means.
  enum class RangeID : int8_t {
    FULL = 0,
    LIMITED = 1,

    LAST = LIMITED
  };

  ColorSpace();
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID full_range);

  static ColorSpace CreateSRGB();
  static ColorSpace CreateXYZD50();

  // TODO: Remove these, and replace with more generic constructors.
  static ColorSpace CreateJpeg();
  static ColorSpace CreateREC601();
  static ColorSpace CreateREC709();

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;

 private:
  PrimaryID primaries_;
  TransferID transfer_;
  MatrixID matrix_;
  RangeID range_;

  // Only used if primaries_ == PrimaryID::CUSTOM
  float custom_primary_matrix_[12];

  // This is used to look up the ICCProfile from which this ColorSpace was
  // created, if possible.
  uint64_t icc_profile_id_ = 0;

  friend class ICCProfile;
  friend class ColorSpaceToColorSpaceTransform;
  friend struct IPC::ParamTraits<gfx::ColorSpace>;
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
