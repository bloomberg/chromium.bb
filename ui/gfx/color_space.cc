// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/icc_profile.h"

namespace gfx {

namespace {

SkColorSpaceTransferFn InvertTransferFn(SkColorSpaceTransferFn fn) {
  SkColorSpaceTransferFn fn_inv = {0};
  if (fn.fA > 0 && fn.fG > 0) {
    double a_to_the_g = pow(fn.fA, fn.fG);
    fn_inv.fA = 1.f / a_to_the_g;
    fn_inv.fB = -fn.fE / a_to_the_g;
    fn_inv.fG = 1.f / fn.fG;
  }
  fn_inv.fD = fn.fC * fn.fD + fn.fF;
  fn_inv.fE = -fn.fB / fn.fA;
  if (fn.fC != 0) {
    fn_inv.fC = 1.f / fn.fC;
    fn_inv.fF = -fn.fF / fn.fC;
  }
  return fn_inv;
}
};

ColorSpace::PrimaryID ColorSpace::PrimaryIDFromInt(int primary_id) {
  if (primary_id < 0 || primary_id > static_cast<int>(PrimaryID::LAST))
    return PrimaryID::UNKNOWN;
  if (primary_id > static_cast<int>(PrimaryID::LAST_STANDARD_VALUE) &&
      primary_id < 1000)
    return PrimaryID::UNKNOWN;
  return static_cast<PrimaryID>(primary_id);
}

ColorSpace::TransferID ColorSpace::TransferIDFromInt(int transfer_id) {
  if (transfer_id < 0 || transfer_id > static_cast<int>(TransferID::LAST))
    return TransferID::UNKNOWN;
  if (transfer_id > static_cast<int>(TransferID::LAST_STANDARD_VALUE) &&
      transfer_id < 1000)
    return TransferID::UNKNOWN;
  return static_cast<TransferID>(transfer_id);
}

ColorSpace::MatrixID ColorSpace::MatrixIDFromInt(int matrix_id) {
  if (matrix_id < 0 || matrix_id > static_cast<int>(MatrixID::LAST))
    return MatrixID::UNKNOWN;
  if (matrix_id > static_cast<int>(MatrixID::LAST_STANDARD_VALUE) &&
      matrix_id < 1000)
    return MatrixID::UNKNOWN;
  return static_cast<MatrixID>(matrix_id);
}

ColorSpace::ColorSpace() {
  memset(custom_transfer_params_, 0, sizeof(custom_transfer_params_));
  memset(custom_primary_matrix_, 0, sizeof(custom_primary_matrix_));
}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range) {
  memset(custom_transfer_params_, 0, sizeof(custom_transfer_params_));
  memset(custom_primary_matrix_, 0, sizeof(custom_primary_matrix_));
}

ColorSpace::ColorSpace(int primaries, int transfer, int matrix, RangeID range)
    : primaries_(PrimaryIDFromInt(primaries)),
      transfer_(TransferIDFromInt(transfer)),
      matrix_(MatrixIDFromInt(matrix)),
      range_(range) {
  memset(custom_transfer_params_, 0, sizeof(custom_transfer_params_));
  memset(custom_primary_matrix_, 0, sizeof(custom_primary_matrix_));
}

ColorSpace::ColorSpace(const ColorSpace& other)
    : primaries_(other.primaries_),
      transfer_(other.transfer_),
      matrix_(other.matrix_),
      range_(other.range_),
      icc_profile_id_(other.icc_profile_id_),
      sk_color_space_(other.sk_color_space_) {
  memcpy(custom_transfer_params_, other.custom_transfer_params_,
         sizeof(custom_transfer_params_));
  memcpy(custom_primary_matrix_, other.custom_primary_matrix_,
         sizeof(custom_primary_matrix_));
}

ColorSpace::~ColorSpace() = default;

// static
ColorSpace ColorSpace::CreateSRGB() {
  ColorSpace result(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::RGB,
                    RangeID::FULL);
  result.sk_color_space_ = SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named);
  return result;
}

// static
ColorSpace ColorSpace::CreateSCRGBLinear() {
  return ColorSpace(PrimaryID::BT709, TransferID::LINEAR, MatrixID::RGB,
                    RangeID::FULL);
}

// Static
ColorSpace ColorSpace::CreateXYZD50() {
  return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateJpeg() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::BT709,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateREC601() {
  return ColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                    MatrixID::SMPTE170M, RangeID::LIMITED);
}

// static
ColorSpace ColorSpace::CreateREC709() {
  return ColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                    RangeID::LIMITED);
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  if (primaries_ != other.primaries_ || transfer_ != other.transfer_ ||
      matrix_ != other.matrix_ || range_ != other.range_)
    return false;
  if (primaries_ == PrimaryID::CUSTOM &&
      memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
             sizeof(custom_primary_matrix_)))
    return false;
  if (transfer_ == TransferID::CUSTOM &&
      memcmp(custom_transfer_params_, other.custom_transfer_params_,
             sizeof(custom_transfer_params_)))
    return false;
  return true;
}

