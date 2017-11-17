// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/touch_device_manager.h"

#include <algorithm>
#include <string>

#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"

namespace display {
namespace {

using DisplayInfoList = std::vector<ManagedDisplayInfo*>;
using DeviceList = std::vector<ui::TouchscreenDevice>;

constexpr char kFallbackTouchDeviceName[] = "fallback_touch_device_name";

// Returns true if |path| is likely a USB device.
bool IsDeviceConnectedViaUsb(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);

  for (const auto& component : components) {
    if (base::StartsWith(component, "usb",
                         base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }

  return false;
}

// Returns the UDL association score between |display| and |device|. A score <=
// 0 means that there is no association.
int GetUdlAssociationScore(const ManagedDisplayInfo* display,
                           const ui::TouchscreenDevice& device) {
  // If the devices are not both connected via USB, then there cannot be a UDL
  // association score.
  if (!IsDeviceConnectedViaUsb(display->sys_path()) ||
      !IsDeviceConnectedViaUsb(device.sys_path))
    return 0;

  // The association score is simply the number of prefix path components that
  // sysfs paths have in common.
  std::vector<base::FilePath::StringType> display_components;
  std::vector<base::FilePath::StringType> device_components;
  display->sys_path().GetComponents(&display_components);
  device.sys_path.GetComponents(&device_components);

  std::size_t largest_idx = 0;
  while (largest_idx < display_components.size() &&
         largest_idx < device_components.size() &&
         display_components[largest_idx] == device_components[largest_idx]) {
    ++largest_idx;
  }
  return largest_idx;
}

// Tries to find a UDL device that best matches |display|. Returns
// |devices.end()| if one is not found.
DeviceList::const_iterator GuessBestUdlDevice(const ManagedDisplayInfo* display,
                                              const DeviceList& devices) {
  int best_score = 0;
  DeviceList::const_iterator best_device_it = devices.end();

  // TODO(malaykeshav): Migrate to std::max_element in the future.
  for (auto it = devices.begin(); it != devices.end(); it++) {
    int score = GetUdlAssociationScore(display, *it);
    if (score > best_score) {
      best_score = score;
      best_device_it = it;
    }
  }
  return best_device_it;
}

// Returns true if |display| is internal.
bool IsInternalDisplay(const ManagedDisplayInfo* display) {
  return Display::IsInternalDisplayId(display->id());
}

// Returns true if |device| is internal.
bool IsInternalDevice(const ui::TouchscreenDevice& device) {
  return device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
}

// Returns a pointer to the internal display from the list of |displays|. Will
// return null if there is no internal display in the list.
ManagedDisplayInfo* GetInternalDisplay(DisplayInfoList* displays) {
  auto it =
      std::find_if(displays->begin(), displays->end(), &IsInternalDisplay);
  return it == displays->end() ? nullptr : *it;
}

// Clears any calibration data from |info_map| for the display identified by
// |display_id|.
void ClearCalibrationDataInMap(TouchDeviceManager::AssociationInfoMap& info_map,
                               int64_t display_id) {
  if (info_map.find(display_id) == info_map.end())
    return;
  info_map[display_id].calibration_data = TouchCalibrationData();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceIdentifier

// static
const TouchDeviceIdentifier&
TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier() {
  static const TouchDeviceIdentifier kFallTouchDeviceIdentifier(
      GenerateIdentifier(kFallbackTouchDeviceName, 0, 0));
  return kFallTouchDeviceIdentifier;
}

// static
uint32_t TouchDeviceIdentifier::GenerateIdentifier(std::string name,
                                                   uint16_t vendor_id,
                                                   uint16_t product_id) {
  std::string hash_str = name + "-" + base::UintToString(vendor_id) + "-" +
                         base::UintToString(product_id);
  return base::PersistentHash(hash_str);
}

// static
TouchDeviceIdentifier TouchDeviceIdentifier::FromDevice(
    const ui::TouchscreenDevice& touch_device) {
  return TouchDeviceIdentifier(GenerateIdentifier(
      touch_device.name, touch_device.vendor_id, touch_device.product_id));
}

TouchDeviceIdentifier::TouchDeviceIdentifier(uint32_t identifier)
    : id_(identifier) {}

TouchDeviceIdentifier::TouchDeviceIdentifier(const TouchDeviceIdentifier& other)
    : id_(other.id_) {}

TouchDeviceIdentifier& TouchDeviceIdentifier::operator=(
    TouchDeviceIdentifier other) {
  id_ = other.id_;
  return *this;
}

bool TouchDeviceIdentifier::operator<(const TouchDeviceIdentifier& rhs) const {
  return id_ < rhs.id_;
}

bool TouchDeviceIdentifier::operator==(const TouchDeviceIdentifier& rhs) const {
  return id_ == rhs.id_;
}

bool TouchDeviceIdentifier::operator!=(const TouchDeviceIdentifier& rhs) const {
  return !(*this == rhs);
}

std::string TouchDeviceIdentifier::ToString() const {
  return base::UintToString(id_);
}

////////////////////////////////////////////////////////////////////////////////
// TouchCalibrationData

// static
bool TouchCalibrationData::CalibrationPointPairCompare(
    const CalibrationPointPair& pair_1,
    const CalibrationPointPair& pair_2) {
  return pair_1.first.y() < pair_2.first.y()
             ? true
             : pair_1.first.x() < pair_2.first.x();
}

TouchCalibrationData::TouchCalibrationData() {}

TouchCalibrationData::TouchCalibrationData(
    const TouchCalibrationData::CalibrationPointPairQuad& point_pairs,
    const gfx::Size& bounds)
    : point_pairs(point_pairs), bounds(bounds) {}

TouchCalibrationData::TouchCalibrationData(
    const TouchCalibrationData& calibration_data)
    : point_pairs(calibration_data.point_pairs),
      bounds(calibration_data.bounds) {}

bool TouchCalibrationData::operator==(const TouchCalibrationData& other) const {
  if (bounds != other.bounds)
    return false;
  CalibrationPointPairQuad quad_1 = point_pairs;
  CalibrationPointPairQuad quad_2 = other.point_pairs;

  // Make sure the point pairs are in the correct order.
  std::sort(quad_1.begin(), quad_1.end(), CalibrationPointPairCompare);
  std::sort(quad_2.begin(), quad_2.end(), CalibrationPointPairCompare);

  return quad_1 == quad_2;
}

bool TouchCalibrationData::IsEmpty() const {
  return bounds.IsEmpty();
}

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceManager
TouchDeviceManager::TouchDeviceManager() {}

TouchDeviceManager::~TouchDeviceManager() {}

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceManager
// Touch screen association logic
void TouchDeviceManager::AssociateTouchscreens(
    std::vector<ManagedDisplayInfo>* all_displays,
    const std::vector<ui::TouchscreenDevice>& all_devices) {
  // |displays| and |devices| contain pointers directly to the values stored
  // inside of |all_displays| and |all_devices|. When a display or input device
  // has been associated, it is removed from the |displays| or |devices| list.

  // Construct our initial set of display/devices that we will process.
  DisplayInfoList displays;
  for (ManagedDisplayInfo& display : *all_displays) {
    // Reset touch support from the display.
    display.set_touch_support(Display::TOUCH_SUPPORT_UNAVAILABLE);
    displays.push_back(&display);
  }

  // Construct initial set of devices.
  DeviceList devices;
  for (const ui::TouchscreenDevice& device : all_devices)
    devices.push_back(device);

  if (VLOG_IS_ON(2)) {
    for (const ManagedDisplayInfo* display : displays) {
      VLOG(2) << "Received display " << display->name()
              << " (size: " << display->GetNativeModeSize().ToString() << ", "
              << "sys_path: " << display->sys_path().LossyDisplayName() << ")";
    }
    for (const ui::TouchscreenDevice& device : devices) {
      VLOG(2) << "Received device " << device.name
              << " (size: " << device.size.ToString()
              << ", sys_path: " << device.sys_path.LossyDisplayName() << ")";
    }
  }

  AssociateInternalDevices(&displays, &devices);
  AssociateUdlDevices(&displays, &devices);
  AssociateSameSizeDevices(&displays, &devices);
  AssociateToSingleDisplay(&displays, &devices);
  AssociateAnyRemainingDevices(&displays, &devices);

  for (const ui::TouchscreenDevice& device : devices)
    LOG(WARNING) << "Unmatched device " << device.name;
}

void TouchDeviceManager::AssociateInternalDevices(DisplayInfoList* displays,
                                                  DeviceList* devices) {
  VLOG(2) << "Trying to match internal devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  // Internal device assocation has a couple of gotchas:
  // - There can be internal devices but no internal display, or visa-versa.
  // - There can be multiple internal devices matching one internal display. We
  //   assume there is at most one internal display.

  // Capture the internal display reference as we remove it from |displays|.
  ManagedDisplayInfo* internal_display = GetInternalDisplay(displays);

  bool matched = false;

  // Remove all internal devices from |devices|. If we have an internal display,
  // then associate the device with the display before removing it.
  for (auto device_it = devices->begin(); device_it != devices->end();) {
    const ui::TouchscreenDevice& internal_device = *device_it;

    // Not an internal device, skip it.
    if (!IsInternalDevice(internal_device)) {
      ++device_it;
      continue;
    }

    if (internal_display) {
      VLOG(2) << "=> Matched device " << internal_device.name << " to display "
              << internal_display->name();
      Associate(internal_display, internal_device);
      matched = true;
    } else {
      // We do not want to associate an internal device to any other display.
      VLOG(2) << "=> Removing internal device " << internal_device.name;
    }
    device_it = devices->erase(device_it);
  }

  if (!matched && internal_display) {
    VLOG(2) << "=> No device found to match with internal display "
            << internal_display->name();
  }
}

void TouchDeviceManager::AssociateUdlDevices(DisplayInfoList* displays,
                                             DeviceList* devices) {
  VLOG(2) << "Trying to match udl devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  for (auto display_it = displays->begin(); display_it != displays->end();
       display_it++) {
    ManagedDisplayInfo* display = *display_it;
    auto device_it = GuessBestUdlDevice(display, *devices);

    if (device_it != devices->end()) {
      const ui::TouchscreenDevice& device = *device_it;
      VLOG(2) << "=> Matched device " << device.name << " to display "
              << display->name()
              << " (score=" << GetUdlAssociationScore(display, device) << ")";
      Associate(display, device);
      devices->erase(device_it);
    }
  }
}

void TouchDeviceManager::AssociateSameSizeDevices(DisplayInfoList* displays,
                                                  DeviceList* devices) {
  // Associate screens/displays with the same size.
  VLOG(2) << "Trying to match same-size devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  for (auto display_it = displays->begin(); display_it != displays->end();
       display_it++) {
    ManagedDisplayInfo* display = *display_it;
    // We do not want to associate any other devices to the internal display.
    if (IsInternalDisplay(display))
      continue;

    const gfx::Size native_size = display->GetNativeModeSize();

    // Try to find an input device with roughly the same size as the display.
    DeviceList::iterator device_it = std::find_if(
        devices->begin(), devices->end(),
        [&native_size](const ui::TouchscreenDevice& device) {
          // Allow 1 pixel difference between screen and touchscreen
          // resolutions. Because in some cases for monitor resolution
          // 1024x768 touchscreen's resolution would be 1024x768, but for
          // some 1023x767. It really depends on touchscreen's firmware
          // configuration.
          return std::abs(native_size.width() - device.size.width()) <= 1 &&
                 std::abs(native_size.height() - device.size.height()) <= 1;
        });

    if (device_it != devices->end()) {
      const ui::TouchscreenDevice& device = *device_it;
      VLOG(2) << "=> Matched device " << device.name << " to display "
              << display->name() << " (display_size: " << native_size.ToString()
              << ", device_size: " << device.size.ToString() << ")";
      Associate(display, device);

      device_it = devices->erase(device_it);
    }
  }
}

void TouchDeviceManager::AssociateToSingleDisplay(DisplayInfoList* displays,
                                                  DeviceList* devices) {
  // If there is only one display left, then we should associate all input
  // devices with it.
  VLOG(2) << "Trying to match to single display (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  std::size_t num_displays_excluding_internal = displays->size();
  ManagedDisplayInfo* internal_display = GetInternalDisplay(displays);
  if (internal_display)
    num_displays_excluding_internal--;

  // We only associate to one display.
  if (num_displays_excluding_internal != 1 || devices->empty())
    return;

  // Pick the non internal display.
  ManagedDisplayInfo* display = *displays->begin();
  if (display == internal_display)
    display = (*displays)[1];

  for (const ui::TouchscreenDevice& device : *devices) {
    VLOG(2) << "=> Matched device " << device.name << " to display "
            << display->name();
    Associate(display, device);
  }
  devices->clear();
}

void TouchDeviceManager::AssociateAnyRemainingDevices(DisplayInfoList* displays,
                                                      DeviceList* devices) {
  if (!displays->size() || !devices->size())
    return;
  VLOG(2) << "Trying to match remaining " << devices->size()
          << " devices to a display.";

  // Try to match all devices to the internal display.
  ManagedDisplayInfo* display = GetInternalDisplay(displays);
  if (!display) {
    // If no internal displays were found, then associate the devices to any of
    // the other displays.
    display = *(displays->begin());

    VLOG(2) << "Could not find any internal display. Matching all devices to a "
            << "random non internal display.";
  }
  VLOG(2) << "Matching " << devices->size() << " touch devices to "
          << display->name() << "[" << display->id() << "]";

  // device_it is iterated within the loop.
  for (auto device_it = devices->begin(); device_it != devices->end();) {
    // We do not want to associate an internal touch device with anything other
    // than internal display.
    if ((*device_it).type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      device_it++;
      continue;
    }

    VLOG(2) << "=> Matched " << (*device_it).name << " to " << display->name();
    Associate(display, *device_it);
    device_it = devices->erase(device_it);
  }
}

void TouchDeviceManager::Associate(ManagedDisplayInfo* display,
                                   const ui::TouchscreenDevice& device) {
  display->set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
  active_touch_associations_[TouchDeviceIdentifier::FromDevice(device)] =
      display->id();
}

////////////////////////////////////////////////////////////////////////////////
// TouchDeviceManager
// Managing Touch device calibration data

void TouchDeviceManager::AddTouchCalibrationData(
    const TouchDeviceIdentifier& identifier,
    int64_t display_id,
    const TouchCalibrationData& data) {
  if (!base::ContainsKey(touch_associations_, identifier))
    touch_associations_.emplace(identifier, AssociationInfoMap());

  // Update the current touch association and associate the display identified
  // by |display_id| to the touch device identified by |identifier|.
  active_touch_associations_[identifier] = display_id;

  auto it = touch_associations_.at(identifier).find(display_id);
  if (it != touch_associations_.at(identifier).end()) {
    // Update the timestamp and calibration data if information about the
    // display identified by |display_id| already exists for the touch device
    // identified by |identifier|.
    it->second.calibration_data = data;
    it->second.timestamp = base::Time::Now();
  } else {
    // Add a new entry for the display identified by |display_id| in the map
    // of associations for the touch device identified by |identifier|.
    TouchAssociationInfo info;
    info.timestamp = base::Time::Now();
    info.calibration_data = data;
    touch_associations_.at(identifier).emplace(display_id, info);
  }
}

void TouchDeviceManager::ClearTouchCalibrationData(
    const TouchDeviceIdentifier& identifier,
    int64_t display_id) {
  if (base::ContainsKey(touch_associations_, identifier)) {
    ClearCalibrationDataInMap(touch_associations_.at(identifier), display_id);
  }
}

void TouchDeviceManager::ClearAllTouchCalibrationData(int64_t display_id) {
  for (auto it : touch_associations_) {
    // Erase all calibration data from the persistent storage associated with
    // the display identified by |display_id|.
    ClearCalibrationDataInMap(it.second, display_id);
  }
}

TouchCalibrationData TouchDeviceManager::GetCalibrationData(
    const ui::TouchscreenDevice& touchscreen,
    int64_t display_id) const {
  TouchDeviceIdentifier identifier =
      TouchDeviceIdentifier::FromDevice(touchscreen);
  if (display_id == kInvalidDisplayId) {
    // If the touch device is currently not associated with any display and the
    // |display_id| was not provided, then this is an invalid query.
    if (!base::ContainsKey(active_touch_associations_, identifier))
      return TouchCalibrationData();

    // If the display id is not provided, we return the calibration information
    // for the touch device |touchscreen| and the display it is actively
    // associated with.
    display_id = active_touch_associations_.at(identifier);
  }

  if (base::ContainsKey(touch_associations_, identifier)) {
    const AssociationInfoMap& info_map = touch_associations_.at(identifier);
    if (info_map.find(display_id) != info_map.end())
      return info_map.at(display_id).calibration_data;
  }

  // Check for legacy calibration data.
  TouchDeviceIdentifier fallback_identifier(
      TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier());
  if (base::ContainsKey(touch_associations_, fallback_identifier)) {
    const AssociationInfoMap& info_map =
        touch_associations_.at(fallback_identifier);
    if (info_map.find(display_id) != info_map.end())
      return info_map.at(display_id).calibration_data;
  }

  // Return an empty calibration data if none was found.
  return TouchCalibrationData();
}

bool TouchDeviceManager::DisplayHasTouchDevice(
    int64_t display_id,
    const TouchDeviceIdentifier& identifier) const {
  return base::ContainsKey(active_touch_associations_, identifier) &&
         active_touch_associations_.at(identifier) == display_id;
}

int64_t TouchDeviceManager::GetAssociatedDisplay(
    const TouchDeviceIdentifier& identifier) const {
  if (base::ContainsKey(active_touch_associations_, identifier))
    return active_touch_associations_.at(identifier);
  return kInvalidDisplayId;
}

std::vector<TouchDeviceIdentifier>
TouchDeviceManager::GetAssociatedTouchDevicesForDisplay(
    int64_t display_id) const {
  std::vector<TouchDeviceIdentifier> identifiers;
  for (const auto& association : active_touch_associations_) {
    if (association.second == display_id)
      identifiers.push_back(association.first);
  }
  return identifiers;
}

void TouchDeviceManager::RegisterTouchAssociations(
    const TouchAssociationMap& touch_associations) {
  touch_associations_ = touch_associations;
}

std::ostream& operator<<(std::ostream& os,
                         const TouchDeviceIdentifier& identifier) {
  return os << identifier.ToString();
}

bool HasExternalTouchscreenDevice() {
  for (const auto& device :
       ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.type == ui::InputDeviceType::INPUT_DEVICE_EXTERNAL)
      return true;
  }
  return false;
}

bool IsInternalTouchscreenDevice(const TouchDeviceIdentifier& identifier) {
  for (const auto& device :
       ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices()) {
    if (TouchDeviceIdentifier::FromDevice(device) == identifier)
      return device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
  }
  VLOG(1) << "Touch device identified by " << identifier << " is currently"
          << " not connected to the device or is an invalid device.";
  return false;
}

}  // namespace display
