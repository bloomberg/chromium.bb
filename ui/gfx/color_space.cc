// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <iomanip>
#include <map>
#include <sstream>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {

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

ColorSpace::ColorSpace(PrimaryID primaries,
                       const SkColorSpaceTransferFn& fn,
                       MatrixID matrix,
                       RangeID range)
    : primaries_(primaries), matrix_(matrix), range_(range) {
  SetCustomTransferFunction(fn);
}

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

ColorSpace::ColorSpace(ColorSpace&& other) = default;

ColorSpace& ColorSpace::operator=(const ColorSpace& other) = default;

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
ColorSpace ColorSpace::CreateDisplayP3D65() {
  return ColorSpace(PrimaryID::SMPTEST432_1, TransferID::IEC61966_2_1,
                    MatrixID::RGB, RangeID::FULL);
}

// static
ColorSpace ColorSpace::CreateExtendedSRGB() {
  return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1_HDR,
                    MatrixID::RGB, RangeID::FULL);
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
  result.SetCustomTransferFunction(fn);
  return result;
}

void ColorSpace::SetCustomTransferFunction(const SkColorSpaceTransferFn& fn) {
  custom_transfer_params_[0] = fn.fA;
  custom_transfer_params_[1] = fn.fB;
  custom_transfer_params_[2] = fn.fC;
  custom_transfer_params_[3] = fn.fD;
  custom_transfer_params_[4] = fn.fE;
  custom_transfer_params_[5] = fn.fF;
  custom_transfer_params_[6] = fn.fG;
  // TODO(ccameron): Use enums for near matches to know color spaces.
  transfer_ = TransferID::CUSTOM;
}

// static
ColorSpace ColorSpace::CreateCustom(const SkMatrix44& to_XYZD50,
                                    ColorSpace::TransferID transfer_id) {
  DCHECK_NE(transfer_id, ColorSpace::TransferID::CUSTOM);
  DCHECK_NE(transfer_id, ColorSpace::TransferID::INVALID);
  ColorSpace result(ColorSpace::PrimaryID::CUSTOM, transfer_id,
                    ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      result.custom_primary_matrix_[3 * row + col] = to_XYZD50.get(row, col);
    }
  }
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
  if (primaries_ == PrimaryID::CUSTOM) {
    if (memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
               sizeof(custom_primary_matrix_))) {
      return false;
    }
  }
  if (transfer_ == TransferID::CUSTOM) {
    if (memcmp(custom_transfer_params_, other.custom_transfer_params_,
               sizeof(custom_transfer_params_))) {
      return false;
    }
  }
  if (primaries_ == PrimaryID::ICC_BASED ||
      transfer_ == TransferID::ICC_BASED) {
    if (icc_profile_id_ != other.icc_profile_id_)
      return false;
  }
  return true;
}

bool ColorSpace::IsHDR() const {
  return transfer_ == TransferID::SMPTEST2084 ||
         transfer_ == TransferID::ARIB_STD_B67 ||
         transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::IEC61966_2_1_HDR;
}

bool ColorSpace::FullRangeEncodedValues() const {
  return transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::IEC61966_2_1_HDR ||
         transfer_ == TransferID::BT1361_ECG ||
         transfer_ == TransferID::IEC61966_2_4;
}

bool ColorSpace::IsParametric() const {
  return primaries_ != PrimaryID::ICC_BASED &&
         transfer_ != TransferID::ICC_BASED;
}