bool ColorSpace::IsHDR() const {
  return transfer_ == TransferID::SMPTEST2084 ||
         transfer_ == TransferID::ARIB_STD_B67;
}

bool ColorSpace::operator!=(const ColorSpace& other) const {
  return !(*this == other);
}

bool ColorSpace::operator<(const ColorSpace& other) const {
  if (primaries_ < other.primaries_)
    return true;
  if (primaries_ > other.primaries_)
    return false;
  if (transfer_ < other.transfer_)
    return true;
  if (transfer_ > other.transfer_)
    return false;
  if (matrix_ < other.matrix_)
    return true;
  if (matrix_ > other.matrix_)
    return false;
  if (range_ < other.range_)
    return true;
  if (range_ > other.range_)
    return false;
  if (primaries_ == PrimaryID::CUSTOM) {
    int primary_result =
        memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
               sizeof(custom_primary_matrix_));
    if (primary_result < 0)
      return true;
    if (primary_result > 0)
      return false;
  }
  if (transfer_ == TransferID::CUSTOM) {
    int transfer_result =
        memcmp(custom_transfer_params_, other.custom_transfer_params_,
               sizeof(custom_transfer_params_));
    if (transfer_result < 0)
      return true;
    if (transfer_result > 0)
      return false;
  }
  return false;
}

ColorSpace ColorSpace::FromSkColorSpace(
    const sk_sp<SkColorSpace>& sk_color_space) {
  if (!sk_color_space)
    return gfx::ColorSpace();
  if (SkColorSpace::Equals(
          sk_color_space.get(),
          SkColorSpace::MakeNamed(SkColorSpace::kSRGB_Named).get()))
    return gfx::ColorSpace::CreateSRGB();

  // TODO(crbug.com/634102): Add conversion to gfx::ColorSpace for
  // non-ICC-profile based color spaces.
  ICCProfile icc_profile = ICCProfile::FromSkColorSpace(sk_color_space);
  return icc_profile.GetColorSpace();
}

void ColorSpace::GetPrimaryMatrix(SkMatrix44* to_XYZD50) const {
  SkColorSpacePrimaries primaries = {0};
  switch (primaries_) {
    case ColorSpace::PrimaryID::CUSTOM:
      to_XYZD50->set3x3RowMajorf(custom_primary_matrix_);
      return;

    case ColorSpace::PrimaryID::RESERVED0:
    case ColorSpace::PrimaryID::RESERVED:
    case ColorSpace::PrimaryID::UNSPECIFIED:
    case ColorSpace::PrimaryID::UNKNOWN:
    case ColorSpace::PrimaryID::BT709:
      // BT709 is our default case. Put it after the switch just
      // in case we somehow get an id which is not listed in the switch.
      // (We don't want to use "default", because we want the compiler
      //  to tell us if we forgot some enum values.)
      primaries.fRX = 0.640f;
      primaries.fRY = 0.330f;
      primaries.fGX = 0.300f;
      primaries.fGY = 0.600f;
      primaries.fBX = 0.150f;
      primaries.fBY = 0.060f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::BT470M:
      primaries.fRX = 0.67f;
      primaries.fRY = 0.33f;
      primaries.fGX = 0.21f;
      primaries.fGY = 0.71f;
      primaries.fBX = 0.14f;
      primaries.fBY = 0.08f;
      primaries.fWX = 0.31f;
      primaries.fWY = 0.316f;
      break;

    case ColorSpace::PrimaryID::BT470BG:
      primaries.fRX = 0.64f;
      primaries.fRY = 0.33f;
      primaries.fGX = 0.29f;
      primaries.fGY = 0.60f;
      primaries.fBX = 0.15f;
      primaries.fBY = 0.06f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::SMPTE170M:
    case ColorSpace::PrimaryID::SMPTE240M:
      primaries.fRX = 0.630f;
      primaries.fRY = 0.340f;
      primaries.fGX = 0.310f;
      primaries.fGY = 0.595f;
      primaries.fBX = 0.155f;
      primaries.fBY = 0.070f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::FILM:
      primaries.fRX = 0.681f;
      primaries.fRY = 0.319f;
      primaries.fGX = 0.243f;
      primaries.fGY = 0.692f;
      primaries.fBX = 0.145f;
      primaries.fBY = 0.049f;
      primaries.fWX = 0.310f;
      primaries.fWY = 0.136f;
      break;

    case ColorSpace::PrimaryID::BT2020:
      primaries.fRX = 0.708f;
      primaries.fRY = 0.292f;
      primaries.fGX = 0.170f;
      primaries.fGY = 0.797f;
      primaries.fBX = 0.131f;
      primaries.fBY = 0.046f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::SMPTEST428_1:
      primaries.fRX = 1.0f;
      primaries.fRY = 0.0f;
      primaries.fGX = 0.0f;
      primaries.fGY = 1.0f;
      primaries.fBX = 0.0f;
      primaries.fBY = 0.0f;
      primaries.fWX = 1.0f / 3.0f;
      primaries.fWY = 1.0f / 3.0f;
      break;

    case ColorSpace::PrimaryID::SMPTEST431_2:
      primaries.fRX = 0.680f;
      primaries.fRY = 0.320f;
      primaries.fGX = 0.265f;
      primaries.fGY = 0.690f;
      primaries.fBX = 0.150f;
      primaries.fBY = 0.060f;
      primaries.fWX = 0.314f;
      primaries.fWY = 0.351f;
      break;

    case ColorSpace::PrimaryID::SMPTEST432_1:
      primaries.fRX = 0.680f;
      primaries.fRY = 0.320f;
      primaries.fGX = 0.265f;
      primaries.fGY = 0.690f;
      primaries.fBX = 0.150f;
      primaries.fBY = 0.060f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::XYZ_D50:
      primaries.fRX = 1.0f;
      primaries.fRY = 0.0f;
      primaries.fGX = 0.0f;
      primaries.fGY = 1.0f;
      primaries.fBX = 0.0f;
      primaries.fBY = 0.0f;
      primaries.fWX = 0.34567f;
      primaries.fWY = 0.35850f;
      break;
  }
  primaries.toXYZD50(to_XYZD50);
}

