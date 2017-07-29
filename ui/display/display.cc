// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/icc_profile.h"

namespace display {
namespace {

constexpr int DEFAULT_BITS_PER_PIXEL = 24;
constexpr int DEFAULT_BITS_PER_COMPONENT = 8;

constexpr int HDR_BITS_PER_PIXEL = 48;
constexpr int HDR_BITS_PER_COMPONENT = 16;

// This variable tracks whether the forced device scale factor switch needs to
// be read from the command line, i.e. if it is set to -1 then the command line
// is checked.
int g_has_forced_device_scale_factor = -1;

// This variable caches the forced device scale factor value which is read off
// the command line. If the cache is invalidated by setting this variable to
// -1.0, we read the forced device scale factor again.
float g_forced_device_scale_factor = -1.0;

bool HasForceDeviceScaleFactorImpl() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kForceDeviceScaleFactor);
}

float GetForcedDeviceScaleFactorImpl() {
  double scale_in_double = 1.0;
  if (HasForceDeviceScaleFactorImpl()) {
    std::string value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceDeviceScaleFactor);
    if (!base::StringToDouble(value, &scale_in_double)) {
      LOG(ERROR) << "Failed to parse the default device scale factor:" << value;
      scale_in_double = 1.0;
    }
  }
  return static_cast<float>(scale_in_double);
}

int64_t internal_display_id_ = -1;

}  // namespace

bool CompareDisplayIds(int64_t id1, int64_t id2) {
  if (id1 == id2)
    return false;
  // Output index is stored in the first 8 bits. See GetDisplayIdFromEDID
  // in edid_parser.cc.
  int index_1 = id1 & 0xFF;
  int index_2 = id2 & 0xFF;
  DCHECK_NE(index_1, index_2) << id1 << " and " << id2;
  return Display::IsInternalDisplayId(id1) ||
         (index_1 < index_2 && !Display::IsInternalDisplayId(id2));
}

// static
float Display::GetForcedDeviceScaleFactor() {
  if (g_forced_device_scale_factor < 0)
    g_forced_device_scale_factor = GetForcedDeviceScaleFactorImpl();
  return g_forced_device_scale_factor;
}

// static
bool Display::HasForceDeviceScaleFactor() {
  if (g_has_forced_device_scale_factor == -1)
    g_has_forced_device_scale_factor = HasForceDeviceScaleFactorImpl();
  return !!g_has_forced_device_scale_factor;
}

// static
void Display::ResetForceDeviceScaleFactorForTesting() {
  g_has_forced_device_scale_factor = -1;
  g_forced_device_scale_factor = -1.0;
}

// static
void Display::SetForceDeviceScaleFactor(double dsf) {
  // Reset any previously set values and unset the flag.
  g_forced_device_scale_factor = -1.0;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor, base::StringPrintf("%.2f", dsf));
}

// static
gfx::ColorSpace Display::GetForcedColorProfile() {
  DCHECK(HasForceColorProfile());
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceColorProfile);
  if (value == "srgb") {
    return gfx::ColorSpace::CreateSRGB();
  } else if (value == "generic-rgb") {
    return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB,
                           gfx::ColorSpace::TransferID::GAMMA18);
  } else if (value == "color-spin-gamma24") {
    // Run this color profile through an ICC profile. The resulting color space
    // is slightly different from the input color space, and removing the ICC
    // profile would require rebaselineing many layout tests.
    gfx::ColorSpace color_space(
        gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN,
        gfx::ColorSpace::TransferID::GAMMA24);
    gfx::ICCProfile icc_profile;
    color_space.GetICCProfile(&icc_profile);
    return icc_profile.GetColorSpace();
  }
  LOG(ERROR) << "Invalid forced color profile";
  return gfx::ColorSpace::CreateSRGB();
}

// static
bool Display::HasForceColorProfile() {
  static bool has_force_color_profile =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceColorProfile);
  return has_force_color_profile;
}

Display::Display() : Display(kInvalidDisplayId) {}

Display::Display(int64_t id) : Display(id, gfx::Rect()) {}

