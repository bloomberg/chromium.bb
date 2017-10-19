// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/managed_display_info.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/display/display.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if defined(OS_WIN)
#include <windows.h>
#include "ui/display/win/dpi.h"
#endif

namespace display {
namespace {

// Use larger than max int to catch overflow early.
const int64_t kSynthesizedDisplayIdStart = 2200000000LL;

int64_t synthesized_display_id = kSynthesizedDisplayIdStart;

const float kDpi96 = 96.0;

constexpr char kFallbackTouchDeviceName[] = "fallback_touch_device_name";

// Check the content of |spec| and fill |bounds| and |device_scale_factor|.
// Returns true when |bounds| is found.
bool GetDisplayBounds(const std::string& spec,
                      gfx::Rect* bounds,
                      float* device_scale_factor) {
  int width = 0;
  int height = 0;
  int x = 0;
  int y = 0;
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, device_scale_factor) >=
          2 ||
      sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             device_scale_factor) >= 4) {
    bounds->SetRect(x, y, width, height);
    return true;
  }
  return false;
}

// Display mode list is sorted by:
//  * the area in pixels in ascending order
//  * refresh rate in descending order
struct ManagedDisplayModeSorter {
  explicit ManagedDisplayModeSorter(bool is_internal)
      : is_internal(is_internal) {}

  bool operator()(const ManagedDisplayMode& a, const ManagedDisplayMode& b) {
    gfx::Size size_a_dip = a.GetSizeInDIP(is_internal);
    gfx::Size size_b_dip = b.GetSizeInDIP(is_internal);
    if (size_a_dip.GetArea() == size_b_dip.GetArea())
      return (a.refresh_rate() > b.refresh_rate());
    return (size_a_dip.GetArea() < size_b_dip.GetArea());
  }

  bool is_internal;
};

}  // namespace

TouchCalibrationData::TouchCalibrationData() {}

TouchCalibrationData::TouchCalibrationData(
    const TouchCalibrationData::CalibrationPointPairQuad& point_pairs,
    const gfx::Size& bounds) : point_pairs(point_pairs),
                               bounds(bounds) {}

TouchCalibrationData::TouchCalibrationData(
    const TouchCalibrationData& calibration_data)
    : point_pairs(calibration_data.point_pairs),
      bounds(calibration_data.bounds) {}

// static
uint32_t TouchCalibrationData::GenerateTouchDeviceIdentifier(
    const ui::TouchscreenDevice& device) {
  std::string hash_str = device.name + "-" +
                         base::UintToString(device.vendor_id) + "-" +
                         base::UintToString(device.product_id);
  return base::PersistentHash(hash_str);
}

// static
uint32_t TouchCalibrationData::GetFallbackTouchDeviceIdentifier() {
  ui::TouchscreenDevice device;
  device.name = kFallbackTouchDeviceName;
  device.vendor_id = device.product_id = 0;
  static const uint32_t kFallbackTouchDeviceIdentifier =
      GenerateTouchDeviceIdentifier(device);
  return kFallbackTouchDeviceIdentifier;
}

bool TouchCalibrationData::operator==(TouchCalibrationData other) const {
  if (bounds != other.bounds)
    return false;
  CalibrationPointPairQuad quad_1 = point_pairs;
  CalibrationPointPairQuad& quad_2 = other.point_pairs;

  // Make sure the point pairs are in the correct order.
  std::sort(quad_1.begin(), quad_1.end(), CalibrationPointPairCompare);
  std::sort(quad_2.begin(), quad_2.end(), CalibrationPointPairCompare);

  return quad_1 == quad_2;
}

ManagedDisplayMode::ManagedDisplayMode()
    : refresh_rate_(0.0f),
      is_interlaced_(false),
      native_(false),
      ui_scale_(1.0f),
      device_scale_factor_(1.0f) {}

ManagedDisplayMode::ManagedDisplayMode(const gfx::Size& size)
    : size_(size),
      refresh_rate_(0.0f),
      is_interlaced_(false),
      native_(false),
      ui_scale_(1.0f),
      device_scale_factor_(1.0f) {}

ManagedDisplayMode::ManagedDisplayMode(const gfx::Size& size,
                                       float refresh_rate,
                                       bool is_interlaced,
                                       bool native)
    : size_(size),
      refresh_rate_(refresh_rate),
      is_interlaced_(is_interlaced),
      native_(native),
      ui_scale_(1.0f),
      device_scale_factor_(1.0f) {}

