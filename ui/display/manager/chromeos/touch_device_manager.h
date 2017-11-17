// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_DEVICE_MANAGER_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_DEVICE_MANAGER_H_

#include <array>
#include <map>
#include <ostream>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
struct TouchscreenDevice;
}  // namespace ui

namespace display {
class ManagedDisplayInfo;

namespace test {
class TouchDeviceManagerTestApi;
}  // namespace test

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
  struct TouchAssociationInfo {
    // The timestamp at which the most recent touch association was performed.
    base::Time timestamp;

    // The touch calibration data associated with the pairing.
    TouchCalibrationData calibration_data;
  };

  using AssociationInfoMap = std::map<int64_t, TouchAssociationInfo>;
  using TouchAssociationMap =
      std::map<TouchDeviceIdentifier, AssociationInfoMap>;

  TouchDeviceManager();
  ~TouchDeviceManager();

  // Given a list of displays and a list of touchscreens, associate them. The
  // information in |displays| will be updated to reflect which display supports
  // touch. The associations are stored in |active_touch_associations_|.
  void AssociateTouchscreens(
      std::vector<ManagedDisplayInfo>* all_displays,
      const std::vector<ui::TouchscreenDevice>& all_devices);

  // Adds/updates the touch calibration data for touch device identified by
  // |identifier| and display with id |display_id|. This updates the mapping for
  // |active_touch_associations_|.
  void AddTouchCalibrationData(const TouchDeviceIdentifier& identifier,
                               int64_t display_id,
                               const TouchCalibrationData& data);

  // Clears any touch calibration data associated with the pair, touch device
  // identified by |identifier| and display identified by |display_id|.
  // NOTE: This does not disassociate the pair, it only resets the calibration
  // data.
  void ClearTouchCalibrationData(const TouchDeviceIdentifier& identifier,
                                 int64_t display_id);

  // Clears all touch calibration data associated with the display identified
  // by |display_id|.
  // NOTE: This does not disassociate any pairing for display with |display_id|.
  void ClearAllTouchCalibrationData(int64_t display_id);

  // Returns the touch calibration data associated with the display identified
  // by |display_id| and touch device identified by |touchscreen|. If
  // |display_id| is not provided, then the display id of the display currently
  // associated with |touchscreen| is used. Returns an empty object if the
  // calibration data was not found.
  TouchCalibrationData GetCalibrationData(
      const ui::TouchscreenDevice& touchscreen,
      int64_t display_id = kInvalidDisplayId) const;

  // Returns true of the display identified by |display_id| is associated with
  // the touch device identified by |identifier|.
  bool DisplayHasTouchDevice(int64_t display_id,
                             const TouchDeviceIdentifier& identifier) const;

  // Returns the display id of the display that the touch device identified by
  // |identifier| is currently associated with. Returns |kInvalidDisplayId| if
  // no display associated to touch device was found.
  int64_t GetAssociatedDisplay(const TouchDeviceIdentifier& identifier) const;

  // Returns a list of touch devices that are associated with the display with
  // id as |display_id|. This list only includes active associations, that is,
  // the devices that are currently connected to the system and associated with
  // this display.
  std::vector<TouchDeviceIdentifier> GetAssociatedTouchDevicesForDisplay(
      int64_t display_id) const;

  // Registers the touch associations retrieved from the persistent store. This
  // function is used to initialize the TouchDeviceManager on system start up.
  void RegisterTouchAssociations(const TouchAssociationMap& touch_associations);

  const TouchAssociationMap& touch_associations() const {
    return touch_associations_;
  }

 private:
  friend class test::TouchDeviceManagerTestApi;

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

  // A mapping of touch device identifiers to a map of TouchAssociationInfo
  // data. This may contain devices and displays that are not currently
  // connected to the system. This is a history of all calibration and
  // association information for this system.
  TouchAssociationMap touch_associations_;

  // A mapping of touch devices identified by their TouchDeviceIdentifier and
  // display ids they are currently associated with. This map only contains
  // items (displays and touch devices) that are currently active.
  std::map<TouchDeviceIdentifier, int64_t> active_touch_associations_;

  DISALLOW_COPY_AND_ASSIGN(TouchDeviceManager);
};

DISPLAY_MANAGER_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const TouchDeviceIdentifier& identifier);

// Returns true if the device has any external touch devices attached.
DISPLAY_MANAGER_EXPORT bool HasExternalTouchscreenDevice();

// Returns true if the touch device identified by |identifier| is an
// internal touchscreen device.
DISPLAY_MANAGER_EXPORT bool IsInternalTouchscreenDevice(
    const TouchDeviceIdentifier& identifier);
}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_DEVICE_MANAGER_H_
