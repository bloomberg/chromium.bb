// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"
#include "ui/gfx/transform.h"

namespace gfx {

// static
ColorSpace ColorSpace::CreateVideo(int video_primary,
                                   int video_transfer,
                                   int video_matrix,
                                   RangeID range_id) {
  // TODO(hubbe): Use more context to decide how to handle UNSPECIFIED values.
  ColorSpace result;
  switch (video_primary) {
    default:
    case 0:  // RESERVED0
    case 1:  // BT709
    case 2:  // UNSPECIFIED
    case 3:  // RESERVED
      result.primaries_ = PrimaryID::BT709;
      break;
    case 4:  // BT470M
      result.primaries_ = PrimaryID::BT470M;
      break;
    case 5:  // BT470BG
      result.primaries_ = PrimaryID::BT470BG;
      break;
    case 6:  // SMPTE170M
      result.primaries_ = PrimaryID::SMPTE170M;
      break;
    case 7:  // SMPTE240M
      result.primaries_ = PrimaryID::SMPTE240M;
      break;
    case 8:  // FILM
      result.primaries_ = PrimaryID::FILM;
      break;
    case 9:  // BT2020
      result.primaries_ = PrimaryID::BT2020;
      break;
    case 10:  // SMPTEST428_1
      result.primaries_ = PrimaryID::SMPTEST428_1;
      break;
    case 11:  // SMPTEST431_2
      result.primaries_ = PrimaryID::SMPTEST431_2;
      break;
    case 12:  // SMPTEST432_1
      result.primaries_ = PrimaryID::SMPTEST432_1;
      break;
  }
  switch (video_transfer) {
    default:
    case 0:  // RESERVED0
    case 1:  // BT709
    case 2:  // UNSPECIFIED
    case 3:  // RESERVED
      result.transfer_ = TransferID::BT709;
      break;
    case 4:  // GAMMA22
      result.transfer_ = TransferID::GAMMA22;
      break;
    case 5:  // GAMMA28
      result.transfer_ = TransferID::GAMMA28;
      break;
    case 6:  // SMPTE170M
      result.transfer_ = TransferID::SMPTE170M;
      break;
    case 7:  // SMPTE240M
      result.transfer_ = TransferID::SMPTE240M;
      break;
    case 8:  // LINEAR
      result.transfer_ = TransferID::LINEAR;
      break;
    case 9:  // LOG
      result.transfer_ = TransferID::LOG;
      break;
    case 10:  // LOG_SQRT
      result.transfer_ = TransferID::LOG_SQRT;
      break;
    case 11:  // IEC61966_2_4
      result.transfer_ = TransferID::IEC61966_2_4;
      break;
    case 12:  // BT1361_ECG
      result.transfer_ = TransferID::BT1361_ECG;
      break;
    case 13:  // IEC61966_2_1
      result.transfer_ = TransferID::IEC61966_2_1;
      break;
    case 14:  // BT2020_10
      result.transfer_ = TransferID::BT2020_10;
      break;
    case 15:  // BT2020_12
      result.transfer_ = TransferID::BT2020_12;
      break;
    case 16:  // SMPTEST2084
      result.transfer_ = TransferID::SMPTEST2084;
      break;
    case 17:  // SMPTEST428_1
      result.transfer_ = TransferID::SMPTEST428_1;
      break;
    case 18:  // ARIB_STD_B67
      result.transfer_ = TransferID::ARIB_STD_B67;
      break;
  }
  switch (video_matrix) {
    case 0:  // RGB
      result.matrix_ = MatrixID::RGB;
      break;
    default:
    case 1:  // BT709
    case 2:  // UNSPECIFIED
    case 3:  // RESERVED
      result.matrix_ = MatrixID::BT709;
      break;
    case 4:  // FCC
      result.matrix_ = MatrixID::FCC;
      break;
    case 5:  // BT470BG
      result.matrix_ = MatrixID::BT470BG;
      break;
    case 6:  // SMPTE170M
      result.matrix_ = MatrixID::SMPTE170M;
      break;
    case 7:  // SMPTE240M
      result.matrix_ = MatrixID::SMPTE240M;
      break;
    case 8:  // YCOCG
      result.matrix_ = MatrixID::YCOCG;
      break;
    case 9:  // BT2020_NCL
      result.matrix_ = MatrixID::BT2020_NCL;
      break;
    case 10:  // BT2020_CL
      result.matrix_ = MatrixID::BT2020_CL;
      break;
    case 11:  // YDZDX
      result.matrix_ = MatrixID::YDZDX;
      break;
  }
  result.range_ = range_id;
  return result;
}

ColorSpace::ColorSpace() {}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(MatrixID::RGB),
      range_(RangeID::FULL) {}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range) {}