ColorSpace ColorSpace::GetParametricApproximation() const {
  // If this is parametric already, return it directly.
  if (IsParametric())
    return *this;

  // Query the ICC profile, if available, for the parametric approximation.
  ICCProfile icc_profile;
  if (icc_profile_id_ && GetICCProfile(&icc_profile)) {
    return icc_profile.GetParametricColorSpace();
  } else {
    DLOG(ERROR)
        << "Unable to acquire ICC profile for parametric approximation.";
  }

  // Fall back to sRGB if the ICC profile is no longer cached.
  return CreateSRGB();
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

size_t ColorSpace::GetHash() const {
  size_t result = (static_cast<size_t>(primaries_) << 0) |
                  (static_cast<size_t>(transfer_) << 8) |
                  (static_cast<size_t>(matrix_) << 16) |
                  (static_cast<size_t>(range_) << 24);
  if (primaries_ == PrimaryID::CUSTOM) {
    const uint32_t* params =
        reinterpret_cast<const uint32_t*>(custom_primary_matrix_);
    result ^= params[0];
    result ^= params[4];
    result ^= params[8];
  }
  if (transfer_ == TransferID::CUSTOM) {
    const uint32_t* params =
        reinterpret_cast<const uint32_t*>(custom_transfer_params_);
    result ^= params[3];
    result ^= params[6];
  }
  return result;
}

#define PRINT_ENUM_CASE(TYPE, NAME) \
  case TYPE::NAME:                  \
    ss << #NAME;                    \
    break;

std::string ColorSpace::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4);
  ss << "{primaries:";
  switch (primaries_) {
    PRINT_ENUM_CASE(PrimaryID, INVALID)
    PRINT_ENUM_CASE(PrimaryID, BT709)
    PRINT_ENUM_CASE(PrimaryID, BT470M)
    PRINT_ENUM_CASE(PrimaryID, BT470BG)
    PRINT_ENUM_CASE(PrimaryID, SMPTE170M)
    PRINT_ENUM_CASE(PrimaryID, SMPTE240M)
    PRINT_ENUM_CASE(PrimaryID, FILM)
    PRINT_ENUM_CASE(PrimaryID, BT2020)
    PRINT_ENUM_CASE(PrimaryID, SMPTEST428_1)
    PRINT_ENUM_CASE(PrimaryID, SMPTEST431_2)
    PRINT_ENUM_CASE(PrimaryID, SMPTEST432_1)
    PRINT_ENUM_CASE(PrimaryID, XYZ_D50)
    PRINT_ENUM_CASE(PrimaryID, ADOBE_RGB)
    PRINT_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
    PRINT_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
    PRINT_ENUM_CASE(PrimaryID, ICC_BASED)
    case PrimaryID::CUSTOM:
      ss << "[";
      for (size_t i = 0; i < 3; ++i) {
        ss << "[";
        for (size_t j = 0; j < 3; ++j)
          ss << custom_primary_matrix_[3 * i + j] << ",";
        ss << "],";
      }
      ss << "]";
      break;
  }
  ss << ", transfer:";
  switch (transfer_) {
    PRINT_ENUM_CASE(TransferID, INVALID)
    PRINT_ENUM_CASE(TransferID, BT709)
    PRINT_ENUM_CASE(TransferID, GAMMA18)
    PRINT_ENUM_CASE(TransferID, GAMMA22)
    PRINT_ENUM_CASE(TransferID, GAMMA24)
    PRINT_ENUM_CASE(TransferID, GAMMA28)
    PRINT_ENUM_CASE(TransferID, SMPTE170M)
    PRINT_ENUM_CASE(TransferID, SMPTE240M)
    PRINT_ENUM_CASE(TransferID, LINEAR)
    PRINT_ENUM_CASE(TransferID, LOG)
    PRINT_ENUM_CASE(TransferID, LOG_SQRT)
    PRINT_ENUM_CASE(TransferID, IEC61966_2_4)
    PRINT_ENUM_CASE(TransferID, BT1361_ECG)
    PRINT_ENUM_CASE(TransferID, IEC61966_2_1)
    PRINT_ENUM_CASE(TransferID, BT2020_10)
    PRINT_ENUM_CASE(TransferID, BT2020_12)
    PRINT_ENUM_CASE(TransferID, SMPTEST2084)
    PRINT_ENUM_CASE(TransferID, SMPTEST428_1)
    PRINT_ENUM_CASE(TransferID, ARIB_STD_B67)
    PRINT_ENUM_CASE(TransferID, SMPTEST2084_NON_HDR)
    PRINT_ENUM_CASE(TransferID, IEC61966_2_1_HDR)
    PRINT_ENUM_CASE(TransferID, LINEAR_HDR)
    PRINT_ENUM_CASE(TransferID, ICC_BASED)
    case TransferID::CUSTOM: {
      SkColorSpaceTransferFn fn;
      GetTransferFunction(&fn);
      ss << fn.fC << "*x + " << fn.fF << " if x < " << fn.fD << " else (";
      ss << fn.fA << "*x + " << fn.fB << ")**" << fn.fG << " + " << fn.fE;
      break;
    }
  }
  ss << ", matrix:";
  switch (matrix_) {
    PRINT_ENUM_CASE(MatrixID, INVALID)
    PRINT_ENUM_CASE(MatrixID, RGB)
    PRINT_ENUM_CASE(MatrixID, BT709)
    PRINT_ENUM_CASE(MatrixID, FCC)
    PRINT_ENUM_CASE(MatrixID, BT470BG)
    PRINT_ENUM_CASE(MatrixID, SMPTE170M)
    PRINT_ENUM_CASE(MatrixID, SMPTE240M)
    PRINT_ENUM_CASE(MatrixID, YCOCG)
    PRINT_ENUM_CASE(MatrixID, BT2020_NCL)
    PRINT_ENUM_CASE(MatrixID, BT2020_CL)
    PRINT_ENUM_CASE(MatrixID, YDZDX)
  }
  ss << ", range:";
  switch (range_) {
    PRINT_ENUM_CASE(RangeID, INVALID)
    PRINT_ENUM_CASE(RangeID, LIMITED)
    PRINT_ENUM_CASE(RangeID, FULL)
    PRINT_ENUM_CASE(RangeID, DERIVED)
  }
  ss << ", icc_profile_id:" << icc_profile_id_;
  ss << "}";
  return ss.str();
}

