// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_
#define UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_

#include <stdint.h>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct TouchscreenDevice;
}  // namespace ui

namespace display {

// A struct that represents all the data required for touch calibration for the
// display.
struct DISPLAY_MANAGER_EXPORT TouchCalibrationData {
  // CalibrationPointPair.first -> display point
  // CalibrationPointPair.second -> touch point
  using CalibrationPointPair = std::pair<gfx::Point, gfx::Point>;
  using CalibrationPointPairQuad = std::array<CalibrationPointPair, 4>;
  TouchCalibrationData();
  TouchCalibrationData(const CalibrationPointPairQuad& point_pairs,
                       const gfx::Size& bounds);
  TouchCalibrationData(const TouchCalibrationData& calibration_data);

  static bool CalibrationPointPairCompare(const CalibrationPointPair& pair_1,
                                          const CalibrationPointPair& pair_2) {
    return pair_1.first.y() < pair_2.first.y()
               ? true
               : pair_1.first.x() < pair_2.first.x();
  }

  // Returns a hash that can be used as a key for storing display preferences
  // for a display associated with a touch device.
  static uint32_t GenerateTouchDeviceIdentifier(
      const ui::TouchscreenDevice& device);

  // Returns a touch device identifier used as a default or a fallback option.
  static uint32_t GetFallbackTouchDeviceIdentifier();

  bool operator==(TouchCalibrationData other) const;

  // Calibration point pairs used during calibration. Each point pair contains a
  // display point and the corresponding touch point.
  CalibrationPointPairQuad point_pairs;

  // Bounds of the touch display when the calibration was performed.
  gfx::Size bounds;
};

// A class that represents the display's mode info.
class DISPLAY_MANAGER_EXPORT ManagedDisplayMode
    : public base::RefCounted<ManagedDisplayMode> {
 public:
  ManagedDisplayMode();

  explicit ManagedDisplayMode(const gfx::Size& size);

  ManagedDisplayMode(const gfx::Size& size,
                     float refresh_rate,
                     bool is_interlaced,
                     bool native);

  ManagedDisplayMode(const gfx::Size& size,
                     float refresh_rate,
                     bool is_interlaced,
                     bool native,
                     float ui_scale,
                     float device_scale_factor);
  // Returns the size in DIP which is visible to the user.
  gfx::Size GetSizeInDIP(bool is_internal) const;

  // Returns true if |other| has same size and scale factors.
  bool IsEquivalent(const scoped_refptr<ManagedDisplayMode>& other) const;

  const gfx::Size& size() const { return size_; }
  bool is_interlaced() const { return is_interlaced_; }
  float refresh_rate() const { return refresh_rate_; }

  bool native() const { return native_; }

  bool is_default() const { return is_default_; }
  void set_is_default(bool is_default) { is_default_ = is_default; }

  // Missing from ui::ManagedDisplayMode
  float ui_scale() const { return ui_scale_; }
  float device_scale_factor() const { return device_scale_factor_; }

 private:
  ~ManagedDisplayMode();
  friend class base::RefCounted<ManagedDisplayMode>;

  gfx::Size size_;             // Physical pixel size of the display.
  float refresh_rate_;         // Refresh rate of the display, in Hz.
  bool is_interlaced_;         // True if mode is interlaced.
  bool native_;                // True if mode is native mode of the display.
  bool is_default_ = false;    // True if mode is one with default UI scale.
  float ui_scale_;             // The UI scale factor of the mode.
  float device_scale_factor_;  // The device scale factor of the mode.

  DISALLOW_COPY_AND_ASSIGN(ManagedDisplayMode);
};

// ManagedDisplayInfo contains metadata for each display. This is used to create
// |Display| as well as to maintain extra infomation to manage displays in ash
// environment. This class is intentionally made copiable.
class DISPLAY_MANAGER_EXPORT ManagedDisplayInfo {
 public:
  using ManagedDisplayModeList = std::vector<scoped_refptr<ManagedDisplayMode>>;

  // Creates a ManagedDisplayInfo from string spec. 100+200-1440x800 creates
  // display
  // whose size is 1440x800 at the location (100, 200) in host coordinates.
  // The format is
  //
  // [origin-]widthxheight[*device_scale_factor][#resolutions list]
  //     [/<properties>][@ui-scale]
  //
  // where [] are optional:
  // - |origin| is given in x+y- format.
  // - |device_scale_factor| is either 2 or 1 (or empty).
  // - properties can combination of 'o', which adds default overscan insets
  //   (5%), and one rotation property where 'r' is 90 degree clock-wise
  //   (to the 'r'ight) 'u' is 180 degrees ('u'pside-down) and 'l' is
  //   270 degrees (to the 'l'eft).
  // - ui-scale is floating value, e.g. @1.5 or @1.25.
  // - |resolution list| is the list of size that is given in
  //   |width x height [% refresh_rate]| separated by '|'.
  //
  // A couple of examples:
  // "100x100"
  //      100x100 window at 0,0 origin. 1x device scale factor. no overscan.
  //      no rotation. 1.0 ui scale.
  // "5+5-300x200*2"
  //      300x200 window at 5,5 origin. 2x device scale factor.
  //      no overscan, no rotation. 1.0 ui scale.
  // "300x200/ol"
  //      300x200 window at 0,0 origin. 1x device scale factor.
  //      with 5% overscan. rotated to left (90 degree counter clockwise).
  //      1.0 ui scale.
  // "10+20-300x200/u@1.5"
  //      300x200 window at 10,20 origin. 1x device scale factor.
  //      no overscan. flipped upside-down (180 degree) and 1.5 ui scale.
  // "200x100#300x200|200x100%59.0|100x100%60"
  //      200x100 window at 0,0 origin, with 3 possible resolutions,
  //      300x200, 200x100 at 59 Hz, and 100x100 at 60 Hz.
  static ManagedDisplayInfo CreateFromSpec(const std::string& spec);