ColorSpace::ColorSpace(const ColorSpace& other)
    : primaries_(other.primaries_),
      transfer_(other.transfer_),
      matrix_(other.matrix_),
      range_(other.range_),
      icc_profile_id_(other.icc_profile_id_),
      icc_profile_sk_color_space_(other.icc_profile_sk_color_space_) {
  if (transfer_ == TransferID::CUSTOM) {
    memcpy(custom_transfer_params_, other.custom_transfer_params_,
           sizeof(custom_transfer_params_));
  }
  if (primaries_ == PrimaryID::CUSTOM) {
    memcpy(custom_primary_matrix_, other.custom_primary_matrix_,
           sizeof(custom_primary_matrix_));
  }
}
ColorSpace::~ColorSpace() = default;

bool ColorSpace::IsValid() const {
  return primaries_ != PrimaryID::INVALID && transfer_ != TransferID::INVALID &&
         matrix_ != MatrixID::INVALID && range_ != RangeID::INVALID;
}

// static
ColorSpace ColorSpace::CreateSRGB() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::RGB,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateCustom(const SkMatrix44& to_XYZD50,
                                    const SkColorSpaceTransferFn& fn) {
  ColorSpace result(ColorSpace::PrimaryID::CUSTOM,
                    ColorSpace::TransferID::CUSTOM, ColorSpace::MatrixID::RGB,
                    ColorSpace::RangeID::FULL);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      result.custom_primary_matrix_[3 * row + col] = to_XYZD50.get(row, col);
    }
  }
  result.custom_transfer_params_[0] = fn.fA;
  result.custom_transfer_params_[1] = fn.fB;
  result.custom_transfer_params_[2] = fn.fC;
  result.custom_transfer_params_[3] = fn.fD;
  result.custom_transfer_params_[4] = fn.fE;
  result.custom_transfer_params_[5] = fn.fF;
  result.custom_transfer_params_[6] = fn.fG;
  // TODO(ccameron): Use enums for near matches to know color spaces.
  return result;
}

// static
ColorSpace ColorSpace::CreateSCRGBLinear() {
  return ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR, MatrixID::RGB,
                    RangeID::FULL);
}

