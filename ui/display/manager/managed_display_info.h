// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_
#define UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_

#include <stdint.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace display {

// A class that represents the display's mode info.
class DISPLAY_MANAGER_EXPORT ManagedDisplayMode {
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
                     float device_scale_factor);

  ~ManagedDisplayMode();
  ManagedDisplayMode(const ManagedDisplayMode& other);
  ManagedDisplayMode& operator=(const ManagedDisplayMode& other);

  // Returns the size in DIP which is visible to the user.
  gfx::Size GetSizeInDIP() const;

  // Returns true if |other| has same size and scale factors.
  bool IsEquivalent(const ManagedDisplayMode& other) const;

  const gfx::Size& size() const { return size_; }
  bool is_interlaced() const { return is_interlaced_; }
  float refresh_rate() const { return refresh_rate_; }

  bool native() const { return native_; }

  // Missing from ui::ManagedDisplayMode
  float device_scale_factor() const { return device_scale_factor_; }

  std::string ToString() const;

 private:
  gfx::Size size_;              // Physical pixel size of the display.
  float refresh_rate_ = 0.0f;   // Refresh rate of the display, in Hz.
  bool is_interlaced_ = false;  // True if mode is interlaced.
  bool native_ = false;         // True if mode is native mode of the display.
  float device_scale_factor_ = 1.0f;  // The device scale factor of the mode.
};

inline bool operator==(const ManagedDisplayMode& lhs,
                       const ManagedDisplayMode& rhs) {
  return lhs.size() == rhs.size() &&
         lhs.is_interlaced() == rhs.is_interlaced() &&
         lhs.refresh_rate() == rhs.refresh_rate() &&
         lhs.native() == rhs.native() &&
         lhs.device_scale_factor() == rhs.device_scale_factor();
}

inline bool operator!=(const ManagedDisplayMode& lhs,
                       const ManagedDisplayMode& rhs) {
  return !(lhs == rhs);
}

// ManagedDisplayInfo contains metadata for each display. This is used to create
// |Display| as well as to maintain extra infomation to manage displays in ash
// environment. This class is intentionally made copiable.
class DISPLAY_MANAGER_EXPORT ManagedDisplayInfo {
 public:
  using ManagedDisplayModeList = std::vector<ManagedDisplayMode>;

  // Creates a ManagedDisplayInfo from string spec. 100+200-1440x800 creates
  // display
  // whose size is 1440x800 at the location (100, 200) in host coordinates.
  // The format is
  //
  // [origin-]widthxheight[*device_scale_factor][#resolutions list]
  //     [/<properties>][@zoom-factor]
  //
  // where [] are optional:
  // - |origin| is given in x+y- format.
  // - |device_scale_factor| is either 2 or 1 (or empty).
  // - properties can combination of 'o', which adds default overscan insets
  //   (5%), and one rotation property where 'r' is 90 degree clock-wise
  //   (to the 'r'ight) 'u' is 180 degrees ('u'pside-down) and 'l' is
  //   270 degrees (to the 'l'eft).
  // - zoom-factor is floating value, e.g. @1.5 or @1.25.
  // - |resolution list| is the list of size that is given in
  //   |width x height [% refresh_rate]| separated by '|'.
  //
  // A couple of examples:
  // "100x100"
  //      100x100 window at 0,0 origin. 1x device scale factor. no overscan.
  //      no rotation. 1.0 zoom factor.
  // "5+5-300x200*2"
  //      300x200 window at 5,5 origin. 2x device scale factor.
  //      no overscan, no rotation. 1.0 zoom factor.
  // "300x200/ol"
  //      300x200 window at 0,0 origin. 1x device scale factor.
  //      with 5% overscan. rotated to left (90 degree counter clockwise).
  //      1.0 zoom factor.
  // "10+20-300x200/u@1.5"
  //      300x200 window at 10,20 origin. 1x device scale factor.
  //      no overscan. flipped upside-down (180 degree) and 1.5 zoom factor.
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

  // Gets/Sets the device scale factor of the display.
  // TODO(oshima): Rename this to |default_device_scale_factor|.
  float device_scale_factor() const { return device_scale_factor_; }
  void set_device_scale_factor(float scale) { device_scale_factor_ = scale; }

  float zoom_factor() const { return zoom_factor_; }
  void set_zoom_factor(float zoom_factor) { zoom_factor_ = zoom_factor; }

  float refresh_rate() const { return refresh_rate_; }
  void set_refresh_rate(float refresh_rate) { refresh_rate_ = refresh_rate; }
  bool is_interlaced() const { return is_interlaced_; }
  void set_is_interlaced(bool is_interlaced) { is_interlaced_ = is_interlaced; }

  // Gets/Sets the device DPI of the display.
  float device_dpi() const { return device_dpi_; }
  void set_device_dpi(float dpi) { device_dpi_ = dpi; }

  PanelOrientation panel_orientation() const { return panel_orientation_; }
  void set_panel_orientation(PanelOrientation panel_orientation) {
    panel_orientation_ = panel_orientation;
  }

  // The native bounds for the display. The size of this can be
  // different from the |size_in_pixel| when overscan insets are set.
  const gfx::Rect& bounds_in_native() const { return bounds_in_native_; }

  // The size for the display in pixels with the rotation taking into
  // account.
  const gfx::Size& size_in_pixel() const { return size_in_pixel_; }

