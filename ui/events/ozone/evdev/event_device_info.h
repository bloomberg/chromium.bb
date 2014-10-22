// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_INFO_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_INFO_H_

#include <limits.h>
#include <linux/input.h>

#include <vector>

#include "base/basictypes.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

// ABS_MT_SLOT isn't valid options for EVIOCGMTSLOTS ioctl.
#define EVDEV_ABS_MT_COUNT (ABS_MAX - ABS_MT_SLOT - 1)

namespace ui {

// Device information for Linux input devices
//
// This stores and queries information about input devices; in
// particular it knows which events the device can generate.
class EVENTS_OZONE_EVDEV_EXPORT EventDeviceInfo {
 public:
  EventDeviceInfo();
  ~EventDeviceInfo();

  // Initialize device information from an open device.
  bool Initialize(int fd);

  // Check events this device can generate.
  bool HasEventType(unsigned int type) const;
  bool HasKeyEvent(unsigned int code) const;
  bool HasRelEvent(unsigned int code) const;
  bool HasAbsEvent(unsigned int code) const;
  bool HasMscEvent(unsigned int code) const;
  bool HasSwEvent(unsigned int code) const;
  bool HasLedEvent(unsigned int code) const;

  // Properties of absolute axes.
  int32 GetAbsMinimum(unsigned int code) const;
  int32 GetAbsMaximum(unsigned int code) const;
  int32 GetSlotValue(unsigned int code, unsigned int slot) const;

  // Check input device properties.
  bool HasProp(unsigned int code) const;

  // Has absolute X & Y axes.
  bool HasAbsXY() const;

  // Has relativeX & Y axes.
  bool HasRelXY() const;

  // Determine whether absolute device X/Y coordinates are mapped onto the
  // screen. This is the case for touchscreens and tablets but not touchpads.
  bool IsMappedToScreen() const;

 private:
  // Return the slot vector in |slot_values_| for |code|.
  const std::vector<int32_t>& GetMtSlotsForCode(int code) const;

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

  DISALLOW_COPY_AND_ASSIGN(EventDeviceInfo);
};

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_INFO_H_