// Static
ColorSpace ColorSpace::CreateXYZD50() {
  return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                    RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateJpeg() {
  // TODO(ccameron): Determine which primaries and transfer function were
  // intended here.
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1,
                    MatrixID::SMPTE170M, RangeID::FULL);
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
         transfer_ == TransferID::ARIB_STD_B67 ||
         transfer_ == TransferID::LINEAR_HDR;
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

sk_sp<SkColorSpace> ColorSpace::ToSkColorSpace() const {
  // If we got a specific SkColorSpace from the ICCProfile that this color space
  // was created from, use that.
  if (icc_profile_sk_color_space_)
    return icc_profile_sk_color_space_;

  // Unspecified color spaces correspond to the null SkColorSpace.
  if (!IsValid())
    return nullptr;

  // Handle only full-range RGB spaces.
  if (matrix_ != MatrixID::RGB) {
    DLOG(ERROR) << "Not creating non-RGB SkColorSpace";
    return nullptr;
  }
  if (range_ != RangeID::FULL) {
    DLOG(ERROR) << "Not creating non-full-range SkColorSpace";
    return nullptr;
  }

  // Use the named SRGB and linear-SRGB instead of the generic constructors.
  if (primaries_ == PrimaryID::BT709) {
    if (transfer_ == TransferID::IEC61966_2_1)
      return SkColorSpace::MakeSRGB();
    if (transfer_ == TransferID::LINEAR || transfer_ == TransferID::LINEAR_HDR)
      return SkColorSpace::MakeSRGBLinear();
  }

  SkMatrix44 to_xyz_d50;
  GetPrimaryMatrix(&to_xyz_d50);

  // Use the named sRGB and linear transfer functions.
  if (transfer_ == TransferID::IEC61966_2_1) {
    return SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                 to_xyz_d50);
  }
  if (transfer_ == TransferID::LINEAR || transfer_ == TransferID::LINEAR_HDR) {
    return SkColorSpace::MakeRGB(SkColorSpace::kSRGB_RenderTargetGamma,
                                 to_xyz_d50);
  }

  // Use the parametric transfer function if no other option is available.
  SkColorSpaceTransferFn fn;
  if (!GetTransferFunction(&fn)) {
    DLOG(ERROR) << "Failed to parameterize transfer function for SkColorSpace";
    return nullptr;
  }
  return SkColorSpace::MakeRGB(fn, to_xyz_d50);
}

bool ColorSpace::GetICCProfile(ICCProfile* icc_profile) const {
  if (!IsValid())
    return false;

  // If this was created from an ICC profile, retrieve that exact profile.
  ICCProfile result;
  if (ICCProfile::FromId(icc_profile_id_, false, icc_profile))
    return true;

  // Otherwise, construct an ICC profile based on the best approximated
  // primaries and matrix.
  SkMatrix44 to_XYZD50_matrix;
  GetPrimaryMatrix(&to_XYZD50_matrix);
  SkColorSpaceTransferFn fn;
  if (!GetTransferFunction(&fn)) {
    DLOG(ERROR) << "Failed to get ColorSpace transfer function for ICCProfile.";
    return false;
  }
  sk_sp<SkData> data = SkICC::WriteToICC(fn, to_XYZD50_matrix);
  if (!data) {
    DLOG(ERROR) << "Failed to create SkICC.";
    return false;
  }
  *icc_profile = ICCProfile::FromData(data->data(), data->size());
  DCHECK(icc_profile->IsValid());
  return true;
}

void ColorSpace::GetPrimaryMatrix(SkMatrix44* to_XYZD50) const {
  SkColorSpacePrimaries primaries = {0};
  switch (primaries_) {
    case ColorSpace::PrimaryID::CUSTOM:
      to_XYZD50->set3x3RowMajorf(custom_primary_matrix_);
      return;

    case ColorSpace::PrimaryID::INVALID:
      to_XYZD50->setIdentity();
      return;

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

    case ColorSpace::PrimaryID::ADOBE_RGB:
      primaries.fRX = 0.6400f;
      primaries.fRY = 0.3300f;
      primaries.fGX = 0.2100f;
      primaries.fGY = 0.7100f;
      primaries.fBX = 0.1500f;
      primaries.fBY = 0.0600f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
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
    case ColorSpace::TransferID::LINEAR_HDR:
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
    case ColorSpace::TransferID::INVALID:
      break;
  }

  return false;
}

bool ColorSpace::GetInverseTransferFunction(SkColorSpaceTransferFn* fn) const {
  if (!GetTransferFunction(fn))
    return false;
  *fn = SkTransferFnInverse(*fn);
  return true;
}

