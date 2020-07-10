// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_UTIL_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_UTIL_H_

#include <limits.h>

namespace ui {

#define EVDEV_LONG_BITS (CHAR_BIT * sizeof(long))
#define EVDEV_BITS_TO_LONGS(x) (((x) + EVDEV_LONG_BITS - 1) / EVDEV_LONG_BITS)

static inline bool EvdevBitIsSet(const unsigned long* data, int bit) {
  return data[bit / EVDEV_LONG_BITS] & (1UL << (bit % EVDEV_LONG_BITS));
}

static inline void EvdevSetBit(unsigned long* data, int bit) {
  data[bit / EVDEV_LONG_BITS] |= (1UL << (bit % EVDEV_LONG_BITS));
}

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DEVICE_UTIL_H_
