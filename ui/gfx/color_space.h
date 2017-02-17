// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/gfx_export.h"

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace gfx {

class ICCProfile;

// Used to represet a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class GFX_EXPORT ColorSpace {
 public:
  enum class PrimaryID : uint16_t {
    INVALID,
    BT709,
    BT470M,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    FILM,
    BT2020,
    SMPTEST428_1,
    SMPTEST431_2,
    SMPTEST432_1,
    XYZ_D50,
    ADOBE_RGB,
    CUSTOM,
    LAST = CUSTOM
  };

  enum class TransferID : uint16_t {
    INVALID,
    BT709,
    GAMMA22,
    GAMMA24,
    GAMMA28,
    SMPTE170M,
    SMPTE240M,
    LINEAR,
    LOG,
    LOG_SQRT,
    IEC61966_2_4,
    BT1361_ECG,
    IEC61966_2_1,
    BT2020_10,
    BT2020_12,
    SMPTEST2084,
    SMPTEST428_1,
    ARIB_STD_B67,  // // AKA hybrid-log gamma, HLG.
    // This is an ad-hoc transfer function that decodes SMPTE 2084 content
    // into a 0-1 range more or less suitable for viewing on a non-hdr
    // display.
    SMPTEST2084_NON_HDR,
    // Like LINEAR, but intended for HDR. (can go outside of 0-1)
    LINEAR_HDR,
    CUSTOM,
    LAST = CUSTOM,
  };

  enum class MatrixID : int16_t {
    INVALID,
    RGB,
    BT709,
    FCC,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    YCOCG,
    BT2020_NCL,
    BT2020_CL,
    YDZDX,
    LAST = YDZDX,
  };

  enum class RangeID : int8_t {
    INVALID,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    LIMITED,
    // Full RGB color range with RGB valees from 0 to 255.
    FULL,
    // Range is defined by TransferID/MatrixID.
    DERIVED,
    LAST = DERIVED,
  };

  ColorSpace();
  ColorSpace(PrimaryID primaries, TransferID transfer);
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID full_range);
  ColorSpace(const ColorSpace& other);
  ~ColorSpace();

  // Create a color space with primary, transfer and matrix values from the
  // H264 specification (Table E-3 Colour Primaries, E-4 Transfer
  // Characteristics, and E-5 Matrix Coefficients in
  // https://www.itu.int/rec/T-REC-H.264/en).
  static ColorSpace CreateVideo(int h264_primary,
                                int h264_transfer,
                                int h264_matrix,
                                RangeID range_id);

  // Returns true if this is not the default-constructor object.
  bool IsValid() const;

  static ColorSpace CreateSRGB();
  static ColorSpace CreateCustom(const SkMatrix44& to_XYZD50,
                                 const SkColorSpaceTransferFn& fn);
  // scRGB is like RGB, but linear and values outside of 0-1 are allowed.
  // scRGB is normally used with fp16 textures.
  static ColorSpace CreateSCRGBLinear();
  static ColorSpace CreateXYZD50();

  // TODO: Remove these, and replace with more generic constructors.
  static ColorSpace CreateJpeg();
  static ColorSpace CreateREC601();
  static ColorSpace CreateREC709();

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;

  bool IsHDR() const;

  // This will return nullptr for non-RGB spaces, spaces with non-FULL
  // range, and unspecified spaces.
  sk_sp<SkColorSpace> ToSkColorSpace() const;

  // Populate |icc_profile| with an ICC profile that represents this color
  // space. Returns false if this space is not representable. This ICC profile
  // will be constructed ignoring the range adjust and transfer matrices (this
  // is to match the IOSurface interface which takes the ICC profile and range
  // and transfer matrices separately).
  bool GetICCProfile(ICCProfile* icc_profile) const;

  void GetPrimaryMatrix(SkMatrix44* to_XYZD50) const;
  bool GetTransferFunction(SkColorSpaceTransferFn* fn) const;
  bool GetInverseTransferFunction(SkColorSpaceTransferFn* fn) const;

  // For most formats, this is the RGB to YUV matrix.
  void GetTransferMatrix(SkMatrix44* matrix) const;
  void GetRangeAdjustMatrix(SkMatrix44* matrix) const;

 private:
  PrimaryID primaries_ = PrimaryID::INVALID;
  TransferID transfer_ = TransferID::INVALID;
  MatrixID matrix_ = MatrixID::INVALID;
  RangeID range_ = RangeID::INVALID;

  // Only used if primaries_ is PrimaryID::CUSTOM.
  float custom_primary_matrix_[9] = {0, 0, 0, 0, 0, 0, 0, 0};

  // Only used if transfer_ is TransferID::CUSTOM. This array consists of the A
  // through G entries of the SkColorSpaceTransferFn structure in alphabetical
  // order.
  float custom_transfer_params_[7] = {0, 0, 0, 0, 0, 0, 0};

  // This is used to look up the ICCProfile from which this ColorSpace was
  // created, if possible.
  uint64_t icc_profile_id_ = 0;
  sk_sp<SkColorSpace> icc_profile_sk_color_space_;

  friend class ICCProfile;
  friend class ColorTransform;
  friend class ColorTransformInternal;
  friend class ColorSpaceWin;
  friend struct IPC::ParamTraits<gfx::ColorSpace>;
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
};

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