void ColorSpace::GetTransferMatrix(SkMatrix44* matrix) const {
  float Kr = 0;
  float Kb = 0;
  switch (matrix_) {
    case ColorSpace::MatrixID::RGB:
    case ColorSpace::MatrixID::INVALID:
      matrix->setIdentity();
      return;

    case ColorSpace::MatrixID::BT709:
      Kr = 0.2126f;
      Kb = 0.0722f;
      break;

    case ColorSpace::MatrixID::FCC:
      Kr = 0.30f;
      Kb = 0.11f;
      break;

    case ColorSpace::MatrixID::BT470BG:
    case ColorSpace::MatrixID::SMPTE170M:
      Kr = 0.299f;
      Kb = 0.114f;
      break;

    case ColorSpace::MatrixID::SMPTE240M:
      Kr = 0.212f;
      Kb = 0.087f;
      break;

    case ColorSpace::MatrixID::YCOCG: {
      float data[16] = {
           0.25f, 0.5f,  0.25f, 0.5f,  // Y
          -0.25f, 0.5f, -0.25f, 0.5f,  // Cg
            0.5f, 0.0f,  -0.5f, 0.0f,  // Co
            0.0f, 0.0f,   0.0f, 1.0f
      };
      matrix->setRowMajorf(data);
      return;
    }

    // BT2020_CL is a special case.
    // Basically we return a matrix that transforms RGB values
    // to RYB values. (We replace the green component with the
    // the luminance.) Later steps will compute the Cb & Cr values.
    case ColorSpace::MatrixID::BT2020_CL: {
      Kr = 0.2627f;
      Kb = 0.0593f;
      float data[16] = {
          1.0f,           0.0f, 0.0f, 0.0f,  // R
            Kr, 1.0f - Kr - Kb,   Kb, 0.0f,  // Y
          0.0f,           0.0f, 1.0f, 0.0f,  // B
          0.0f,           0.0f, 0.0f, 1.0f
      };
      matrix->setRowMajorf(data);
      return;
    }

    case ColorSpace::MatrixID::BT2020_NCL:
      Kr = 0.2627f;
      Kb = 0.0593f;
      break;

    case ColorSpace::MatrixID::YDZDX: {
      float data[16] = {
          0.0f,              1.0f,             0.0f, 0.0f,  // Y
          0.0f,             -0.5f, 0.986566f / 2.0f, 0.5f,  // DX or DZ
          0.5f, -0.991902f / 2.0f,             0.0f, 0.5f,  // DZ or DX
          0.0f,              0.0f,             0.0f, 1.0f,
      };
      matrix->setRowMajorf(data);
      return;
    }
  }
  float Kg = 1.0f - Kr - Kb;
  float u_m = 0.5f / (1.0f - Kb);
  float v_m = 0.5f / (1.0f - Kr);
  float data[16] = {
                     Kr,        Kg,                Kb, 0.0f,  // Y
              u_m * -Kr, u_m * -Kg, u_m * (1.0f - Kb), 0.5f,  // U
      v_m * (1.0f - Kr), v_m * -Kg,         v_m * -Kb, 0.5f,  // V
                   0.0f,      0.0f,              0.0f, 1.0f,
  };
  matrix->setRowMajorf(data);
}

void ColorSpace::GetRangeAdjustMatrix(SkMatrix44* matrix) const {
  switch (range_) {
    case RangeID::FULL:
    case RangeID::INVALID:
      matrix->setIdentity();
      return;

    case RangeID::DERIVED:
    case RangeID::LIMITED:
      break;
  }
  switch (matrix_) {
    case MatrixID::RGB:
    case MatrixID::INVALID:
    case MatrixID::YCOCG:
      matrix->setScale(255.0f/219.0f, 255.0f/219.0f, 255.0f/219.0f);
      matrix->postTranslate(-16.0f/219.0f, -16.0f/219.0f, -16.0f/219.0f);
      break;

    case MatrixID::BT709:
    case MatrixID::FCC:
    case MatrixID::BT470BG:
    case MatrixID::SMPTE170M:
    case MatrixID::SMPTE240M:
    case MatrixID::BT2020_NCL:
    case MatrixID::BT2020_CL:
    case MatrixID::YDZDX:
      matrix->setScale(255.0f/219.0f, 255.0f/224.0f, 255.0f/224.0f);
      matrix->postTranslate(-16.0f/219.0f, -15.5f/224.0f, -15.5f/224.0f);
      break;
  }
}

}  // namespace gfx