#undef PRINT_ENUM_CASE

ColorSpace ColorSpace::GetAsFullRangeRGB() const {
  ColorSpace result(*this);
  if (!IsValid())
    return result;
  result.matrix_ = MatrixID::RGB;
  result.range_ = RangeID::FULL;
  return result;
}

ColorSpace ColorSpace::GetRasterColorSpace() const {
  // Rasterization can only be done into parametric color spaces.
  if (!IsParametric())
    return GetParametricApproximation();
  // Rasterization doesn't support more than 8 bit unorm values. If the output
  // space has an extended range, use Display P3 for the rasterization space,
  // to get a somewhat wider color gamut.
  if (HasExtendedSkTransferFn())
    return CreateDisplayP3D65();
  return *this;
}

ColorSpace ColorSpace::GetBlendingColorSpace() const {
  // HDR output on windows requires output have a linear transfer function.
  // Linear blending breaks the web, so use extended-sRGB for blending.
  if (transfer_ == TransferID::LINEAR_HDR)
    return CreateExtendedSRGB();
  return *this;
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

  // Prefer to used the named gamma and gamut, if possible.
  bool has_named_gamma = true;
  SkColorSpace::RenderTargetGamma named_gamma =
      SkColorSpace::kSRGB_RenderTargetGamma;
  switch (transfer_) {
    case TransferID::IEC61966_2_1:
      break;
    case TransferID::LINEAR:
    case TransferID::LINEAR_HDR:
      named_gamma = SkColorSpace::kLinear_RenderTargetGamma;
      break;
    default:
      has_named_gamma = false;
      break;
  }
  bool has_named_gamut = true;
  SkColorSpace::Gamut named_gamut = SkColorSpace::kSRGB_Gamut;
  switch (primaries_) {
    case PrimaryID::BT709:
      break;
    case PrimaryID::ADOBE_RGB:
      named_gamut = SkColorSpace::kAdobeRGB_Gamut;
      break;
    case PrimaryID::SMPTEST432_1:
      named_gamut = SkColorSpace::kDCIP3_D65_Gamut;
      break;
    case PrimaryID::BT2020:
      named_gamut = SkColorSpace::kRec2020_Gamut;
      break;
    default:
      has_named_gamut = false;
      break;
  }
  if (has_named_gamut && has_named_gamma)
    return SkColorSpace::MakeRGB(named_gamma, named_gamut);

  // Use named gamma with custom primaries, if possible.
  SkMatrix44 to_xyz_d50;
  GetPrimaryMatrix(&to_xyz_d50);
  if (has_named_gamma)
    return SkColorSpace::MakeRGB(named_gamma, to_xyz_d50);

  // Use the parametric transfer function if there is no named transfer
  // function.
  SkColorSpaceTransferFn fn;
  if (!GetTransferFunction(&fn)) {
    DLOG(ERROR) << "Failed to parameterize transfer function for SkColorSpace";
    return nullptr;
  }
  if (has_named_gamut)
    return SkColorSpace::MakeRGB(fn, named_gamut);
  return SkColorSpace::MakeRGB(fn, to_xyz_d50);
}