ManagedDisplayMode::ManagedDisplayMode(const gfx::Size& size,
                                       float refresh_rate,
                                       bool is_interlaced,
                                       bool native,
                                       float ui_scale,
                                       float device_scale_factor)
    : size_(size),
      refresh_rate_(refresh_rate),
      is_interlaced_(is_interlaced),
      native_(native),
      ui_scale_(ui_scale),
      device_scale_factor_(device_scale_factor) {}

ManagedDisplayMode::~ManagedDisplayMode() = default;
ManagedDisplayMode::ManagedDisplayMode(const ManagedDisplayMode& other) =
    default;
ManagedDisplayMode& ManagedDisplayMode::operator=(
    const ManagedDisplayMode& other) = default;

gfx::Size ManagedDisplayMode::GetSizeInDIP(bool is_internal) const {
  gfx::SizeF size_dip(size_);
  size_dip.Scale(ui_scale_);
  // DSF=1.25 is special on internal display. The screen is drawn with DSF=1.25
  // but it doesn't affect the screen size computation.
  if (is_internal && device_scale_factor_ == 1.25f)
    return gfx::ToFlooredSize(size_dip);
  size_dip.Scale(1.0f / device_scale_factor_);
  return gfx::ToFlooredSize(size_dip);
}

bool ManagedDisplayMode::IsEquivalent(const ManagedDisplayMode& other) const {
  const float kEpsilon = 0.0001f;
  return size_ == other.size_ &&
         std::abs(ui_scale_ - other.ui_scale_) < kEpsilon &&
         std::abs(device_scale_factor_ - other.device_scale_factor_) < kEpsilon;
}

// static
ManagedDisplayInfo ManagedDisplayInfo::CreateFromSpec(const std::string& spec) {
  return CreateFromSpecWithID(spec, kInvalidDisplayId);
}

