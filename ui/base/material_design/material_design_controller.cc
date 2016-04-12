// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_CHROMEOS)
#include "ui/base/touch/touch_device.h"
#include "ui/events/devices/device_data_manager.h"

#if defined(USE_OZONE)
#include <fcntl.h>

#include "base/files/file_enumerator.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#endif  // defined(USE_OZONE)

#endif  // defined(OS_CHROMEOS)

namespace ui {

bool MaterialDesignController::is_mode_initialized_ = false;

MaterialDesignController::Mode MaterialDesignController::mode_ =
    MaterialDesignController::Mode::NON_MATERIAL;

MaterialDesignController::Mode MaterialDesignController::GetMode() {
  if (!is_mode_initialized_)
    InitializeMode();
  CHECK(is_mode_initialized_);
  return mode_;
}

bool MaterialDesignController::IsModeMaterial() {
  return GetMode() == Mode::MATERIAL_NORMAL ||
         GetMode() == Mode::MATERIAL_HYBRID;
}

MaterialDesignController::Mode MaterialDesignController::DefaultMode() {
#if defined(OS_CHROMEOS)
  if (DeviceDataManager::HasInstance() &&
      DeviceDataManager::GetInstance()->device_lists_complete()) {
    return GetTouchScreensAvailability() == TouchScreensAvailability::ENABLED ?
        Mode::MATERIAL_HYBRID : Mode::MATERIAL_NORMAL;
  }

#if defined(USE_OZONE)
  base::FileEnumerator file_enum(
      base::FilePath(FILE_PATH_LITERAL("/dev/input")), false,
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("event*[0-9]"));
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    EventDeviceInfo devinfo;
    int fd = open(path.value().c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd >= 0) {
      if (devinfo.Initialize(fd, path) && devinfo.HasTouchscreen()) {
        close(fd);
        return Mode::MATERIAL_HYBRID;
      }
      close(fd);
    }
  }
#endif  // defined(USE_OZONE)
  return Mode::MATERIAL_NORMAL;
#elif defined(OS_LINUX)
  return Mode::MATERIAL_NORMAL;
#else
  return Mode::NON_MATERIAL;
#endif  // defined(OS_CHROMEOS)
}

void MaterialDesignController::InitializeMode() {
#if !defined(ENABLE_TOPCHROME_MD)
  SetMode(Mode::NON_MATERIAL);
#else
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTopChromeMD);

  if (switch_value == switches::kTopChromeMDMaterial) {
    SetMode(Mode::MATERIAL_NORMAL);
  } else if (switch_value == switches::kTopChromeMDMaterialHybrid) {
    SetMode(Mode::MATERIAL_HYBRID);
  } else if (switch_value == switches::kTopChromeMDNonMaterial) {
    SetMode(Mode::NON_MATERIAL);
  } else {
    if (!switch_value.empty()) {
      LOG(ERROR) << "Invalid value='" << switch_value
                 << "' for command line switch '" << switches::kTopChromeMD
                 << "'.";
    }
    SetMode(DefaultMode());
  }
#endif  // !defined(ENABLE_TOPCHROME_MD)
}

void MaterialDesignController::UninitializeMode() {
  MaterialDesignController::SetMode(
      MaterialDesignController::Mode::NON_MATERIAL);
  is_mode_initialized_ = false;
}

void MaterialDesignController::SetMode(MaterialDesignController::Mode mode) {
  mode_ = mode;
  is_mode_initialized_ = true;
}

}  // namespace ui