bool ColorSpace::GetTransferFunction(SkColorSpaceTransferFn* fn) const {
  // Default to F(x) = pow(x, 1)
  fn->fA = 1;
  fn->fB = 0;
  fn->fC = 1;
  fn->fD = 0;
  fn->fE = 0;
  fn->fF = 0;
  fn->fG = 1;

  switch (transfer_) {
    case ColorSpace::TransferID::CUSTOM:
      fn->fA = custom_transfer_params_[0];
      fn->fB = custom_transfer_params_[1];
      fn->fC = custom_transfer_params_[2];
      fn->fD = custom_transfer_params_[3];
      fn->fE = custom_transfer_params_[4];
      fn->fF = custom_transfer_params_[5];
      fn->fG = custom_transfer_params_[6];
      return true;
    case ColorSpace::TransferID::LINEAR:
      return true;
    case ColorSpace::TransferID::GAMMA22:
      fn->fG = 2.2f;
      return true;
    case ColorSpace::TransferID::GAMMA24:
      fn->fG = 2.4f;
      return true;
    case ColorSpace::TransferID::GAMMA28:
      fn->fG = 2.8f;
      return true;
    case ColorSpace::TransferID::RESERVED0:
    case ColorSpace::TransferID::RESERVED:
    case ColorSpace::TransferID::UNSPECIFIED:
    case ColorSpace::TransferID::UNKNOWN:
    // All unknown values default to BT709
    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
      fn->fA = 0.909672431050f;
      fn->fB = 0.090327568950f;
      fn->fC = 0.222222222222f;
      fn->fD = 0.081242862158f;
      fn->fG = 2.222222222222f;
      return true;
    case ColorSpace::TransferID::SMPTE240M:
      fn->fA = 0.899626676224f;
      fn->fB = 0.100373323776f;
      fn->fC = 0.250000000000f;
      fn->fD = 0.091286342118f;
      fn->fG = 2.222222222222f;
      return true;
    case ColorSpace::TransferID::IEC61966_2_1:
      fn->fA = 0.947867345704f;
      fn->fB = 0.052132654296f;
      fn->fC = 0.077399380805f;
      fn->fD = 0.040449937172f;
      fn->fG = 2.400000000000f;
      return true;
    case ColorSpace::TransferID::SMPTEST428_1:
      fn->fA = 0.225615407568f;
      fn->fE = -1.091041666667f;
      fn->fG = 2.600000000000f;
      return true;
    case ColorSpace::TransferID::IEC61966_2_4:
      // This could potentially be represented the same as IEC61966_2_1, but
      // it handles negative values differently.
      break;
    case ColorSpace::TransferID::ARIB_STD_B67:
    case ColorSpace::TransferID::BT1361_ECG:
    case ColorSpace::TransferID::LOG:
    case ColorSpace::TransferID::LOG_SQRT:
    case ColorSpace::TransferID::SMPTEST2084:
    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      break;
  }

  return false;
}

bool ColorSpace::GetInverseTransferFunction(SkColorSpaceTransferFn* fn) const {
  if (!GetTransferFunction(fn))
    return false;
  *fn = InvertTransferFn(*fn);
  return true;
}

}  // namespace gfx
