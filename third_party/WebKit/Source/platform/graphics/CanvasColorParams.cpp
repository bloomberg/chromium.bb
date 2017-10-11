// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CanvasColorParams.h"

#include "platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gfx/color_space.h"

namespace blink {

namespace {

gfx::ColorSpace::PrimaryID GetPrimaryID(CanvasColorSpace color_space) {
  gfx::ColorSpace::PrimaryID primary_id = gfx::ColorSpace::PrimaryID::BT709;
  switch (color_space) {
    case kLegacyCanvasColorSpace:
    case kSRGBCanvasColorSpace:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      break;
    case kRec2020CanvasColorSpace:
      primary_id = gfx::ColorSpace::PrimaryID::BT2020;
      break;
    case kP3CanvasColorSpace:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTEST432_1;
      break;
  }
  return primary_id;
}

}  // namespace

CanvasColorParams::CanvasColorParams() = default;

CanvasColorParams::CanvasColorParams(CanvasColorSpace color_space,
                                     CanvasPixelFormat pixel_format,
                                     OpacityMode opacity_mode)
    : color_space_(color_space),
      pixel_format_(pixel_format),
      opacity_mode_(opacity_mode) {}

CanvasColorParams::CanvasColorParams(const SkImageInfo& info) {
  color_space_ = kLegacyCanvasColorSpace;
  pixel_format_ = kRGBA8CanvasPixelFormat;
  // When there is no color space information, the SkImage is in legacy mode and
  // the color type is kN32_SkColorType (which translates to kRGBA8 canvas pixel
  // format).
  if (!info.colorSpace())
    return;
  if (SkColorSpace::Equals(info.colorSpace(), SkColorSpace::MakeSRGB().get()))
    color_space_ = kSRGBCanvasColorSpace;
  else if (SkColorSpace::Equals(
               info.colorSpace(),
               SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                     SkColorSpace::kRec2020_Gamut)
                   .get()))
    color_space_ = kRec2020CanvasColorSpace;
  else if (SkColorSpace::Equals(
               info.colorSpace(),
               SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                     SkColorSpace::kDCIP3_D65_Gamut)
                   .get()))
    color_space_ = kP3CanvasColorSpace;
  if (info.colorType() == kRGBA_F16_SkColorType)
    pixel_format_ = kF16CanvasPixelFormat;
}

void CanvasColorParams::SetCanvasColorSpace(CanvasColorSpace color_space) {
  color_space_ = color_space;
}

void CanvasColorParams::SetCanvasPixelFormat(CanvasPixelFormat pixel_format) {
  pixel_format_ = pixel_format;
}

void CanvasColorParams::SetOpacityMode(OpacityMode opacity_mode) {
  opacity_mode_ = opacity_mode;
}

bool CanvasColorParams::LinearPixelMath() const {
  return color_space_ != kLegacyCanvasColorSpace;
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpaceForSkSurfaces() const {
  switch (color_space_) {
    case kLegacyCanvasColorSpace:
      return nullptr;
    case kSRGBCanvasColorSpace:
      if (pixel_format_ == kF16CanvasPixelFormat)
        return SkColorSpace::MakeSRGBLinear();
      return SkColorSpace::MakeSRGB();
    case kRec2020CanvasColorSpace:
      return SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                   SkColorSpace::kRec2020_Gamut);
    case kP3CanvasColorSpace:
      return SkColorSpace::MakeRGB(SkColorSpace::kLinear_RenderTargetGamma,
                                   SkColorSpace::kDCIP3_D65_Gamut);
  }
  return nullptr;

  // TODO(ccameron): This should return GetSkColorSpace if linear pixel math was
  // requested, and nullptr otherwise.
}

SkColorType CanvasColorParams::GetSkColorType() const {
  if (pixel_format_ == kF16CanvasPixelFormat)
    return kRGBA_F16_SkColorType;
  return kN32_SkColorType;
}

SkAlphaType CanvasColorParams::GetSkAlphaType() const {
  if (opacity_mode_ == kOpaque)
    return kOpaque_SkAlphaType;
  return kPremul_SkAlphaType;
}

const SkSurfaceProps* CanvasColorParams::GetSkSurfaceProps() const {
  static const SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  if (opacity_mode_ == kOpaque)
    return nullptr;
  return &disable_lcd_props;
}

uint8_t CanvasColorParams::BytesPerPixel() const {
  return SkColorTypeBytesPerPixel(GetSkColorType());
}

gfx::ColorSpace CanvasColorParams::GetSamplerGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = GetPrimaryID(color_space_);

  // TODO(ccameron): This needs to take into account whether or not this texture
  // will be sampled in linear or nonlinear space.
  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::IEC61966_2_1;
  if (pixel_format_ == kF16CanvasPixelFormat)
    transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;

  return gfx::ColorSpace(primary_id, transfer_id);
}

gfx::ColorSpace CanvasColorParams::GetStorageGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = GetPrimaryID(color_space_);

  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::IEC61966_2_1;
  if (pixel_format_ == kF16CanvasPixelFormat)
    transfer_id = gfx::ColorSpace::TransferID::LINEAR_HDR;

  return gfx::ColorSpace(primary_id, transfer_id);
}

sk_sp<SkColorSpace> CanvasColorParams::GetSkColorSpace() const {
  SkColorSpace::Gamut gamut = SkColorSpace::kSRGB_Gamut;
  switch (color_space_) {
    case kLegacyCanvasColorSpace:
    case kSRGBCanvasColorSpace:
      gamut = SkColorSpace::kSRGB_Gamut;
      break;
    case kRec2020CanvasColorSpace:
      gamut = SkColorSpace::kRec2020_Gamut;
      break;
    case kP3CanvasColorSpace:
      gamut = SkColorSpace::kDCIP3_D65_Gamut;
      break;
  }

  SkColorSpace::RenderTargetGamma gamma = SkColorSpace::kSRGB_RenderTargetGamma;
  if (pixel_format_ == kF16CanvasPixelFormat)
    gamma = SkColorSpace::kLinear_RenderTargetGamma;

  return SkColorSpace::MakeRGB(gamma, gamut);
}

}  // namespace blink
