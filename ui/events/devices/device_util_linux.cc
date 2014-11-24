// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_util_linux.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"

namespace ui {

// We consider the touchscreen to be internal if it is an I2c device.
bool IsTouchscreenInternal(const base::FilePath& path) {
  DCHECK(!base::MessageLoopForUI::IsCurrent());
  std::string event_node = path.BaseName().value();
  if (event_node.empty() || !StartsWithASCII(event_node, "event", false))
    return false;

  // Find sysfs device path for this device.
  base::FilePath sysfs_path =
      base::FilePath(FILE_PATH_LITERAL("/sys/class/input"));
  sysfs_path = sysfs_path.Append(path.BaseName());
  sysfs_path = base::MakeAbsoluteFilePath(sysfs_path);

  // Device does not exist.
  if (sysfs_path.empty())
    return false;

  // Check all parent devices. If any of them is the "i2c" subsystem,
  // consider the device internal. Otherwise consider it external.
  const base::FilePath i2c_subsystem(FILE_PATH_LITERAL("/sys/bus/i2c"));
  for (base::FilePath path = sysfs_path; path != base::FilePath("/");
       path = path.DirName()) {
    if (base::MakeAbsoluteFilePath(
          path.Append(FILE_PATH_LITERAL("subsystem"))) == i2c_subsystem)
      return true;
  }

  return false;
}

}  // namespace