Display::Display(int64_t id, const gfx::Rect& bounds)
    : id_(id),
      bounds_(bounds),
      work_area_(bounds),
      device_scale_factor_(GetForcedDeviceScaleFactor()),
      color_space_(HasForceColorProfile() ? GetForcedColorProfile()
                                          : gfx::ColorSpace::CreateSRGB()),
      color_depth_(DEFAULT_BITS_PER_PIXEL),
      depth_per_component_(DEFAULT_BITS_PER_COMPONENT) {
  if (base::FeatureList::IsEnabled(features::kHighDynamicRange)) {
    color_depth_ = HDR_BITS_PER_PIXEL;
    depth_per_component_ = HDR_BITS_PER_COMPONENT;
  }
#if defined(USE_AURA)
  SetScaleAndBounds(device_scale_factor_, bounds);
#endif
}

Display::Display(const Display& other) = default;

Display::~Display() {}

int Display::RotationAsDegree() const {
  switch (rotation_) {
    case ROTATE_0:
      return 0;
    case ROTATE_90:
      return 90;
    case ROTATE_180:
      return 180;
    case ROTATE_270:
      return 270;
  }
  NOTREACHED();
  return 0;
}

void Display::SetRotationAsDegree(int rotation) {
  switch (rotation) {
    case 0:
      rotation_ = ROTATE_0;
      break;
    case 90:
      rotation_ = ROTATE_90;
      break;
    case 180:
      rotation_ = ROTATE_180;
      break;
    case 270:
      rotation_ = ROTATE_270;
      break;
    default:
      // We should not reach that but we will just ignore the call if we do.
      NOTREACHED();
  }
}

gfx::Insets Display::GetWorkAreaInsets() const {
  return gfx::Insets(work_area_.y() - bounds_.y(), work_area_.x() - bounds_.x(),
                     bounds_.bottom() - work_area_.bottom(),
                     bounds_.right() - work_area_.right());
}

void Display::SetScaleAndBounds(float device_scale_factor,
                                const gfx::Rect& bounds_in_pixel) {
  gfx::Insets insets = bounds_.InsetsFrom(work_area_);
  if (!HasForceDeviceScaleFactor()) {
#if defined(OS_MACOSX)
    // Unless an explicit scale factor was provided for testing, ensure the
    // scale is integral.
    device_scale_factor = static_cast<int>(device_scale_factor);
#endif
    device_scale_factor_ = device_scale_factor;
  }
  device_scale_factor_ = std::max(1.0f, device_scale_factor_);
  bounds_ = gfx::Rect(gfx::ScaleToFlooredPoint(bounds_in_pixel.origin(),
                                               1.0f / device_scale_factor_),
                      gfx::ScaleToFlooredSize(bounds_in_pixel.size(),
                                              1.0f / device_scale_factor_));
#if defined(OS_ANDROID)
  size_in_pixels_ = bounds_in_pixel.size();
#endif  // defined(OS_ANDROID)
  UpdateWorkAreaFromInsets(insets);
}

void Display::SetSize(const gfx::Size& size_in_pixel) {
  gfx::Point origin = bounds_.origin();
#if defined(USE_AURA)
  origin = gfx::ScaleToFlooredPoint(origin, device_scale_factor_);
#endif
  SetScaleAndBounds(device_scale_factor_, gfx::Rect(origin, size_in_pixel));
}

void Display::UpdateWorkAreaFromInsets(const gfx::Insets& insets) {
  work_area_ = bounds_;
  work_area_.Inset(insets);
}

gfx::Size Display::GetSizeInPixel() const {
  // TODO(oshima): This should always use size_in_pixels_.
  if (!size_in_pixels_.IsEmpty())
    return size_in_pixels_;
  return gfx::ScaleToFlooredSize(size(), device_scale_factor_);
}

std::string Display::ToString() const {
  return base::StringPrintf(
      "Display[%lld] bounds=%s, workarea=%s, scale=%g, %s",
      static_cast<long long int>(id_), bounds_.ToString().c_str(),
      work_area_.ToString().c_str(), device_scale_factor_,
      IsInternal() ? "internal" : "external");
}

bool Display::IsInternal() const {
  return is_valid() && (id_ == internal_display_id_);
}

// static
int64_t Display::InternalDisplayId() {
  DCHECK_NE(kInvalidDisplayId, internal_display_id_);
  return internal_display_id_;
}

// static
void Display::SetInternalDisplayId(int64_t internal_display_id) {
  internal_display_id_ = internal_display_id;
}

// static
bool Display::IsInternalDisplayId(int64_t display_id) {
  DCHECK_NE(kInvalidDisplayId, display_id);
  return HasInternalDisplay() && internal_display_id_ == display_id;
}

// static
bool Display::HasInternalDisplay() {
  return internal_display_id_ != kInvalidDisplayId;
}

}  // namespace display