  // Creates a ManagedDisplayInfo from string spec using given |id|.
  static ManagedDisplayInfo CreateFromSpecWithID(const std::string& spec,
                                                 int64_t id);

  ManagedDisplayInfo();
  ManagedDisplayInfo(int64_t id, const std::string& name, bool has_overscan);
  ManagedDisplayInfo(const ManagedDisplayInfo& other);
  ~ManagedDisplayInfo();

  int64_t id() const { return id_; }

  // The name of the display.
  const std::string& name() const { return name_; }

  // The path to the display device in the sysfs filesystem.
  void set_sys_path(const base::FilePath& sys_path) { sys_path_ = sys_path; }
  const base::FilePath& sys_path() const { return sys_path_; }

  // True if the display EDID has the overscan flag. This does not create the
  // actual overscan automatically, but used in the message.
  bool has_overscan() const { return has_overscan_; }

  void set_touch_support(Display::TouchSupport support) {
    touch_support_ = support;
  }
  Display::TouchSupport touch_support() const { return touch_support_; }

  // Associate the touch device with identifier |touch_device_identifier| to
  // this display.
  void AddTouchDevice(uint32_t touch_device_identifier);

  // Clear the list of touch devices associated with this display.
  void ClearTouchDevices();

  // Returns true if the touch device with identifer |touch_device_identifier|
  // is associated with this display.
  bool HasTouchDevice(uint32_t touch_device_identifier) const;

  // The identifiers of touch devices that are associated with this display.
  std::set<uint32_t> touch_device_identifiers() const {
    return touch_device_identifiers_;
  }

  // Gets/Sets the device scale factor of the display.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  // Gets/Sets the device DPI of the display.
  float device_dpi() const { return device_dpi_; }
  void set_device_dpi(float dpi) { device_dpi_ = dpi; }

  // The native bounds for the display. The size of this can be
  // different from the |size_in_pixel| when overscan insets are set
  // and/or |configured_ui_scale_| is set.
  const gfx::Rect& bounds_in_native() const { return bounds_in_native_; }

  // The size for the display in pixels.
  const gfx::Size& size_in_pixel() const { return size_in_pixel_; }

  // The overscan insets for the display in DIP.
  const gfx::Insets& overscan_insets_in_dip() const {
    return overscan_insets_in_dip_;
  }

  // Sets/gets configured ui scale. This can be different from the ui
  // scale actually used when the scale is 2.0 and DSF is 2.0.
  // (the effective ui scale is 1.0 in this case).
  float configured_ui_scale() const { return configured_ui_scale_; }
  void set_configured_ui_scale(float scale) { configured_ui_scale_ = scale; }

  // Sets the rotation for the given |source|. Setting a new rotation will also
  // have it become the active rotation.
  void SetRotation(Display::Rotation rotation, Display::RotationSource source);

  // Returns the currently active rotation for this display.
  Display::Rotation GetActiveRotation() const;

  // Returns the source which set the active rotation for this display.
  Display::RotationSource active_rotation_source() const {
    return active_rotation_source_;
  }

  // Returns the rotation set by a given |source|.
  Display::Rotation GetRotation(Display::RotationSource source) const;

  // Returns a measure of density relative to a display with 1.0 DSF. Unlike the
  // effective DSF, this is independent from the UI scale.
  float GetDensityRatio() const;

  // Returns the ui scale and device scale factor actually used to create
  // display that chrome sees. This can be different from one obtained
  // from dispaly or one specified by a user in following situation.
  // 1) DSF is 2.0f and UI scale is 2.0f. (Returns 1.0f and 1.0f respectiely)
  // 2) A user specified 0.8x on the device that has 1.25 DSF. 1.25 DSF device
  //    uses 1.0f DFS unless 0.8x UI scaling is specified.
  float GetEffectiveDeviceScaleFactor() const;

  // Returns the ui scale used for the device scale factor. This
  // return 1.0f if the ui scale and dsf are both set to 2.0.
  float GetEffectiveUIScale() const;

  // Copy the display info except for fields that can be modified by a
  // user (|rotation_| and |configured_ui_scale_|). |rotation_| and
  // |configured_ui_scale_| are copied when the |another_info| isn't native one.
  void Copy(const ManagedDisplayInfo& another_info);