  // The original size for the display in pixel, without rotation, but
  // |panel_orientation_| taking into account.
  gfx::Size GetSizeInPixelWithPanelOrientation() const;

  // The overscan insets for the display in DIP.
  const gfx::Insets& overscan_insets_in_dip() const {
    return overscan_insets_in_dip_;
  }

  // Sets the rotation for the given |source|. Setting a new rotation will also
  // have it become the active rotation.
  void SetRotation(Display::Rotation rotation, Display::RotationSource source);

  // Returns the currently active rotation for this display.
  Display::Rotation GetActiveRotation() const;

  // Returns the currently active rotation for this display with the panel
  // orientation adjustment applied.
  Display::Rotation GetLogicalActiveRotation() const;

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
  // display that chrome sees. This is |device_scale_factor| x |zoom_factor_|.
  // TODO(oshima): Rename to |GetDeviceScaleFactor()|.
  float GetEffectiveDeviceScaleFactor() const;

  // Copy the display info except for fields that can be modified by a user
  // (|rotation_|). |rotation_| is copied when the |another_info| isn't native
  // one.
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

  void set_from_native_platform(bool from_native_platform) {
    from_native_platform_ = from_native_platform;
  }
  bool from_native_platform() const { return from_native_platform_; }

  const ManagedDisplayModeList& display_modes() const { return display_modes_; }
  // Sets the display mode list. The mode list will be sorted for the
  // display.
  void SetManagedDisplayModes(const ManagedDisplayModeList& display_modes);

  // Returns the native mode size. If a native mode is not present, return an
  // empty size.
  gfx::Size GetNativeModeSize() const;

  const gfx::DisplayColorSpaces& display_color_spaces() const {
    return display_color_spaces_;
  }
  void set_display_color_spaces(
      const gfx::DisplayColorSpaces& display_color_spaces) {
    display_color_spaces_ = display_color_spaces;
  }

  uint32_t bits_per_channel() const { return bits_per_channel_; }
  void set_bits_per_channel(uint32_t bits_per_channel) {
    bits_per_channel_ = bits_per_channel;
  }

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

  const std::string& manufacturer_id() const { return manufacturer_id_; }
  void set_manufacturer_id(const std::string& id) { manufacturer_id_ = id; }

  const std::string& product_id() const { return product_id_; }
  void set_product_id(const std::string& id) { product_id_ = id; }

  int32_t year_of_manufacture() const { return year_of_manufacture_; }
  void set_year_of_manufacture(int32_t year) { year_of_manufacture_ = year; }

  // Returns a string representation of the ManagedDisplayInfo, excluding
  // display modes.
  std::string ToString() const;

  // Returns a string representation of the ManagedDisplayInfo, including
  // display modes.
  std::string ToFullString() const;

 private:
  // Return the rotation with the panel orientation applied.
  Display::Rotation GetRotationWithPanelOrientation(
      Display::Rotation rotation) const;

  int64_t id_;
  std::string name_;
  std::string manufacturer_id_;
  std::string product_id_;
  int32_t year_of_manufacture_;
  base::FilePath sys_path_;
  bool has_overscan_;
  std::map<Display::RotationSource, Display::Rotation> rotations_;
  Display::RotationSource active_rotation_source_;
  Display::TouchSupport touch_support_;

  // This specifies the device's pixel density. (For example, a display whose
  // DPI is higher than the threshold is considered to have device_scale_factor
  // = 2.0 on Chrome OS).  This is used by the graphics layer to choose and draw
  // appropriate images and scale layers properly.
  float device_scale_factor_;
  gfx::Rect bounds_in_native_;

  // This specifies the device's DPI.
  float device_dpi_;

  // Orientation of the panel relative to natural device orientation.
  display::PanelOrientation panel_orientation_;

  // The size of the display in use. The size can be different from the size
  // of |bounds_in_native_| if the display has overscan insets and/or rotation.
  gfx::Size size_in_pixel_;
  // TODO(oshima): Change this to store pixel.
  gfx::Insets overscan_insets_in_dip_;

  // The zoom level currently applied to the display. This value is appended
  // multiplicatively to the device scale factor to get the effecting scaling
  // for a display.
  float zoom_factor_;

  // The value of the current display mode refresh rate.
  float refresh_rate_;

  // True if the current display mode is interlaced (i.e. the display's odd
  // and even lines are scanned alternately in two interwoven rasterized lines).
  bool is_interlaced_;

  // True if this comes from native platform (DisplayChangeObserver).
  bool from_native_platform_;

  // True if current mode is native mode of the display.
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

  // Colorimetry information of the Display.
  gfx::DisplayColorSpaces display_color_spaces_;

  // Bit depth of every channel, extracted from its EDID, usually 8, but can be
  // 0 if EDID says so or if the EDID (retrieval) was faulty.
  uint32_t bits_per_channel_;

  // If you add a new member, you need to update Copy().
};

// Resets the synthesized display id for testing. This
// is necessary to avoid overflowing the output index.
void DISPLAY_MANAGER_EXPORT ResetDisplayIdForTest();

// Generates a fake, synthesized display ID that will be used when the
// |kInvalidDisplayId| is passed to |ManagedDisplayInfo| constructor.
int64_t DISPLAY_MANAGER_EXPORT GetNextSynthesizedDisplayId(int64_t id);

}  // namespace display

#endif  //  UI_DISPLAY_MANAGER_MANAGED_DISPLAY_INFO_H_
