// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_DEVICE_MANAGER_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_DEVICE_MANAGER_H_

#include <array>
#include <vector>

#include "base/macros.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
struct TouchscreenDevice;
}  // namespace ui

namespace display {

class ManagedDisplayInfo;

// A unique identifier to identify |ui::TouchscreenDevices|. These identifiers
// are persistent across system restarts.
class DISPLAY_MANAGER_EXPORT TouchDeviceIdentifier {
 public:
  // Returns a touch device identifier used as a default or a fallback option.
  static const TouchDeviceIdentifier& GetFallbackTouchDeviceIdentifier();

  static TouchDeviceIdentifier FromDevice(
      const ui::TouchscreenDevice& touch_device);

  explicit TouchDeviceIdentifier(uint32_t identifier);
  TouchDeviceIdentifier(const TouchDeviceIdentifier& other);
  ~TouchDeviceIdentifier() = default;

  TouchDeviceIdentifier& operator=(TouchDeviceIdentifier other);

  bool operator<(const TouchDeviceIdentifier& other) const;
  bool operator==(const TouchDeviceIdentifier& other) const;
  bool operator!=(const TouchDeviceIdentifier& other) const;

  std::string ToString() const;

 private:
  static uint32_t GenerateIdentifier(std::string name,
                                     uint16_t vendor_id,
                                     uint16_t product_id);
  uint32_t id_;
};

// A struct that represents all the data required for touch calibration for the
// display.
struct DISPLAY_MANAGER_EXPORT TouchCalibrationData {
  // CalibrationPointPair.first -> display point
  // CalibrationPointPair.second -> touch point
  // TODO(malaykeshav): Migrate this to struct.
  using CalibrationPointPair = std::pair<gfx::Point, gfx::Point>;
  using CalibrationPointPairQuad = std::array<CalibrationPointPair, 4>;

  static bool CalibrationPointPairCompare(const CalibrationPointPair& pair_1,
                                          const CalibrationPointPair& pair_2);

  TouchCalibrationData();
  TouchCalibrationData(const CalibrationPointPairQuad& point_pairs,
                       const gfx::Size& bounds);
  TouchCalibrationData(const TouchCalibrationData& calibration_data);

  bool operator==(const TouchCalibrationData& other) const;

  bool IsEmpty() const;

  // Calibration point pairs used during calibration. Each point pair contains a
  // display point and the corresponding touch point.
  CalibrationPointPairQuad point_pairs;

  // Bounds of the touch display when the calibration was performed.
  gfx::Size bounds;
};

// This class is responsible for managing all the touch device associations with
// the display. It also provides an API to set and retrieve touch calibration
// data for a given touch device.
class DISPLAY_MANAGER_EXPORT TouchDeviceManager {
 public:
  TouchDeviceManager();
  ~TouchDeviceManager();

  // Given a list of displays and a list of touchscreens, associate them. The
  // information in |displays| will be updated to reflect which display supports
  // touch.
  void AssociateTouchscreens(
      std::vector<ManagedDisplayInfo>* all_displays,
      const std::vector<ui::TouchscreenDevice>& all_devices);

 private:
  void AssociateInternalDevices(std::vector<ManagedDisplayInfo*>* displays,
                                std::vector<ui::TouchscreenDevice>* devices);

  void AssociateUdlDevices(std::vector<ManagedDisplayInfo*>* displays,
                           std::vector<ui::TouchscreenDevice>* devices);

  void AssociateSameSizeDevices(std::vector<ManagedDisplayInfo*>* displays,
                                std::vector<ui::TouchscreenDevice>* devices);

  void AssociateToSingleDisplay(std::vector<ManagedDisplayInfo*>* displays,
                                std::vector<ui::TouchscreenDevice>* devices);

  void AssociateAnyRemainingDevices(
      std::vector<ManagedDisplayInfo*>* displays,
      std::vector<ui::TouchscreenDevice>* devices);

  void Associate(ManagedDisplayInfo* display,
                 const ui::TouchscreenDevice& device);

  DISALLOW_COPY_AND_ASSIGN(TouchDeviceManager);
};

DISPLAY_MANAGER_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const TouchDeviceIdentifier& identifier);

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_DEVICE_MANAGER_H_