  // Update the |bounds_in_native_| and |size_in_pixel_| using
  // given |bounds_in_native|.
  void SetBounds(const gfx::Rect& bounds_in_native);

  // Update the |bounds_in_native| according to the current overscan
  // and rotation settings.
  void UpdateDisplaySize();

  // Sets/Clears the overscan insets.
  void SetOverscanInsets(const gfx::Insets& insets_in_dip);
  gfx::Insets GetOverscanInsetsInPixel() const;

  // Sets/Gets the flag to clear overscan insets.
  bool clear_overscan_insets() const { return clear_overscan_insets_; }
  void set_clear_overscan_insets(bool clear) { clear_overscan_insets_ = clear; }

  void set_native(bool native) { native_ = native; }
  bool native() const { return native_; }

  const ManagedDisplayModeList& display_modes() const { return display_modes_; }
  // Sets the display mode list. The mode list will be sorted for the
  // display.
  void SetManagedDisplayModes(const ManagedDisplayModeList& display_modes);

  void SetTouchCalibrationData(uint32_t touch_device_identifier,
                               const TouchCalibrationData& calibration_data);
  const TouchCalibrationData& GetTouchCalibrationData(
      uint32_t touch_device_identifier) const;
  const std::map<uint32_t, TouchCalibrationData>& touch_calibration_data_map()
      const {
    return touch_calibration_data_map_;
  }
  void SetTouchCalibrationDataMap(
      const std::map<uint32_t, TouchCalibrationData>& data_map);
  bool HasTouchCalibrationData(uint32_t touch_device_identifier) const;
  void ClearTouchCalibrationData(uint32_t touch_device_identifier);
  void ClearAllTouchCalibrationData();

  // Returns the native mode size. If a native mode is not present, return an
  // empty size.
  gfx::Size GetNativeModeSize() const;

  bool is_aspect_preserving_scaling() const {
    return is_aspect_preserving_scaling_;
  }

  void set_is_aspect_preserving_scaling(bool value) {
    is_aspect_preserving_scaling_ = value;
  }

  // Maximum cursor size in native pixels.
  const gfx::Size& maximum_cursor_size() const { return maximum_cursor_size_; }
  void set_maximum_cursor_size(const gfx::Size& size) {
    maximum_cursor_size_ = size;
  }

  // Returns a string representation of the ManagedDisplayInfo, excluding
  // display
  // modes.
  std::string ToString() const;

  // Returns a string representation of the ManagedDisplayInfo, including
  // display
  // modes.
  std::string ToFullString() const;

 private:
  // Returns true if this display should use DSF=1.25 for UI scaling; i.e.
  // SetUse125DSFForUIScaling(true) is called and this is the internal display.
  bool Use125DSFForUIScaling() const;

  int64_t id_;
  std::string name_;
  base::FilePath sys_path_;
  bool has_overscan_;
  std::map<Display::RotationSource, Display::Rotation> rotations_;
  Display::RotationSource active_rotation_source_;
  Display::TouchSupport touch_support_;

  // Identifiers for touch devices that are associated with this display.
  std::set<uint32_t> touch_device_identifiers_;

  // This specifies the device's pixel density. (For example, a
  // display whose DPI is higher than the threshold is considered to have
  // device_scale_factor = 2.0 on Chrome OS).  This is used by the
  // grapics layer to choose and draw appropriate images and scale
  // layers properly.
  float device_scale_factor_;
  gfx::Rect bounds_in_native_;

  // This specifies the device's DPI.
  float device_dpi_;

  // The size of the display in use. The size can be different from the size
  // of |bounds_in_native_| if the display has overscan insets and/or rotation.
  gfx::Size size_in_pixel_;
  gfx::Insets overscan_insets_in_dip_;

  // The pixel scale of the display. This is used to simply expand (or
  // shrink) the desktop over the native display resolution (useful in
  // HighDPI display).  Note that this should not be confused with the
  // device scale factor, which specifies the pixel density of the
  // display. The actuall scale value to be used depends on the device
  // scale factor.  See |GetEffectiveScaleFactor()|.
  float configured_ui_scale_;

  // True if this comes from native platform (DisplayChangeObserver).
  bool native_;

  // True if the display is configured to preserve the aspect ratio. When the
  // display is configured in a non-native mode, only parts of the display will
  // be used such that the aspect ratio is preserved.
  bool is_aspect_preserving_scaling_;

  // True if the displays' overscan inset should be cleared. This is
  // to distinguish the empty overscan insets from native display info.
  bool clear_overscan_insets_;

  // The list of modes supported by this display.
  ManagedDisplayModeList display_modes_;

  // Maximum cursor size.
  gfx::Size maximum_cursor_size_;

  // Information associated to touch calibration for the display. Stores a
  // mapping of each touch device, identified by its unique touch device
  // identifier, with the touch calibration data associated with the display.
  std::map<uint32_t, TouchCalibrationData> touch_calibration_data_map_;

  // If you add a new member, you need to update Copy().
};

// Resets the synthesized display id for testing. This
// is necessary to avoid overflowing the output index.
void DISPLAY_MANAGER_EXPORT ResetDisplayIdForTest();

}  // namespace display

#endif  //  UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_
