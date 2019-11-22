// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_INFO_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_INFO_H_

#include <limits.h>
#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/event_device_util.h"

#if !defined(ABS_MT_TOOL_Y)
#define ABS_MT_TOOL_Y 0x3d
#endif

// ABS_MT_SLOT isn't valid options for EVIOCGMTSLOTS ioctl.
#define EVDEV_ABS_MT_FIRST ABS_MT_TOUCH_MAJOR
#define EVDEV_ABS_MT_LAST ABS_MT_TOOL_Y
#define EVDEV_ABS_MT_COUNT (EVDEV_ABS_MT_LAST - EVDEV_ABS_MT_FIRST + 1)

namespace base {
class FilePath;
}

namespace ui {

// Input device types.
enum COMPONENT_EXPORT(EVDEV) EventDeviceType {
  DT_KEYBOARD,
  DT_MOUSE,
  DT_TOUCHPAD,
  DT_TOUCHSCREEN,
  DT_MULTITOUCH,
  DT_MULTITOUCH_MOUSE,
  DT_ALL,
};

// Device information for Linux input devices
//
// This stores and queries information about input devices; in
// particular it knows which events the device can generate.
class COMPONENT_EXPORT(EVDEV) EventDeviceInfo {
 public:
  EventDeviceInfo();
  ~EventDeviceInfo();

  // Initialize device information from an open device.
  bool Initialize(int fd, const base::FilePath& path);

  // Manual initialization.
  void SetEventTypes(const unsigned long* ev_bits, size_t len);
  void SetKeyEvents(const unsigned long* key_bits, size_t len);
  void SetRelEvents(const unsigned long* rel_bits, size_t len);
  void SetAbsEvents(const unsigned long* abs_bits, size_t len);
  void SetMscEvents(const unsigned long* msc_bits, size_t len);
  void SetSwEvents(const unsigned long* sw_bits, size_t len);
  void SetLedEvents(const unsigned long* led_bits, size_t len);
  void SetProps(const unsigned long* prop_bits, size_t len);
  void SetAbsInfo(unsigned int code, const input_absinfo& absinfo);
  void SetAbsMtSlots(unsigned int code, const std::vector<int32_t>& values);
  void SetAbsMtSlot(unsigned int code, unsigned int slot, uint32_t value);
  void SetDeviceType(InputDeviceType type);
  void SetId(input_id id);
  void SetName(const std::string& name);

  // Check events this device can generate.
  bool HasEventType(unsigned int type) const;
  bool HasKeyEvent(unsigned int code) const;
  bool HasRelEvent(unsigned int code) const;
  bool HasAbsEvent(unsigned int code) const;
  bool HasMscEvent(unsigned int code) const;
  bool HasSwEvent(unsigned int code) const;
  bool HasLedEvent(unsigned int code) const;

  // Properties of absolute axes.
  int32_t GetAbsMinimum(unsigned int code) const;
  int32_t GetAbsMaximum(unsigned int code) const;
  int32_t GetAbsResolution(unsigned int code) const;
  int32_t GetAbsValue(unsigned int code) const;
  input_absinfo GetAbsInfoByCode(unsigned int code) const;
  uint32_t GetAbsMtSlotCount() const;
  int32_t GetAbsMtSlotValue(unsigned int code, unsigned int slot) const;
  int32_t GetAbsMtSlotValueWithDefault(unsigned int code,
                                       unsigned int slot,
                                       int32_t default_value) const;

  // Device identification.
  const std::string& name() const { return name_; }
  const std::string& phys() const { return phys_; }
  uint16_t bustype() const { return input_id_.bustype; }
  uint16_t vendor_id() const { return input_id_.vendor; }
  uint16_t product_id() const { return input_id_.product; }
  uint16_t version() const { return input_id_.version; }

  // Check input device properties.
  bool HasProp(unsigned int code) const;

  // Has absolute X & Y axes (excludes MT)
  bool HasAbsXY() const;

  // Has MT absolute X & Y events.
  bool HasMTAbsXY() const;

  // Has relative X & Y axes.
  bool HasRelXY() const;

  // Has multitouch protocol "B".
  bool HasMultitouch() const;

  // Determine whether this is a "Direct Touch" device e.g. touchscreen.
  // Corresponds to INPUT_PROP_DIRECT but may be inferred.
  // NB: The Linux documentation says tablets would be direct, but they are
  //     not (and drivers do not actually set INPUT_PROP_DIRECT for them).
  bool HasDirect() const;

  // Determine whether device moves the cursor. This is the case for touchpads
  // and tablets but not touchscreens.
  // Corresponds to INPUT_PROP_POINTER but may be inferred.
  bool HasPointer() const;

  // Has stylus EV_KEY events.
  bool HasStylus() const;

  // Determine whether there's a keyboard on this device.
  bool HasKeyboard() const;

  // Determine whether there's a mouse on this device.
  bool HasMouse() const;

  // Determine whether there's a touchpad on this device.
  bool HasTouchpad() const;

  // Determine whether there's a tablet on this device.
  bool HasTablet() const;

  // Determine whether there's a touchscreen on this device.
  bool HasTouchscreen() const;

  // Determine whether there's a gamepad on this device.
  bool HasGamepad() const;

  // The device type (internal or external.)
  InputDeviceType device_type() const { return device_type_; }

  // Determines InputDeviceType from device identification.
  static InputDeviceType GetInputDeviceTypeFromId(input_id id);

 private:
  enum class LegacyAbsoluteDeviceType {
    TOUCHPAD,
    TOUCHSCREEN,
    TABLET,
    NONE,
  };

  // Probe absolute X & Y axis behavior. This is for legacy drivers that
  // do not tell us what the axes mean.
  LegacyAbsoluteDeviceType ProbeLegacyAbsoluteDevice() const;

  unsigned long ev_bits_[EVDEV_BITS_TO_LONGS(EV_CNT)];
  unsigned long key_bits_[EVDEV_BITS_TO_LONGS(KEY_CNT)];
  unsigned long rel_bits_[EVDEV_BITS_TO_LONGS(REL_CNT)];
  unsigned long abs_bits_[EVDEV_BITS_TO_LONGS(ABS_CNT)];
  unsigned long msc_bits_[EVDEV_BITS_TO_LONGS(MSC_CNT)];
  unsigned long sw_bits_[EVDEV_BITS_TO_LONGS(SW_CNT)];
  unsigned long led_bits_[EVDEV_BITS_TO_LONGS(LED_CNT)];
  unsigned long prop_bits_[EVDEV_BITS_TO_LONGS(INPUT_PROP_CNT)];

  struct input_absinfo abs_info_[ABS_CNT];

  // Store the values for the multi-touch properties for each slot.
  std::vector<int32_t> slot_values_[EVDEV_ABS_MT_COUNT];

  // Device identification.
  std::string name_;
  input_id input_id_ = {};

  // Device evdev physical property containing the output for EVIOCGPHYS that is
  // (supposed to be) stable between reboots and hotplugs.
  std::string phys_;

  // Whether this is an internal or external device.
  InputDeviceType device_type_ = InputDeviceType::INPUT_DEVICE_UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(EventDeviceInfo);
};

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_INFO_H_
