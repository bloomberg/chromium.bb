// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_device_info.h"

#include <linux/input.h>

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

#if !defined(EVIOCGMTSLOTS)
#define EVIOCGMTSLOTS(len) _IOC(_IOC_READ, 'E', 0x0a, len)
#endif

namespace ui {

namespace {

bool GetEventBits(int fd, unsigned int type, void* buf, unsigned int size) {
  if (ioctl(fd, EVIOCGBIT(type, size), buf) < 0) {
    DLOG(ERROR) << "failed EVIOCGBIT(" << type << ", " << size << ") on fd "
                << fd;
    return false;
  }

  return true;
}

bool GetPropBits(int fd, void* buf, unsigned int size) {
  if (ioctl(fd, EVIOCGPROP(size), buf) < 0) {
    DLOG(ERROR) << "failed EVIOCGPROP(" << size << ") on fd " << fd;
    return false;
  }

  return true;
}

bool GetAbsInfo(int fd, int code, struct input_absinfo* absinfo) {
  if (ioctl(fd, EVIOCGABS(code), absinfo)) {
    DLOG(ERROR) << "failed EVIOCGABS(" << code << ") on fd " << fd;
    return false;
  }
  return true;
}

// |request| needs to be the equivalent to:
// struct input_mt_request_layout {
//   uint32_t code;
//   int32_t values[num_slots];
// };
//
// |size| is num_slots + 1 (for code).
bool GetSlotValues(int fd, int32_t* request, unsigned int size) {
  if (ioctl(fd,
            EVIOCGMTSLOTS(sizeof(int32_t) * size),
            request) < 0) {
    DLOG(ERROR) << "failed EVIOCGMTSLOTS(" << request[0] << ") on fd " << fd;
    return false;
  }

  return true;
}

}  // namespace

EventDeviceInfo::EventDeviceInfo() {
  memset(ev_bits_, 0, sizeof(ev_bits_));
  memset(key_bits_, 0, sizeof(key_bits_));
  memset(rel_bits_, 0, sizeof(rel_bits_));
  memset(abs_bits_, 0, sizeof(abs_bits_));
  memset(msc_bits_, 0, sizeof(msc_bits_));
  memset(sw_bits_, 0, sizeof(sw_bits_));
  memset(led_bits_, 0, sizeof(led_bits_));
  memset(prop_bits_, 0, sizeof(prop_bits_));
  memset(abs_info_, 0, sizeof(abs_info_));
}

EventDeviceInfo::~EventDeviceInfo() {}

bool EventDeviceInfo::Initialize(int fd) {
  if (!GetEventBits(fd, 0, ev_bits_, sizeof(ev_bits_)))
    return false;

  if (!GetEventBits(fd, EV_KEY, key_bits_, sizeof(key_bits_)))
    return false;

  if (!GetEventBits(fd, EV_REL, rel_bits_, sizeof(rel_bits_)))
    return false;

  if (!GetEventBits(fd, EV_ABS, abs_bits_, sizeof(abs_bits_)))
    return false;

  if (!GetEventBits(fd, EV_MSC, msc_bits_, sizeof(msc_bits_)))
    return false;

  if (!GetEventBits(fd, EV_SW, sw_bits_, sizeof(sw_bits_)))
    return false;

  if (!GetEventBits(fd, EV_LED, led_bits_, sizeof(led_bits_)))
    return false;

  if (!GetPropBits(fd, prop_bits_, sizeof(prop_bits_)))
    return false;

  for (unsigned int i = 0; i < ABS_CNT; ++i)
    if (HasAbsEvent(i))
      if (!GetAbsInfo(fd, i, &abs_info_[i]))
        return false;

  int max_num_slots = abs_info_[ABS_MT_SLOT].maximum + 1;
  // |request| is MT code + slots.
  int32_t request[max_num_slots + 1];
  for (unsigned int i = ABS_MT_SLOT + 1; i < ABS_MAX; ++i) {
    memset(request, 0, sizeof(request));
    request[0] = i;
    if (HasAbsEvent(i))
      if (!GetSlotValues(fd, request, max_num_slots + 1))
        LOG(WARNING) << "Failed to get multitouch values for code " << i;

    slot_values_[i - ABS_MT_SLOT - 1].assign(
        request + 1, request + max_num_slots + 1);
  }

  return true;
}

bool EventDeviceInfo::HasEventType(unsigned int type) const {
  if (type > EV_MAX)
    return false;
  return EvdevBitIsSet(ev_bits_, type);
}

bool EventDeviceInfo::HasKeyEvent(unsigned int code) const {
  if (code > KEY_MAX)
    return false;
  return EvdevBitIsSet(key_bits_, code);
}

bool EventDeviceInfo::HasRelEvent(unsigned int code) const {
  if (code > REL_MAX)
    return false;
  return EvdevBitIsSet(rel_bits_, code);
}

bool EventDeviceInfo::HasAbsEvent(unsigned int code) const {
  if (code > ABS_MAX)
    return false;
  return EvdevBitIsSet(abs_bits_, code);
}

bool EventDeviceInfo::HasMscEvent(unsigned int code) const {
  if (code > MSC_MAX)
    return false;
  return EvdevBitIsSet(msc_bits_, code);
}

bool EventDeviceInfo::HasSwEvent(unsigned int code) const {
  if (code > SW_MAX)
    return false;
  return EvdevBitIsSet(sw_bits_, code);
}

bool EventDeviceInfo::HasLedEvent(unsigned int code) const {
  if (code > LED_MAX)
    return false;
  return EvdevBitIsSet(led_bits_, code);
}

bool EventDeviceInfo::HasProp(unsigned int code) const {
  if (code > INPUT_PROP_MAX)
    return false;
  return EvdevBitIsSet(prop_bits_, code);
}

int32 EventDeviceInfo::GetAbsMinimum(unsigned int code) const {
  return abs_info_[code].minimum;
}

int32 EventDeviceInfo::GetAbsMaximum(unsigned int code) const {
  return abs_info_[code].maximum;
}

int32 EventDeviceInfo::GetSlotValue(unsigned int code,
                                    unsigned int slot) const {
  const std::vector<int32_t>& slots = GetMtSlotsForCode(code);
  DCHECK_LE(0u, slot) << slot << " is an invalid slot";
  DCHECK_LT(slot, slots.size()) << slot << " is an invalid slot";
  return slots[slot];
}

bool EventDeviceInfo::HasAbsXY() const {
  if (HasAbsEvent(ABS_X) && HasAbsEvent(ABS_Y))
    return true;

  if (HasAbsEvent(ABS_MT_POSITION_X) && HasAbsEvent(ABS_MT_POSITION_Y))
    return true;

  return false;
}

bool EventDeviceInfo::HasRelXY() const {
  return HasRelEvent(REL_X) && HasRelEvent(REL_Y);
}

bool EventDeviceInfo::IsMappedToScreen() const {
  // Device position is mapped directly to the screen.
  if (HasProp(INPUT_PROP_DIRECT))
    return true;

  // Device position moves the cursor.
  if (HasProp(INPUT_PROP_POINTER))
    return false;

  // Tablets are mapped to the screen.
  if (HasKeyEvent(BTN_TOOL_PEN) || HasKeyEvent(BTN_STYLUS) ||
      HasKeyEvent(BTN_STYLUS2))
    return true;

  // Touchpads are not mapped to the screen.
  if (HasKeyEvent(BTN_LEFT) || HasKeyEvent(BTN_MIDDLE) ||
      HasKeyEvent(BTN_RIGHT) || HasKeyEvent(BTN_TOOL_FINGER))
    return false;

  // Touchscreens are mapped to the screen.
  return true;
}

const std::vector<int32_t>& EventDeviceInfo::GetMtSlotsForCode(int code) const {
  int index = code - ABS_MT_SLOT - 1;
  DCHECK_LE(0, index) << code << " is not a valid multi-touch code";
  DCHECK_LT(index, EVDEV_ABS_MT_COUNT)
      << code << " is not a valid multi-touch code";
  return slot_values_[index];
}

}  // namespace ui
