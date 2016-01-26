// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_switches.h"

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
