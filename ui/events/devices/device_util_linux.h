// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_UTIL_LINUX_H_
#define UI_EVENTS_DEVICES_DEVICE_UTIL_LINUX_H_

#include "ui/events/devices/events_devices_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {

// Returns true if the device described by |path| is an internal device.
EVENTS_DEVICES_EXPORT bool IsTouchscreenInternal(const base::FilePath& path);

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_UTIL_LINUX_H_
