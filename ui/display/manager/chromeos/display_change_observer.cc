// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/display_change_observer.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/chromeos/touchscreen_util.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/strings/grit/ui_strings.h"

namespace display {
namespace {

// The DPI threshold to determine the device scale factor.
// DPI higher than |dpi| will use |device_scale_factor|.
struct DeviceScaleFactorDPIThreshold {
  float dpi;
  float device_scale_factor;
};

const DeviceScaleFactorDPIThreshold kThresholdTableForInternal[] = {
    {220.0f, 2.0f},
    {200.0f, 1.6f},
    {150.0f, 1.25f},
    {0.0f, 1.0f},
};

// 1 inch in mm.
const float kInchInMm = 25.4f;

// The minimum pixel width whose monitor can be called as '4K'.
const int kMinimumWidthFor4K = 3840;

// The list of device scale factors (in addition to 1.0f) which is
// available in external large monitors.
const float kAdditionalDeviceScaleFactorsFor4k[] = {1.25f, 2.0f};

}  // namespace

// static
ManagedDisplayInfo::ManagedDisplayModeList
DisplayChangeObserver::GetInternalManagedDisplayModeList(
    const ManagedDisplayInfo& display_info,
    const DisplaySnapshot& output) {
  const DisplayMode* ui_native_mode = output.native_mode();
  scoped_refptr<ManagedDisplayMode> native_mode = new ManagedDisplayMode(
      ui_native_mode->size(), ui_native_mode->refresh_rate(),
      ui_native_mode->is_interlaced(), true, 1.0,
      display_info.device_scale_factor());

  return CreateInternalManagedDisplayModeList(native_mode);
}

// static
ManagedDisplayInfo::ManagedDisplayModeList
DisplayChangeObserver::GetExternalManagedDisplayModeList(
    const DisplaySnapshot& output) {
  using DisplayModeMap =
      std::map<std::pair<int, int>, scoped_refptr<ManagedDisplayMode>>;
  DisplayModeMap display_mode_map;

  scoped_refptr<ManagedDisplayMode> native_mode = new ManagedDisplayMode();
  for (const auto& mode_info : output.modes()) {
    const std::pair<int, int> size(mode_info->size().width(),
                                   mode_info->size().height());
    scoped_refptr<ManagedDisplayMode> display_mode = new ManagedDisplayMode(
        mode_info->size(), mode_info->refresh_rate(),
        mode_info->is_interlaced(), output.native_mode() == mode_info.get(),
        1.0, 1.0);
    if (display_mode->native())
      native_mode = display_mode;

    // Add the display mode if it isn't already present and override interlaced
    // display modes with non-interlaced ones.
    auto display_mode_it = display_mode_map.find(size);
    if (display_mode_it == display_mode_map.end())
      display_mode_map.insert(std::make_pair(size, display_mode));
    else if (display_mode_it->second->is_interlaced() &&
             !display_mode->is_interlaced())
      display_mode_it->second = std::move(display_mode);
  }

  ManagedDisplayInfo::ManagedDisplayModeList display_mode_list;
  for (const auto& display_mode_pair : display_mode_map)
    display_mode_list.push_back(std::move(display_mode_pair.second));

  if (output.native_mode()) {
    const std::pair<int, int> size(native_mode->size().width(),
                                   native_mode->size().height());
    auto it = display_mode_map.find(size);
    DCHECK(it != display_mode_map.end())
        << "Native mode must be part of the mode list.";

    // If the native mode was replaced re-add it.
    if (!it->second->native())
      display_mode_list.push_back(native_mode);
  }

  if (native_mode->size().width() >= kMinimumWidthFor4K) {
    for (size_t i = 0; i < arraysize(kAdditionalDeviceScaleFactorsFor4k); ++i) {
      scoped_refptr<ManagedDisplayMode> mode = new ManagedDisplayMode(
          native_mode->size(), native_mode->refresh_rate(),
          native_mode->is_interlaced(), false /* native */,
          native_mode->ui_scale(), kAdditionalDeviceScaleFactorsFor4k[i]);
      display_mode_list.push_back(mode);
    }
  }

  return display_mode_list;
}

DisplayChangeObserver::DisplayChangeObserver(
    DisplayConfigurator* display_configurator,
    DisplayManager* display_manager)
    : display_configurator_(display_configurator),
      display_manager_(display_manager) {
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

DisplayChangeObserver::~DisplayChangeObserver() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
}

MultipleDisplayState DisplayChangeObserver::GetStateForDisplayIds(
    const DisplayConfigurator::DisplayStateList& display_states) {
  UpdateInternalDisplay(display_states);
  if (display_states.size() == 1)
    return MULTIPLE_DISPLAY_STATE_SINGLE;
  DisplayIdList list =
      GenerateDisplayIdList(display_states.begin(), display_states.end(),
                            [](const DisplaySnapshot* display_state) {
                              return display_state->display_id();
                            });
  bool mirrored = display_manager_->layout_store()->GetMirrorMode(list);
  return mirrored ? MULTIPLE_DISPLAY_STATE_DUAL_MIRROR
                  : MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
}

bool DisplayChangeObserver::GetResolutionForDisplayId(int64_t display_id,
                                                      gfx::Size* size) const {
  scoped_refptr<ManagedDisplayMode> mode =
      display_manager_->GetSelectedModeForDisplayId(display_id);
  if (!mode)
    return false;
  *size = mode->size();
  return true;
}

void DisplayChangeObserver::OnDisplayModeChanged(
    const DisplayConfigurator::DisplayStateList& display_states) {
  UpdateInternalDisplay(display_states);

  std::vector<ManagedDisplayInfo> displays;
  for (const DisplaySnapshot* state : display_states) {
    const DisplayMode* mode_info = state->current_mode();
    if (!mode_info)
      continue;

    displays.emplace_back(CreateManagedDisplayInfo(state, mode_info));
  }

  AssociateTouchscreens(
      &displays,
      ui::InputDeviceManager::GetInstance()->GetTouchscreenDevices());
  display_manager_->OnNativeDisplaysChanged(displays);

  // For the purposes of user activity detection, ignore synthetic mouse events
  // that are triggered by screen resizes: http://crbug.com/360634
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (user_activity_detector)
    user_activity_detector->OnDisplayPowerChanging();
}

void DisplayChangeObserver::OnDisplayModeChangeFailed(
    const DisplayConfigurator::DisplayStateList& displays,
    MultipleDisplayState failed_new_state) {
  // If display configuration failed during startup, simply update the display
  // manager with detected displays. If no display is detected, it will
  // create a pseudo display.
  if (display_manager_->GetNumDisplays() == 0)
    OnDisplayModeChanged(displays);
}

void DisplayChangeObserver::OnTouchscreenDeviceConfigurationChanged() {
  // If there are no cached display snapshots, either there are no attached
  // displays or the cached snapshots have been invalidated. For the first case
  // there aren't any touchscreens to associate. For the second case, the
  // displays and touch input-devices will get associated when display
  // configuration finishes.
  const auto& cached_displays = display_configurator_->cached_displays();
  if (!cached_displays.empty())
    OnDisplayModeChanged(cached_displays);
}

void DisplayChangeObserver::UpdateInternalDisplay(
    const DisplayConfigurator::DisplayStateList& display_states) {
  for (auto* state : display_states) {
    if (state->type() == DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (Display::HasInternalDisplay())
        DCHECK_EQ(Display::InternalDisplayId(), state->display_id());
      Display::SetInternalDisplayId(state->display_id());

      if (state->native_mode()) {
        ManagedDisplayInfo new_info =
            CreateManagedDisplayInfo(state, state->native_mode());
        display_manager_->UpdateInternalDisplay(new_info);
      }
    }
  }
}

ManagedDisplayInfo DisplayChangeObserver::CreateManagedDisplayInfo(
    const DisplaySnapshot* state,
    const DisplayMode* mode_info) {
  float device_scale_factor = 1.0f;
  // Sets dpi only if the screen size is not blacklisted.
  float dpi = IsDisplaySizeBlackListed(state->physical_size())
                  ? 0
                  : kInchInMm * mode_info->size().width() /
                        state->physical_size().width();

  if (state->type() == DISPLAY_CONNECTION_TYPE_INTERNAL) {
    if (dpi)
      device_scale_factor = FindDeviceScaleFactor(dpi);
  } else {
    scoped_refptr<ManagedDisplayMode> mode =
        display_manager_->GetSelectedModeForDisplayId(state->display_id());
    if (mode) {
      device_scale_factor = mode->device_scale_factor();
    } else {
      // For monitors that are 40 inches and 4K or above, set
      // |device_scale_factor| to 2x. For margin purposes, 100 is subtracted
      // from the value of |k2xThreshouldSizeSquaredFor4KInMm|
      const int k2xThreshouldSizeSquaredFor4KInMm =
          (40 * 40 * kInchInMm * kInchInMm) - 100;
      gfx::Vector2d size_in_vec(state->physical_size().width(),
                                state->physical_size().height());
      if (size_in_vec.LengthSquared() > k2xThreshouldSizeSquaredFor4KInMm &&
          mode_info->size().width() >= kMinimumWidthFor4K) {
        // Make sure that additional device scale factors table has 2x.
        DCHECK_EQ(2.0f, kAdditionalDeviceScaleFactorsFor4k[1]);
        device_scale_factor = 2.0f;
      }
    }
  }

  std::string name = (state->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
                         ? l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_INTERNAL)
                         : state->display_name();

  if (name.empty())
    name = l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_UNKNOWN);

  const bool has_overscan = state->has_overscan();
  const int64_t id = state->display_id();

  ManagedDisplayInfo new_info = ManagedDisplayInfo(id, name, has_overscan);
  new_info.set_sys_path(state->sys_path());
  new_info.set_device_scale_factor(device_scale_factor);
  const gfx::Rect display_bounds(state->origin(), mode_info->size());
  new_info.SetBounds(display_bounds);
  new_info.set_native(true);
  new_info.set_is_aspect_preserving_scaling(
      state->is_aspect_preserving_scaling());
  if (dpi)
    new_info.set_device_dpi(dpi);

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      (state->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
          ? GetInternalManagedDisplayModeList(new_info, *state)
          : GetExternalManagedDisplayModeList(*state);
  new_info.SetManagedDisplayModes(display_modes);

  new_info.set_maximum_cursor_size(state->maximum_cursor_size());
  return new_info;
}

// static
float DisplayChangeObserver::FindDeviceScaleFactor(float dpi) {
  for (size_t i = 0; i < arraysize(kThresholdTableForInternal); ++i) {
    if (dpi > kThresholdTableForInternal[i].dpi)
      return kThresholdTableForInternal[i].device_scale_factor;
  }
  return 1.0f;
}

}  // namespace display
