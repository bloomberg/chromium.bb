// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/material_design/material_design_controller.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "ui/base/win/hidden_window.h"
#endif

namespace ui {

bool MaterialDesignController::is_mode_initialized_ = false;

MaterialDesignController::Mode MaterialDesignController::mode_ =
    MaterialDesignController::MATERIAL_REFRESH;

bool MaterialDesignController::is_refresh_dynamic_ui_ = false;

// static
void MaterialDesignController::Initialize() {
  TRACE_EVENT0("startup", "MaterialDesignController::InitializeMode");
  DCHECK(!is_mode_initialized_);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kTopChromeMD);
  const bool touch =
      switch_value == switches::kTopChromeMDMaterialRefreshTouchOptimized;
  SetMode(touch ? MATERIAL_TOUCH_REFRESH : MATERIAL_REFRESH);

  // ChromeOS switches modes dynamically unless the mode is explicitly forced.
  // Other platforms only switch dynamically if explicitly requested.
  is_refresh_dynamic_ui_ =
#if defined(OS_CHROMEOS)
      !touch && (switch_value != switches::kTopChromeMDMaterialRefresh);
#else
      switch_value == switches::kTopChromeMDMaterialRefreshDynamic;
#endif

  // Ideally, there would be a more general, "initialize random stuff here"
  // function into which these things and a call to this function can be placed.
  // TODO(crbug.com/864544)
  color_utils::SetDarkestColor(gfx::kGoogleGrey900);

  double animation_duration_scale;
  if (base::StringToDouble(
          command_line->GetSwitchValueASCII(switches::kAnimationDurationScale),
          &animation_duration_scale)) {
    gfx::LinearAnimation::SetDurationScale(animation_duration_scale);
  }
}

// static
bool MaterialDesignController::IsTouchOptimizedUiEnabled() {
  DCHECK(is_mode_initialized_);
  return mode_ == MATERIAL_TOUCH_REFRESH;
}

// static
void MaterialDesignController::OnTabletModeToggled(bool enabled) {
  if (is_refresh_dynamic_ui_)
    SetMode(enabled ? MATERIAL_TOUCH_REFRESH : MATERIAL_REFRESH);
}

// static
MaterialDesignController* MaterialDesignController::GetInstance() {
  static base::NoDestructor<MaterialDesignController> instance;
  return instance.get();
}

void MaterialDesignController::AddObserver(
    MaterialDesignControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void MaterialDesignController::RemoveObserver(
    MaterialDesignControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

MaterialDesignController::MaterialDesignController() = default;

// static
void MaterialDesignController::Uninitialize() {
  is_mode_initialized_ = false;
}

// static
void MaterialDesignController::SetMode(MaterialDesignController::Mode mode) {
  if (!is_mode_initialized_ || mode_ != mode) {
    is_mode_initialized_ = true;
    mode_ = mode;
    for (auto& observer : GetInstance()->observers_)
      observer.OnMdModeChanged();
  }
}

}  // namespace ui
