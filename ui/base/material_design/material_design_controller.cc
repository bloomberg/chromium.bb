// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/material_design/material_design_controller.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
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
#include "base/win/windows_version.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#endif

namespace ui {

namespace {

#if defined(OS_WIN)
bool IsTabletMode() {
  return base::win::IsWindows10TabletMode(
      gfx::SingletonHwnd::GetInstance()->hwnd());
}

void TabletModeWatcherWinProc(HWND hwnd,
                              UINT message,
                              WPARAM wparam,
                              LPARAM lparam) {
  if (message == WM_SETTINGCHANGE)
    MaterialDesignController::OnTabletModeToggled(IsTabletMode());
}
#endif  // defined(OS_WIN)

}  // namespace

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
  bool touch =
      switch_value == switches::kTopChromeMDMaterialRefreshTouchOptimized;
  is_refresh_dynamic_ui_ = false;

  // When the mode is not explicitly forced, platforms vary as to the default
  // behavior.
  if (!touch && (switch_value != switches::kTopChromeMDMaterialRefresh)) {
#if defined(OS_CHROMEOS)
    // TabletModeClient's default state is in non-tablet mode.
    is_refresh_dynamic_ui_ = true;
#elif defined(OS_WIN)
    if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
      // Win 10+ uses dynamic mode by default and checks the current tablet mode
      // state to determine whether to start in touch mode.
      is_refresh_dynamic_ui_ = true;
      if (base::MessageLoopForUI::IsCurrent()) {
        MaterialDesignController::GetInstance()->singleton_hwnd_observer_ =
            std::make_unique<gfx::SingletonHwndObserver>(
                base::BindRepeating(TabletModeWatcherWinProc));
        touch = IsTabletMode();
      }
    }
#endif
  }
  SetMode(touch ? MATERIAL_TOUCH_REFRESH : MATERIAL_REFRESH);

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