bool ColorSpace::GetICCProfile(ICCProfile* icc_profile) const {
  if (!IsValid()) {
    DLOG(ERROR) << "Cannot fetch ICCProfile for invalid space.";
    return false;
  }
  if (matrix_ != MatrixID::RGB) {
    DLOG(ERROR) << "Not creating non-RGB ICCProfile";
    return false;
  }
  if (range_ != RangeID::FULL) {
    DLOG(ERROR) << "Not creating non-full-range ICCProfile";
    return false;
  }

  // If this was created from an ICC profile, retrieve that exact profile.
  ICCProfile result;
  if (ICCProfile::FromId(icc_profile_id_, icc_profile))
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

bool ColorSpace::GetICCProfileData(std::vector<char>* output_data) const {
  if (!IsValid()) {
    DLOG(ERROR) << "Cannot fetch ICCProfile for invalid space.";
    return false;
  }
  if (matrix_ != MatrixID::RGB) {
    DLOG(ERROR) << "Not creating non-RGB ICCProfile";
    return false;
  }
  if (range_ != RangeID::FULL) {
    DLOG(ERROR) << "Not creating non-full-range ICCProfile";
    return false;
  }

  // If this was created from an ICC profile, retrieve that exact profile.
  ICCProfile icc_profile;
  if (ICCProfile::FromId(icc_profile_id_, &icc_profile)) {
    *output_data = icc_profile.data_;
    return true;
  }

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
  if (!data || !data->size()) {
    DLOG(ERROR) << "Failed to create SkICC.";
    return false;
  }
  const char* data_as_char = reinterpret_cast<const char*>(data->data());
  output_data->insert(output_data->begin(), data_as_char,
                      data_as_char + data->size());
  return true;
}

void ColorSpace::GetPrimaryMatrix(SkMatrix44* to_XYZD50) const {
  SkColorSpacePrimaries primaries = {0};
  switch (primaries_) {
    case ColorSpace::PrimaryID::CUSTOM:
      to_XYZD50->set3x3RowMajorf(custom_primary_matrix_);
      return;

    case ColorSpace::PrimaryID::INVALID:
    case ColorSpace::PrimaryID::ICC_BASED:
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

    case ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
      primaries.fRX = 0.63002f;
      primaries.fRY = 0.34000f;
      primaries.fGX = 0.29505f;
      primaries.fGY = 0.60498f;
      primaries.fBX = 0.15501f;
      primaries.fBY = 0.07701f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
      primaries.fRX = 0.01f;
      primaries.fRY = 0.98f;
      primaries.fGX = 0.01f;
      primaries.fGY = 0.01f;
      primaries.fBX = 0.98f;
      primaries.fBY = 0.01f;
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
    case ColorSpace::TransferID::GAMMA18:
      fn->fG = 1.801f;
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
    case ColorSpace::TransferID::SMPTE240M:
      fn->fA = 0.899626676224f;
      fn->fB = 0.100373323776f;
      fn->fC = 0.250000000000f;
      fn->fD = 0.091286342118f;
      fn->fG = 2.222222222222f;
      return true;
    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
    // With respect to rendering BT709
    //  * SMPTE 1886 suggests that we should be using gamma 2.4.
    //  * Most displays actually use a gamma of 2.2, and most media playing
    //    software uses the sRGB transfer function.
    //  * User studies shows that users don't really care.
    //  * Apple's CoreVideo uses gamma=1.961.
    // Bearing all of that in mind, use the same transfer funciton as sRGB,
    // which will allow more optimization, and will more closely match other
    // media players.
    case ColorSpace::TransferID::IEC61966_2_1:
    case ColorSpace::TransferID::IEC61966_2_1_HDR:
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
    case ColorSpace::TransferID::ICC_BASED:
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

bool ColorSpace::HasExtendedSkTransferFn() const {
  return transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::IEC61966_2_1_HDR;
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

std::ostream& operator<<(std::ostream& out, const ColorSpace& color_space) {
  return out << color_space.ToString();
}

}  // namespace gfx
