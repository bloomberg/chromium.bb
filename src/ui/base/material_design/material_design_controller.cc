// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/material_design/material_design_controller.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_CHROMEOS)
#include <fcntl.h>

#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/threading/thread_restrictions.h"
#include "ui/base/touch/touch_device.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/ozone/evdev/event_device_info.h"  // nogncheck
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "ui/base/win/hidden_window.h"
#endif

namespace ui {
namespace {

#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_LINUX)

// Whether Material Refresh should be used by default.
// Material refresh is controlled by both --top-chrome-md and this feature.
// --top-chrome-md should take precedence over what this feature may indicate.
bool IsMaterialRefreshEnabled() {
  static constexpr base::Feature kMaterialRefreshEnabledFeature = {
      "MaterialRefresh", base::FEATURE_ENABLED_BY_DEFAULT};
  return base::FeatureList::IsEnabled(kMaterialRefreshEnabledFeature);
}

#endif

#if defined(OS_CHROMEOS)

// Whether to use touchable UI.
// http://crbug.com/875122 - Disabled by default on ChromeOS except on tablets.
const base::Feature kTouchOptimizedUi = {"TouchOptimizedUi",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

MaterialDesignController::Mode GetDefaultTouchDeviceMode() {
  bool material_refresh_enabled = IsMaterialRefreshEnabled();
  bool touch_optimized_ui_enabled =
      base::FeatureList::IsEnabled(kTouchOptimizedUi);
  if (material_refresh_enabled) {
    return touch_optimized_ui_enabled
               ? MaterialDesignController::MATERIAL_TOUCH_REFRESH
               : MaterialDesignController::MATERIAL_REFRESH;
  }

  return touch_optimized_ui_enabled
             ? MaterialDesignController::MATERIAL_TOUCH_OPTIMIZED
             : MaterialDesignController::MATERIAL_HYBRID;
}

bool HasTouchscreen() {
  // If a scan of available devices has already completed, use that.
  if (DeviceDataManager::HasInstance() &&
      DeviceDataManager::GetInstance()->AreDeviceListsComplete())
    return GetTouchScreensAvailability() == TouchScreensAvailability::ENABLED;

  // Otherwise perform our own scan to determine the presence of a touchscreen.
  // Note this is a one-time call that occurs during device startup or restart.
  base::FileEnumerator file_enum(
      base::FilePath(FILE_PATH_LITERAL("/dev/input")), false,
      base::FileEnumerator::FILES, FILE_PATH_LITERAL("event*[0-9]"));
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next()) {
    EventDeviceInfo devinfo;
    base::ScopedFD fd(
        open(path.value().c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC));
    if (fd.is_valid() && devinfo.Initialize(fd.get(), path) &&
        devinfo.HasTouchscreen())
      return true;
  }

  return false;
}

#endif  // OS_CHROMEOS

}  // namespace

bool MaterialDesignController::is_mode_initialized_ = false;

MaterialDesignController::Mode MaterialDesignController::mode_ =
    MaterialDesignController::MATERIAL_NORMAL;

// static
void MaterialDesignController::Initialize() {
  TRACE_EVENT0("startup", "MaterialDesignController::InitializeMode");
  CHECK(!is_mode_initialized_);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kTopChromeMD);

  bool force_material_refresh = false;
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  force_material_refresh =
      base::FeatureList::IsEnabled(features::kExperimentalUi);
#endif

  if (switch_value == switches::kTopChromeMDMaterialRefresh) {
    SetMode(MATERIAL_REFRESH);
  } else if (switch_value ==
             switches::kTopChromeMDMaterialRefreshTouchOptimized) {
    SetMode(MATERIAL_TOUCH_REFRESH);
  } else if (force_material_refresh) {
    bool has_touchscreen = false;
#if defined(OS_CHROMEOS)
    has_touchscreen = HasTouchscreen();
#endif
    SetMode(has_touchscreen ? MATERIAL_TOUCH_REFRESH : MATERIAL_REFRESH);
  } else if (switch_value == switches::kTopChromeMDMaterial) {
    SetMode(MATERIAL_NORMAL);
  } else if (switch_value == switches::kTopChromeMDMaterialHybrid) {
    SetMode(MATERIAL_HYBRID);
  } else if (switch_value == switches::kTopChromeMDMaterialTouchOptimized) {
    SetMode(MATERIAL_TOUCH_OPTIMIZED);
  } else if (switch_value == switches::kTopChromeMDMaterialAuto) {
#if defined(OS_WIN)
    // TODO(girard): add support for switching between modes when
    // the device switches to "tablet mode".
    if (base::win::IsTabletDevice(nullptr, ui::GetHiddenWindow()))
      SetMode(MATERIAL_HYBRID);
#endif
    SetMode(DefaultMode());
  } else {
    if (!switch_value.empty()) {
      LOG(ERROR) << "Invalid value='" << switch_value
                 << "' for command line switch '" << switches::kTopChromeMD
                 << "'.";
    }
    SetMode(DefaultMode());
  }

  // Ideally, there would be a more general, "initialize random stuff here"
  // function into which these things and a call to this function can be placed.
  // TODO(crbug.com/864544)
  if (IsRefreshUi())
    color_utils::SetDarkestColor(gfx::kGoogleGrey900);

  double animation_duration_scale;
  if (base::StringToDouble(
          command_line->GetSwitchValueASCII(switches::kAnimationDurationScale),
          &animation_duration_scale)) {
    gfx::LinearAnimation::SetDurationScale(animation_duration_scale);
  }
}

// static
MaterialDesignController::Mode MaterialDesignController::GetMode() {
  CHECK(is_mode_initialized_);
  return mode_;
}

// static
bool MaterialDesignController::IsTouchOptimizedUiEnabled() {
  return GetMode() == MATERIAL_TOUCH_OPTIMIZED ||
         GetMode() == MATERIAL_TOUCH_REFRESH;
}

// static
bool MaterialDesignController::IsNewerMaterialUi() {
  return IsTouchOptimizedUiEnabled() || IsRefreshUi();
}

// static
bool MaterialDesignController::IsRefreshUi() {
  return GetMode() == MATERIAL_REFRESH || GetMode() == MATERIAL_TOUCH_REFRESH;
}

// static
MaterialDesignController::Mode MaterialDesignController::DefaultMode() {
#if defined(OS_CHROMEOS)
  // This is called (once) early in device startup to initialize core UI, so
  // the UI thread should be blocked to perform the device query.
  base::ScopedAllowBlocking allow_io;
  if (HasTouchscreen())
    return GetDefaultTouchDeviceMode();

  return IsMaterialRefreshEnabled() ? MATERIAL_REFRESH : MATERIAL_NORMAL;
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_LINUX)
  return IsMaterialRefreshEnabled() ? MATERIAL_REFRESH : MATERIAL_NORMAL;
#elif defined(OS_MACOSX) && BUILDFLAG(MAC_VIEWS_BROWSER)
  return features::IsViewsBrowserCocoa() ? MATERIAL_NORMAL : MATERIAL_REFRESH;
#else
  return MATERIAL_NORMAL;
#endif
}

// static
void MaterialDesignController::Uninitialize() {
  is_mode_initialized_ = false;
}

// static
void MaterialDesignController::SetMode(MaterialDesignController::Mode mode) {
  mode_ = mode;
  is_mode_initialized_ = true;
}

}  // namespace ui