// static
ManagedDisplayInfo ManagedDisplayInfo::CreateFromSpecWithID(
    const std::string& spec,
    int64_t id) {
#if defined(OS_WIN)
  gfx::Rect bounds_in_native(
      gfx::Size(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));
#else
  // Default bounds for a display.
  const int kDefaultHostWindowX = 200;
  const int kDefaultHostWindowY = 200;
  const int kDefaultHostWindowWidth = 1366;
  const int kDefaultHostWindowHeight = 768;
  gfx::Rect bounds_in_native(kDefaultHostWindowX, kDefaultHostWindowY,
                             kDefaultHostWindowWidth, kDefaultHostWindowHeight);
#endif
  std::string main_spec = spec;

  float ui_scale = 1.0f;
  std::vector<std::string> parts = base::SplitString(
      main_spec, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 2) {
    double scale_in_double = 0;
    if (base::StringToDouble(parts[1], &scale_in_double))
      ui_scale = scale_in_double;
    main_spec = parts[0];
  }

  parts = base::SplitString(main_spec, "/", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  Display::Rotation rotation(Display::ROTATE_0);
  bool has_overscan = false;
  if (!parts.empty()) {
    main_spec = parts[0];
    if (parts.size() >= 2) {
      std::string options = parts[1];
      for (size_t i = 0; i < options.size(); ++i) {
        char c = options[i];
        switch (c) {
          case 'o':
            has_overscan = true;
            break;
          case 'r':  // rotate 90 degrees to 'right'.
            rotation = Display::ROTATE_90;
            break;
          case 'u':  // 180 degrees, 'u'pside-down.
            rotation = Display::ROTATE_180;
            break;
          case 'l':  // rotate 90 degrees to 'left'.
            rotation = Display::ROTATE_270;
            break;
        }
      }
    }
  }

  float device_scale_factor = 1.0f;
  if (!GetDisplayBounds(main_spec, &bounds_in_native, &device_scale_factor)) {
#if defined(OS_WIN)
    device_scale_factor = win::GetDPIScale();
#endif
  }

  ManagedDisplayModeList display_modes;
  parts = base::SplitString(main_spec, "#", base::KEEP_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 2) {
    size_t native_mode = 0;
    int largest_area = -1;
    float highest_refresh_rate = -1.0f;
    main_spec = parts[0];
    std::string resolution_list = parts[1];
    parts = base::SplitString(resolution_list, "|", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY);
    for (size_t i = 0; i < parts.size(); ++i) {
      gfx::Size size;
      float refresh_rate = 0.0f;
      bool is_interlaced = false;

      gfx::Rect mode_bounds;
      std::vector<std::string> resolution = base::SplitString(
          parts[i], "%", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (GetDisplayBounds(resolution[0], &mode_bounds, &device_scale_factor)) {
        size = mode_bounds.size();
        if (resolution.size() > 1)
          sscanf(resolution[1].c_str(), "%f", &refresh_rate);
        if (size.GetArea() >= largest_area &&
            refresh_rate > highest_refresh_rate) {
          // Use mode with largest area and highest refresh rate as native.
          largest_area = size.GetArea();
          highest_refresh_rate = refresh_rate;
          native_mode = i;
        }
        display_modes.push_back(ManagedDisplayMode(size, refresh_rate,
                                                   is_interlaced, false, 1.0,
                                                   device_scale_factor));
      }
    }
    ManagedDisplayMode dm = display_modes[native_mode];
    display_modes[native_mode] =
        ManagedDisplayMode(dm.size(), dm.refresh_rate(), dm.is_interlaced(),
                           true, dm.ui_scale(), dm.device_scale_factor());
  }

  if (id == kInvalidDisplayId)
    id = synthesized_display_id++;
  ManagedDisplayInfo display_info(
      id, base::StringPrintf("Display-%d", static_cast<int>(id)), has_overscan);
  display_info.set_device_scale_factor(device_scale_factor);
  display_info.SetRotation(rotation, Display::ROTATION_SOURCE_ACTIVE);
  display_info.set_configured_ui_scale(ui_scale);
  display_info.SetBounds(bounds_in_native);
  display_info.SetManagedDisplayModes(display_modes);

  // To test the overscan, it creates the default 5% overscan.
  if (has_overscan) {
    int width = bounds_in_native.width() / device_scale_factor / 40;
    int height = bounds_in_native.height() / device_scale_factor / 40;
    display_info.SetOverscanInsets(gfx::Insets(height, width, height, width));
    display_info.UpdateDisplaySize();
  }

  DVLOG(1) << "DisplayInfoFromSpec info=" << display_info.ToString()
           << ", spec=" << spec;
  return display_info;
}

ManagedDisplayInfo::ManagedDisplayInfo()
    : id_(kInvalidDisplayId),
      has_overscan_(false),
      active_rotation_source_(Display::ROTATION_SOURCE_UNKNOWN),
      touch_support_(Display::TOUCH_SUPPORT_UNKNOWN),
      device_scale_factor_(1.0f),
      device_dpi_(kDpi96),
      overscan_insets_in_dip_(0, 0, 0, 0),
      configured_ui_scale_(1.0f),
      native_(false),
      is_aspect_preserving_scaling_(false),
      clear_overscan_insets_(false) {}

ManagedDisplayInfo::ManagedDisplayInfo(int64_t id,
                                       const std::string& name,
                                       bool has_overscan)
    : id_(id),
      name_(name),
      has_overscan_(has_overscan),
      active_rotation_source_(Display::ROTATION_SOURCE_UNKNOWN),
      touch_support_(Display::TOUCH_SUPPORT_UNKNOWN),
      device_scale_factor_(1.0f),
      device_dpi_(kDpi96),
      overscan_insets_in_dip_(0, 0, 0, 0),
      configured_ui_scale_(1.0f),
      native_(false),
      is_aspect_preserving_scaling_(false),
      clear_overscan_insets_(false) {}

ManagedDisplayInfo::ManagedDisplayInfo(const ManagedDisplayInfo& other) =
    default;

ManagedDisplayInfo::~ManagedDisplayInfo() {}

void ManagedDisplayInfo::SetRotation(Display::Rotation rotation,
                                     Display::RotationSource source) {
  rotations_[source] = rotation;
  rotations_[Display::ROTATION_SOURCE_ACTIVE] = rotation;
  active_rotation_source_ = source;
}

Display::Rotation ManagedDisplayInfo::GetActiveRotation() const {
  return GetRotation(Display::ROTATION_SOURCE_ACTIVE);
}

Display::Rotation ManagedDisplayInfo::GetRotation(
    Display::RotationSource source) const {
  if (rotations_.find(source) == rotations_.end())
    return Display::ROTATE_0;
  return rotations_.at(source);
}

void ManagedDisplayInfo::Copy(const ManagedDisplayInfo& native_info) {
  DCHECK(id_ == native_info.id_);
  name_ = native_info.name_;
  has_overscan_ = native_info.has_overscan_;

  active_rotation_source_ = native_info.active_rotation_source_;
  touch_support_ = native_info.touch_support_;
  touch_device_identifiers_ = native_info.touch_device_identifiers_;
  device_scale_factor_ = native_info.device_scale_factor_;
  DCHECK(!native_info.bounds_in_native_.IsEmpty());
  bounds_in_native_ = native_info.bounds_in_native_;
  device_dpi_ = native_info.device_dpi_;
  size_in_pixel_ = native_info.size_in_pixel_;
  is_aspect_preserving_scaling_ = native_info.is_aspect_preserving_scaling_;
  display_modes_ = native_info.display_modes_;
  maximum_cursor_size_ = native_info.maximum_cursor_size_;

  // Rotation, ui_scale, color_profile and overscan are given by preference,
  // or unit tests. Don't copy if this native_info came from
  // DisplayChangeObserver.
  if (!native_info.native()) {
    // Update the overscan_insets_in_dip_ either if the inset should be
    // cleared, or has non empty insts.
    if (native_info.clear_overscan_insets())
      overscan_insets_in_dip_.Set(0, 0, 0, 0);
    else if (!native_info.overscan_insets_in_dip_.IsEmpty())
      overscan_insets_in_dip_ = native_info.overscan_insets_in_dip_;

    touch_calibration_data_map_ = native_info.touch_calibration_data_map_;

    rotations_ = native_info.rotations_;
    configured_ui_scale_ = native_info.configured_ui_scale_;
  }
}

void ManagedDisplayInfo::SetBounds(const gfx::Rect& new_bounds_in_native) {
  bounds_in_native_ = new_bounds_in_native;
  size_in_pixel_ = new_bounds_in_native.size();
  UpdateDisplaySize();
}

float ManagedDisplayInfo::GetDensityRatio() const {
  if (Use125DSFForUIScaling() && device_scale_factor_ == 1.25f)
    return 1.0f;
  return device_scale_factor_;
}

float ManagedDisplayInfo::GetEffectiveDeviceScaleFactor() const {
  if (Use125DSFForUIScaling() && device_scale_factor_ == 1.25f)
    return (configured_ui_scale_ == 0.8f) ? 1.25f : 1.0f;
  if (device_scale_factor_ == configured_ui_scale_)
    return 1.0f;
  return device_scale_factor_;
}

float ManagedDisplayInfo::GetEffectiveUIScale() const {
  if (Use125DSFForUIScaling() && device_scale_factor_ == 1.25f)
    return (configured_ui_scale_ == 0.8f) ? 1.0f : configured_ui_scale_;
  if (device_scale_factor_ == configured_ui_scale_)
    return 1.0f;
  return configured_ui_scale_;
}

void ManagedDisplayInfo::UpdateDisplaySize() {
  size_in_pixel_ = bounds_in_native_.size();
  if (!overscan_insets_in_dip_.IsEmpty()) {
    gfx::Insets insets_in_pixel =
        overscan_insets_in_dip_.Scale(device_scale_factor_);
    size_in_pixel_.Enlarge(-insets_in_pixel.width(), -insets_in_pixel.height());
  } else {
    overscan_insets_in_dip_.Set(0, 0, 0, 0);
  }

  if (GetActiveRotation() == Display::ROTATE_90 ||
      GetActiveRotation() == Display::ROTATE_270) {
    size_in_pixel_.SetSize(size_in_pixel_.height(), size_in_pixel_.width());
  }
  gfx::SizeF size_f(size_in_pixel_);
  size_f.Scale(GetEffectiveUIScale());
  size_in_pixel_ = gfx::ToFlooredSize(size_f);
}

void ManagedDisplayInfo::SetOverscanInsets(const gfx::Insets& insets_in_dip) {
  overscan_insets_in_dip_ = insets_in_dip;
}

gfx::Insets ManagedDisplayInfo::GetOverscanInsetsInPixel() const {
  return overscan_insets_in_dip_.Scale(device_scale_factor_);
}

void ManagedDisplayInfo::SetManagedDisplayModes(
    const ManagedDisplayModeList& display_modes) {
  display_modes_ = display_modes;
  std::sort(display_modes_.begin(), display_modes_.end(),
            ManagedDisplayModeSorter(Display::IsInternalDisplayId(id_)));
}

gfx::Size ManagedDisplayInfo::GetNativeModeSize() const {
  for (size_t i = 0; i < display_modes_.size(); ++i) {
    if (display_modes_[i].native())
      return display_modes_[i].size();
  }
  return gfx::Size();
}

std::string ManagedDisplayInfo::ToString() const {
  int rotation_degree = static_cast<int>(GetActiveRotation()) * 90;
  std::string touch_device_count_str =
      base::UintToString(touch_device_identifiers_.size());

  std::string result = base::StringPrintf(
      "ManagedDisplayInfo[%lld] native bounds=%s, size=%s, device-scale=%g, "
      "overscan=%s, rotation=%d, ui-scale=%g, touchscreen=%s, "
      "touch device count=[%" PRIuS "]",
      static_cast<long long int>(id_), bounds_in_native_.ToString().c_str(),
      size_in_pixel_.ToString().c_str(), device_scale_factor_,
      overscan_insets_in_dip_.ToString().c_str(), rotation_degree,
      configured_ui_scale_,
      touch_support_ == Display::TOUCH_SUPPORT_AVAILABLE
          ? "yes"
          : touch_support_ == Display::TOUCH_SUPPORT_UNAVAILABLE ? "no"
                                                                 : "unknown",
      touch_device_identifiers_.size());

  return result;
}

std::string ManagedDisplayInfo::ToFullString() const {
  std::string display_modes_str;
  ManagedDisplayModeList::const_iterator iter = display_modes_.begin();
  for (; iter != display_modes_.end(); ++iter) {
    ManagedDisplayMode m(*iter);
    if (!display_modes_str.empty())
      display_modes_str += ",";
    base::StringAppendF(&display_modes_str, "(%dx%d@%g%c%s %g/%g)",
                        m.size().width(), m.size().height(), m.refresh_rate(),
                        m.is_interlaced() ? 'I' : 'P', m.native() ? "(N)" : "",
                        m.ui_scale(), m.device_scale_factor());
  }
  return ToString() + ", display_modes==" + display_modes_str;
}

bool ManagedDisplayInfo::Use125DSFForUIScaling() const {
  return Display::IsInternalDisplayId(id_);
}

void ManagedDisplayInfo::AddTouchDevice(uint32_t touch_device_identifier) {
  touch_device_identifiers_.insert(touch_device_identifier);
  set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
}

void ManagedDisplayInfo::ClearTouchDevices() {
  touch_device_identifiers_.clear();
  set_touch_support(Display::TOUCH_SUPPORT_UNAVAILABLE);
}

bool ManagedDisplayInfo::HasTouchDevice(
    uint32_t touch_device_identifier) const {
  return touch_device_identifiers_.count(touch_device_identifier);
}

void ResetDisplayIdForTest() {
  synthesized_display_id = kSynthesizedDisplayIdStart;
}

void ManagedDisplayInfo::SetTouchCalibrationData(
    uint32_t touch_device_identifier,
    const TouchCalibrationData& touch_calibration_data) {
  touch_calibration_data_map_[touch_device_identifier] = touch_calibration_data;
}

const TouchCalibrationData& ManagedDisplayInfo::GetTouchCalibrationData(
    uint32_t touch_device_identifier) const {
  DCHECK(HasTouchCalibrationData(touch_device_identifier));

  // If the system does not have the calibration information for the touch
  // device identified with |touch_device_identifier|, use the fallback
  // calibration data if availble. This also helps keep support for legacy
  // touch calibration data which is stored in association with the fallback
  // device identifier.
  if (touch_calibration_data_map_.count(
          TouchCalibrationData::GetFallbackTouchDeviceIdentifier()) &&
      touch_calibration_data_map_.size() == 1) {
    return touch_calibration_data_map_.at(
        TouchCalibrationData::GetFallbackTouchDeviceIdentifier());
  }

  return touch_calibration_data_map_.at(touch_device_identifier);
}

void ManagedDisplayInfo::SetTouchCalibrationDataMap(
    const std::map<uint32_t, TouchCalibrationData>& data_map) {
  touch_calibration_data_map_ = data_map;
}

bool ManagedDisplayInfo::HasTouchCalibrationData(
    uint32_t touch_device_identifier) const {
  return touch_calibration_data_map_.count(touch_device_identifier) ||
         touch_calibration_data_map_.count(
             TouchCalibrationData::GetFallbackTouchDeviceIdentifier());
}

void ManagedDisplayInfo::ClearTouchCalibrationData(
    uint32_t touch_device_identifier) {
  touch_calibration_data_map_.erase(touch_device_identifier);
}

void ManagedDisplayInfo::ClearAllTouchCalibrationData() {
  touch_calibration_data_map_.clear();
}

}  // namespace display
