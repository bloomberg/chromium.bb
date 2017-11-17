// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/system/devicemode.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace display {

namespace {

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

struct DisplaySortFunctor {
  bool operator()(const Display& a, const Display& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const ManagedDisplayInfo& a, const ManagedDisplayInfo& b) {
    return CompareDisplayIds(a.id(), b.id());
  }
};

Display& GetInvalidDisplay() {
  static Display* invalid_display = new Display();
  return *invalid_display;
}

ManagedDisplayInfo::ManagedDisplayModeList::const_iterator FindDisplayMode(
    const ManagedDisplayInfo& info,
    const ManagedDisplayMode& target_mode) {
  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  return std::find_if(modes.begin(), modes.end(),
                      [target_mode](const ManagedDisplayMode& mode) {
                        return target_mode.IsEquivalent(mode);
                      });
}

void SetInternalManagedDisplayModeList(ManagedDisplayInfo* info) {
  ManagedDisplayMode native_mode(info->bounds_in_native().size(),
                                 0.0 /* refresh_rate */, false /* interlaced */,
                                 false /* native_mode */, 1.0 /* ui_scale */,
                                 info->device_scale_factor());
  info->SetManagedDisplayModes(
      CreateInternalManagedDisplayModeList(native_mode));
}

void MaybeInitInternalDisplay(ManagedDisplayInfo* info) {
  int64_t id = info->id();
  if (ForceFirstDisplayInternal()) {
    Display::SetInternalDisplayId(id);
    SetInternalManagedDisplayModeList(info);
  }
}

gfx::Size GetMaxNativeSize(const ManagedDisplayInfo& info) {
  gfx::Size size;
  for (auto& mode : info.display_modes()) {
    if (mode.size().GetArea() > size.GetArea())
      size = mode.size();
  }
  return size;
}

bool GetDefaultDisplayMode(const ManagedDisplayInfo& info,
                           ManagedDisplayMode* mode) {
  const auto& modes = info.display_modes();
  auto iter = std::find_if(
      modes.begin(), modes.end(),
      [](const ManagedDisplayMode& mode) { return mode.is_default(); });

  if (iter == modes.end())
    return false;
  *mode = *iter;
  return true;
}

bool ContainsDisplayWithId(const std::vector<Display>& displays,
                           int64_t display_id) {
  for (auto& display : displays) {
    if (display.id() == display_id)
      return true;
  }
  return false;
}

// Gets the next mode in |modes| in the direction marked by |up|. If trying to
// move past either end of |modes|, returns the same.
const ManagedDisplayMode* FindNextMode(
    const ManagedDisplayInfo::ManagedDisplayModeList& modes,
    size_t index,
    bool up) {
  DCHECK_LT(index, modes.size());
  size_t new_index = index;
  if (up && (index + 1 < modes.size()))
    ++new_index;
  else if (!up && index != 0)
    --new_index;
  return &modes[new_index];
}

// Gets the display mode for the next valid UI scale. Returns false if the
// current configured UI scale cannot be found in |info|.
bool GetDisplayModeForNextUIScale(const ManagedDisplayInfo& info,
                                  bool up,
                                  ManagedDisplayMode* mode) {
  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  const float configured_ui_scale = info.configured_ui_scale();
  static const float kEpsilon = 0.0001f;

  auto iter = std::find_if(
      modes.begin(), modes.end(),
      [configured_ui_scale](const ManagedDisplayMode& mode) {
        return std::abs(configured_ui_scale - mode.ui_scale()) < kEpsilon;
      });
  if (iter == modes.end())
    return false;
  *mode = *FindNextMode(modes, iter - modes.begin(), up);
  return true;
}

// Gets the display |mode| for the next valid resolution. Returns false if the
// display is an internal display or if the DIP size cannot be found in |info|.
bool GetDisplayModeForNextResolution(const ManagedDisplayInfo& info,
                                     bool up,
                                     ManagedDisplayMode* mode) {
  if (Display::IsInternalDisplayId(info.id()))
    return false;

  const ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();
  ManagedDisplayMode tmp(info.size_in_pixel(), 0.0, false, false, 1.0,
                         info.device_scale_factor());
  const gfx::Size resolution = tmp.GetSizeInDIP(false);

  auto iter = std::find_if(modes.begin(), modes.end(),
                           [resolution](const ManagedDisplayMode& mode) {
                             return mode.GetSizeInDIP(false) == resolution;
                           });
  if (iter == modes.end())
    return false;
  *mode = *FindNextMode(modes, iter - modes.begin(), up);
  return true;
}

// Returns a pointer to the ManagedDisplayInfo of the display with |id|, nullptr
// if the corresponding info was not found.
const ManagedDisplayInfo* FindInfoById(const DisplayInfoList& display_info_list,
                                       int64_t id) {
  const auto iter = std::find_if(
      display_info_list.begin(), display_info_list.end(),
      [id](const ManagedDisplayInfo& info) { return info.id() == id; });

  if (iter == display_info_list.end())
    return nullptr;

  return &(*iter);
}

// Validates that:
// - All display IDs in the |matrix| are included in the |display_info_list|,
// - All IDs in |display_info_list| exist in the |matrix|,
// - All IDs in the matrix are unique (no repeated IDs).
bool ValidateMatrixForDisplayInfoList(
    const DisplayInfoList& display_info_list,
    const UnifiedDesktopLayoutMatrix& matrix) {
  std::set<int64_t> matrix_ids;
  for (const auto& row : matrix) {
    for (const auto& id : row) {
      if (!matrix_ids.emplace(id).second) {
        LOG(ERROR) << "Matrix has a repeated ID: " << id;
        return false;
      }

      if (!FindInfoById(display_info_list, id)) {
        LOG(ERROR) << "Matrix has ID: " << id << " with no corresponding info "
                   << "in the display info list.";
        return false;
      }
    }
  }

  for (const auto& info : display_info_list) {
    if (!matrix_ids.count(info.id())) {
      LOG(ERROR) << "Display info with ID: " << info.id() << " doesn't exist "
                 << "in the layout matrix.";
      return false;
    }
  }

  return true;
}

}  // namespace

DisplayManager::BeginEndNotifier::BeginEndNotifier(
    DisplayManager* display_manager)
    : display_manager_(display_manager) {
  if (display_manager_->notify_depth_++ == 0) {
    for (auto& observer : display_manager_->observers_)
      observer.OnWillProcessDisplayChanges();
  }
}

DisplayManager::BeginEndNotifier::~BeginEndNotifier() {
  if (--display_manager_->notify_depth_ == 0) {
    for (auto& observer : display_manager_->observers_)
      observer.OnDidProcessDisplayChanges();
  }
}

DisplayManager::DisplayManager(std::unique_ptr<Screen> screen)
    : screen_(std::move(screen)),
      layout_store_(new DisplayLayoutStore),
      is_multi_mirroring_enabled_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              ::switches::kEnableMultiMirroring)),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  configure_displays_ = chromeos::IsRunningAsSystemCompositor();
  change_display_upon_host_resize_ = !configure_displays_;
  unified_desktop_enabled_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableUnifiedDesktop);
  touch_device_manager_ = std::make_unique<TouchDeviceManager>();
#endif
}

DisplayManager::~DisplayManager() {
#if defined(OS_CHROMEOS)
  // Reset the font params.
  gfx::SetFontRenderParamsDeviceScaleFactor(1.0f);
#endif
}

void DisplayManager::SetDevDisplayController(
    mojom::DevDisplayControllerPtr controller) {
  dev_display_controller_ = std::move(controller);
}

bool DisplayManager::InitFromCommandLine() {
  DisplayInfoList info_list;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kHostWindowBounds))
    return false;
  const std::string size_str =
      command_line->GetSwitchValueASCII(::switches::kHostWindowBounds);
  for (const std::string& part : base::SplitString(
           size_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    info_list.push_back(ManagedDisplayInfo::CreateFromSpec(part));
    info_list.back().set_native(true);
  }
  MaybeInitInternalDisplay(&info_list[0]);
  if (info_list.size() > 1 &&
      command_line->HasSwitch(::switches::kEnableSoftwareMirroring)) {
    SetMultiDisplayMode(MIRRORING);
  }
  OnNativeDisplaysChanged(info_list);
  return true;
}

void DisplayManager::InitDefaultDisplay() {
  DisplayInfoList info_list;
  info_list.push_back(ManagedDisplayInfo::CreateFromSpec(std::string()));
  info_list.back().set_native(true);
  MaybeInitInternalDisplay(&info_list[0]);
  OnNativeDisplaysChanged(info_list);
}

void DisplayManager::UpdateInternalDisplay(
    const ManagedDisplayInfo& display_info) {
  DCHECK(Display::HasInternalDisplay());
  InsertAndUpdateDisplayInfo(display_info);
}

void DisplayManager::RefreshFontParams() {
#if defined(OS_CHROMEOS)
  // Use the largest device scale factor among currently active displays. Non
  // internal display may have bigger scale factor in case the external display
  // is an 4K display.
  float largest_device_scale_factor = 1.0f;
  for (const Display& display : active_display_list_) {
    const ManagedDisplayInfo& info = display_info_[display.id()];
    largest_device_scale_factor = std::max(
        largest_device_scale_factor, info.GetEffectiveDeviceScaleFactor());
  }
  gfx::SetFontRenderParamsDeviceScaleFactor(largest_device_scale_factor);
#endif  // OS_CHROMEOS
}

const DisplayLayout& DisplayManager::GetCurrentDisplayLayout() const {
  DCHECK_LE(2U, num_connected_displays());
  if (num_connected_displays() > 1) {
    DisplayIdList list = GetCurrentDisplayIdList();
    return layout_store_->GetRegisteredDisplayLayout(list);
  }
  DLOG(ERROR) << "DisplayLayout is requested for single display";
  // On release build, just fallback to default instead of blowing up.
  static DisplayLayout layout;
  layout.primary_id = active_display_list_[0].id();
  return layout;
}

const DisplayLayout& DisplayManager::GetCurrentResolvedDisplayLayout() const {
  return current_resolved_layout_ ? *current_resolved_layout_
                                  : GetCurrentDisplayLayout();
}

DisplayIdList DisplayManager::GetCurrentDisplayIdList() const {
  if (IsInUnifiedMode())
    return CreateDisplayIdList(software_mirroring_display_list_);

  DisplayIdList display_id_list = CreateDisplayIdList(active_display_list_);

  if (IsInSoftwareMirrorMode()) {
    if (!is_multi_mirroring_enabled_) {
      CHECK_EQ(2u, num_connected_displays());
      // This comment is to make it easy to distinguish the crash
      // between two checks.
      CHECK_EQ(1u, active_display_list_.size());
    }

    DisplayIdList software_mirroring_display_id_list =
        CreateDisplayIdList(software_mirroring_display_list_);
    display_id_list.insert(display_id_list.end(),
                           software_mirroring_display_id_list.begin(),
                           software_mirroring_display_id_list.end());
    return display_id_list;
  }

  if (IsInHardwareMirrorMode()) {
    display_id_list.insert(display_id_list.end(),
                           hardware_mirroring_display_id_list_.begin(),
                           hardware_mirroring_display_id_list_.end());
    return display_id_list;
  }

  CHECK_LE(2u, active_display_list_.size());
  return display_id_list;
}

void DisplayManager::SetLayoutForCurrentDisplays(
    std::unique_ptr<DisplayLayout> layout) {
  if (GetNumDisplays() == 1)
    return;
  BeginEndNotifier notifier(this);

  const DisplayIdList list = GetCurrentDisplayIdList();

  DCHECK(DisplayLayout::Validate(list, *layout));

  const DisplayLayout& current_layout =
      layout_store_->GetRegisteredDisplayLayout(list);

  if (layout->HasSamePlacementList(current_layout))
    return;

  layout_store_->RegisterLayoutForDisplayIdList(list, std::move(layout));
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);

  // TODO(oshima): Call UpdateDisplays instead.
  std::vector<int64_t> updated_ids;
  current_resolved_layout_ = GetCurrentDisplayLayout().Copy();
  ApplyDisplayLayout(current_resolved_layout_.get(), &active_display_list_,
                     &updated_ids);
  for (int64_t id : updated_ids) {
    NotifyMetricsChanged(GetDisplayForId(id),
                         DisplayObserver::DISPLAY_METRIC_BOUNDS |
                             DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  }

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

const Display& DisplayManager::GetDisplayForId(int64_t display_id) const {
  Display* display =
      const_cast<DisplayManager*>(this)->FindDisplayForId(display_id);
  return display ? *display : GetInvalidDisplay();
}

bool DisplayManager::IsDisplayIdValid(int64_t display_id) const {
  return GetDisplayForId(display_id).is_valid();
}

const Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  auto iter = display::FindDisplayContainingPoint(active_display_list_,
                                                  point_in_screen);
  return iter == active_display_list_.end() ? GetInvalidDisplay() : *iter;
}

bool DisplayManager::UpdateWorkAreaOfDisplay(int64_t display_id,
                                             const gfx::Insets& insets) {
  BeginEndNotifier notifier(this);
  Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  gfx::Rect old_work_area = display->work_area();
  display->UpdateWorkAreaFromInsets(insets);
  bool workarea_changed = old_work_area != display->work_area();
  if (workarea_changed)
    NotifyMetricsChanged(*display, DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  return workarea_changed;
}

void DisplayManager::SetOverscanInsets(int64_t display_id,
                                       const gfx::Insets& insets_in_dip) {
  bool update = false;
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (insets_in_dip.IsEmpty()) {
        info.set_clear_overscan_insets(true);
      } else {
        info.set_clear_overscan_insets(false);
        info.SetOverscanInsets(insets_in_dip);
      }
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else {
    display_info_[display_id].SetOverscanInsets(insets_in_dip);
  }
}

void DisplayManager::SetDisplayRotation(int64_t display_id,
                                        Display::Rotation rotation,
                                        Display::RotationSource source) {
  if (IsInUnifiedMode())
    return;

  DisplayInfoList display_info_list;
  bool is_active = false;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      if (info.GetRotation(source) == rotation &&
          info.GetActiveRotation() == rotation) {
        return;
      }
      info.SetRotation(rotation, source);
      is_active = true;
    }
    display_info_list.push_back(info);
  }
  if (is_active) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  } else if (display_info_.find(display_id) != display_info_.end()) {
    // Inactive displays can reactivate, ensure they have been updated.
    display_info_[display_id].SetRotation(rotation, source);
  }
}

bool DisplayManager::SetDisplayMode(int64_t display_id,
                                    const ManagedDisplayMode& display_mode) {
  bool change_ui_scale = GetDisplayIdForUIScaling() == display_id;

  DisplayInfoList display_info_list;
  bool display_property_changed = false;
  bool resolution_changed = false;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      auto iter = FindDisplayMode(info, display_mode);
      if (iter == info.display_modes().end()) {
        DLOG(WARNING) << "Unsupported display mode was requested:"
                      << "size=" << display_mode.size().ToString()
                      << ", ui scale=" << display_mode.ui_scale()
                      << ", scale factor="
                      << display_mode.device_scale_factor();
        return false;
      }

      if (change_ui_scale) {
        if (info.configured_ui_scale() == display_mode.ui_scale())
          return true;
        info.set_configured_ui_scale(display_mode.ui_scale());
        display_property_changed = true;
      } else {
        display_modes_[display_id] = *iter;
        if (info.bounds_in_native().size() != display_mode.size()) {
          // If resolution changes, then we can break right here. No need to
          // continue to fill |display_info_list|, since we won't be
          // synchronously updating the displays here.
          resolution_changed = true;
          break;
        }
        if (info.device_scale_factor() != display_mode.device_scale_factor()) {
          info.set_device_scale_factor(display_mode.device_scale_factor());
          display_property_changed = true;
        }
      }
    }
    display_info_list.emplace_back(info);
  }

  if (display_property_changed && !resolution_changed) {
    // We shouldn't synchronously update the displays here if the resolution
    // changed. This should happen asynchronously when configuration is
    // triggered.
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplaysWith(display_info_list);
  }

  if (resolution_changed && IsInUnifiedMode())
    ReconfigureDisplays();
#if defined(OS_CHROMEOS)
  else if (resolution_changed && configure_displays_)
    delegate_->display_configurator()->OnConfigurationChanged();
#endif  // defined(OS_CHROMEOS)

  return resolution_changed || display_property_changed;
}

void DisplayManager::RegisterDisplayProperty(
    int64_t display_id,
    Display::Rotation rotation,
    float ui_scale,
    const gfx::Insets* overscan_insets,
    const gfx::Size& resolution_in_pixels,
    float device_scale_factor) {
  if (display_info_.find(display_id) == display_info_.end())
    display_info_[display_id] =
        ManagedDisplayInfo(display_id, std::string(), false);

  // Do not allow rotation in unified desktop mode.
  if (display_id == kUnifiedDisplayId)
    rotation = Display::ROTATE_0;

  display_info_[display_id].SetRotation(rotation,
                                        Display::ROTATION_SOURCE_USER);
  display_info_[display_id].SetRotation(rotation,
                                        Display::ROTATION_SOURCE_ACTIVE);
  // Just in case the preference file was corrupted.
  // TODO(mukai): register |display_modes_| here as well, so the lookup for the
  // default mode in GetActiveModeForDisplayId() gets much simpler.
  if (0.5f <= ui_scale && ui_scale <= 2.0f)
    display_info_[display_id].set_configured_ui_scale(ui_scale);
  if (overscan_insets)
    display_info_[display_id].SetOverscanInsets(*overscan_insets);

  if (!resolution_in_pixels.IsEmpty()) {
    DCHECK(!Display::IsInternalDisplayId(display_id));
    // Default refresh rate, until OnNativeDisplaysChanged() updates us with the
    // actual display info, is 60 Hz.
    ManagedDisplayMode mode(resolution_in_pixels, 60.0f, false, false, 1.0,
                            device_scale_factor);
    display_modes_[display_id] = mode;
  }
}

bool DisplayManager::GetActiveModeForDisplayId(int64_t display_id,
                                               ManagedDisplayMode* mode) const {
  ManagedDisplayMode selected_mode;
  if (GetSelectedModeForDisplayId(display_id, &selected_mode)) {
    *mode = selected_mode;
    return true;
  }

  // If 'selected' mode is empty, it should return the default mode. This means
  // the native mode for the external display. Unfortunately this is not true
  // for the internal display because restoring UI-scale doesn't register the
  // restored mode to |display_mode_|, so it needs to look up the mode whose
  // UI-scale value matches. See the TODO in RegisterDisplayProperty().
  const ManagedDisplayInfo& info = GetDisplayInfo(display_id);
  const ManagedDisplayInfo::ManagedDisplayModeList& display_modes =
      info.display_modes();

  for (const auto& display_mode : display_modes) {
    if (GetDisplayIdForUIScaling() == display_id) {
      if (info.configured_ui_scale() == display_mode.ui_scale()) {
        *mode = display_mode;
        return true;
      }
    } else if (display_mode.native()) {
      *mode = display_mode;
      return true;
    }
  }
  return false;
}

void DisplayManager::RegisterDisplayRotationProperties(
    bool rotation_lock,
    Display::Rotation rotation) {
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  registered_internal_display_rotation_lock_ = rotation_lock;
  registered_internal_display_rotation_ = rotation;
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

bool DisplayManager::GetSelectedModeForDisplayId(
    int64_t display_id,
    ManagedDisplayMode* mode) const {
  std::map<int64_t, ManagedDisplayMode>::const_iterator iter =
      display_modes_.find(display_id);
  if (iter == display_modes_.end())
    return false;
  *mode = iter->second;
  return true;
}

void DisplayManager::SetSelectedModeForDisplayId(
    int64_t display_id,
    const ManagedDisplayMode& display_mode) {
  ManagedDisplayInfo info = GetDisplayInfo(display_id);
  auto iter = FindDisplayMode(info, display_mode);
  if (iter == info.display_modes().end()) {
    DLOG(WARNING) << "Unsupported display mode was requested:"
                  << "size=" << display_mode.size().ToString()
                  << ", ui scale=" << display_mode.ui_scale()
                  << ", scale factor=" << display_mode.device_scale_factor();
  }

  display_modes_[display_id] = *iter;
}

bool DisplayManager::IsDisplayUIScalingEnabled() const {
  return GetDisplayIdForUIScaling() != kInvalidDisplayId;
}

gfx::Insets DisplayManager::GetOverscanInsets(int64_t display_id) const {
  std::map<int64_t, ManagedDisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ? it->second.overscan_insets_in_dip()
                                     : gfx::Insets();
}

void DisplayManager::OnNativeDisplaysChanged(
    const DisplayInfoList& updated_displays) {
  if (updated_displays.empty()) {
    DVLOG(1) << __func__
             << "(0): # of current displays=" << active_display_list_.size();
    // If the device is booted without display, or chrome is started
    // without --ash-host-window-bounds on linux desktop, use the
    // default display.
    if (active_display_list_.empty()) {
      DisplayInfoList init_displays;
      init_displays.push_back(
          ManagedDisplayInfo::CreateFromSpec(std::string()));
      MaybeInitInternalDisplay(&init_displays[0]);
      OnNativeDisplaysChanged(init_displays);
    } else {
      // Otherwise don't update the displays when all displays are disconnected.
      // This happens when:
      // - the device is idle and powerd requested to turn off all displays.
      // - the device is suspended. (kernel turns off all displays)
      // - the internal display's brightness is set to 0 and no external
      //   display is connected.
      // - the internal display's brightness is 0 and external display is
      //   disconnected.
      // The display will be updated when one of displays is turned on, and the
      // display list will be updated correctly.
    }
    return;
  }
  DVLOG_IF(1, updated_displays.size() == 1)
      << __func__ << "(1):" << updated_displays[0].ToString();
  DVLOG_IF(1, updated_displays.size() > 1)
      << __func__ << "(" << updated_displays.size()
      << ") [0]=" << updated_displays[0].ToString()
      << ", [1]=" << updated_displays[1].ToString();

  first_display_id_ = updated_displays[0].id();
  std::map<gfx::Point, int64_t> origins;

  bool internal_display_connected = false;
  num_connected_displays_ = updated_displays.size();
  ClearMirroringSourceAndDestination();
  DisplayInfoList new_display_info_list;
  for (const auto& display_info : updated_displays) {
    if (!internal_display_connected)
      internal_display_connected =
          Display::IsInternalDisplayId(display_info.id());
    // Mirrored monitors have the same origins.
    gfx::Point origin = display_info.bounds_in_native().origin();
    const auto iter = origins.find(origin);
    if (iter != origins.end()) {
      InsertAndUpdateDisplayInfo(display_info);
      if (hardware_mirroring_display_id_list_.empty()) {
        // Unlike software mirroring, hardware mirroring has no source and
        // target. All mirroring displays scan the same frame buffer. But for
        // convenience, we treat the first mirroring display as source.
        mirroring_source_id_ = iter->second;
      }
      // Only keep the first hardware mirroring display in
      // |new_display_info_list| because hardware mirroring is not visible for
      // display manager and all hardware mirroring displays should be treated
      // as one single display from this point.
      hardware_mirroring_display_id_list_.emplace_back(display_info.id());
    } else {
      origins.emplace(origin, display_info.id());
      new_display_info_list.emplace_back(display_info);
    }

    ManagedDisplayMode new_mode(
        display_info.bounds_in_native().size(), 0.0 /* refresh rate */,
        false /* interlaced */, false /* native */,
        display_info.configured_ui_scale(), display_info.device_scale_factor());
    const ManagedDisplayInfo::ManagedDisplayModeList& display_modes =
        display_info.display_modes();
    // This is empty the displays are initialized from InitFromCommandLine.
    if (display_modes.empty())
      continue;
    auto display_modes_iter = FindDisplayMode(display_info, new_mode);
    // Update the actual resolution selected as the resolution request may fail.
    if (display_modes_iter == display_modes.end())
      display_modes_.erase(display_info.id());
    else if (display_modes_.find(display_info.id()) != display_modes_.end())
      display_modes_[display_info.id()] = *display_modes_iter;
  }
  if (Display::HasInternalDisplay() && !internal_display_connected) {
    if (display_info_.find(Display::InternalDisplayId()) ==
        display_info_.end()) {
      // Create a dummy internal display if the chrome restarted
      // in docked mode.
      ManagedDisplayInfo internal_display_info(
          Display::InternalDisplayId(),
          l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_INTERNAL),
          false /*Internal display must not have overscan */);
      internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
      display_info_[Display::InternalDisplayId()] = internal_display_info;
    } else {
      // Internal display is no longer active. Reset its rotation to user
      // preference, so that it is restored when the internal display becomes
      // active again.
      Display::Rotation user_rotation =
          display_info_[Display::InternalDisplayId()].GetRotation(
              Display::ROTATION_SOURCE_USER);
      display_info_[Display::InternalDisplayId()].SetRotation(
          user_rotation, Display::ROTATION_SOURCE_USER);
    }
  }

#if defined(OS_CHROMEOS)
  if (!configure_displays_ && new_display_info_list.size() > 1) {
    DisplayIdList list = GenerateDisplayIdList(
        new_display_info_list.begin(), new_display_info_list.end(),
        [](const ManagedDisplayInfo& info) { return info.id(); });

    const DisplayLayout& layout =
        layout_store_->GetRegisteredDisplayLayout(list);
    // Mirror mode is set by DisplayConfigurator on the device.
    // Emulate it when running on linux desktop.
    if (layout.mirrored)
      SetMultiDisplayMode(MIRRORING);
  }
#endif

  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplaysWith(display_info_list);
}

void DisplayManager::UpdateDisplaysWith(
    const DisplayInfoList& updated_display_info_list) {
  BeginEndNotifier notifier(this);

#if defined(OS_WIN)
  DCHECK_EQ(1u, updated_display_info_list.size())
      << ": Multiple display test does not work on Windows bots. Please "
         "skip (don't disable) the test.";
#endif

  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(active_display_list_.begin(), active_display_list_.end(),
            DisplaySortFunctor());
  std::sort(new_display_info_list.begin(), new_display_info_list.end(),
            DisplayInfoSortFunctor());

  if (new_display_info_list.size() > 1) {
    DisplayIdList list = GenerateDisplayIdList(
        new_display_info_list.begin(), new_display_info_list.end(),
        [](const ManagedDisplayInfo& info) { return info.id(); });
    const DisplayLayout& layout =
        layout_store_->GetRegisteredDisplayLayout(list);
    current_default_multi_display_mode_ =
        (layout.default_unified && unified_desktop_enabled_) ? UNIFIED
                                                             : EXTENDED;
  }

  if (multi_display_mode_ != MIRRORING)
    multi_display_mode_ = current_default_multi_display_mode_;

  CreateSoftwareMirroringDisplayInfo(&new_display_info_list);

  // Close the mirroring window if any here to avoid creating two compositor on
  // one display.
  if (delegate_)
    delegate_->CloseMirroringDisplayIfNotNecessary();

  Displays new_displays;
  Displays removed_displays;
  std::map<size_t, uint32_t> display_changes;
  std::vector<size_t> added_display_indices;

  Displays::iterator curr_iter = active_display_list_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  while (curr_iter != active_display_list_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (curr_iter == active_display_list_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    } else if (new_info_iter == new_display_info_list.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else if (curr_iter->id() == new_info_iter->id()) {
      const Display& current_display = *curr_iter;
      // Copy the info because |InsertAndUpdateDisplayInfo| updates the
      // instance.
      const ManagedDisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const ManagedDisplayInfo& new_display_info =
          GetDisplayInfo(new_display.id());

      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_NONE;

      // At that point the new Display objects we have are not entirely updated,
      // they are missing the translation related to the Display disposition in
      // the layout.
      // Using display.bounds() and display.work_area() would fail most of the
      // time.
      if (force_bounds_changed_ || (current_display_info.bounds_in_native() !=
                                    new_display_info.bounds_in_native()) ||
          (current_display_info.GetOverscanInsetsInPixel() !=
           new_display_info.GetOverscanInsetsInPixel()) ||
          current_display.size() != new_display.size()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_BOUNDS |
                   DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      }

      if (current_display.device_scale_factor() !=
          new_display.device_scale_factor()) {
        metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
      }

      if (current_display.rotation() != new_display.rotation())
        metrics |= DisplayObserver::DISPLAY_METRIC_ROTATION;

      if (metrics != DisplayObserver::DISPLAY_METRIC_NONE) {
        display_changes.insert(
            std::pair<size_t, uint32_t>(new_displays.size(), metrics));
      }

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      new_displays.push_back(new_display);
      ++curr_iter;
      ++new_info_iter;
    } else if (curr_iter->id() < new_info_iter->id()) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }
  Display old_primary;
  if (delegate_)
    old_primary = screen_->GetPrimaryDisplay();

  // Clear focus if the display has been removed, but don't clear focus if
  // the destkop has been moved from one display to another
  // (mirror -> docked, docked -> single internal).
  bool clear_focus =
      !removed_displays.empty() &&
      !(removed_displays.size() == 1 && added_display_indices.size() == 1);
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(clear_focus);

  std::vector<size_t> updated_indices;
  UpdateNonPrimaryDisplayBoundsForLayout(&new_displays, &updated_indices);
  for (size_t updated_index : updated_indices) {
    if (!base::ContainsValue(added_display_indices, updated_index)) {
      uint32_t metrics = DisplayObserver::DISPLAY_METRIC_BOUNDS |
                         DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      if (display_changes.find(updated_index) != display_changes.end())
        metrics |= display_changes[updated_index];

      display_changes[updated_index] = metrics;
    }
  }

  active_display_list_ = new_displays;
  active_only_display_list_ = active_display_list_;

  RefreshFontParams();
  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  size_t active_display_list_size = active_display_list_.size();
  is_updating_display_list_ = true;
  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  active_display_list_.insert(active_display_list_.end(),
                              removed_displays.begin(), removed_displays.end());

  for (const auto& display : removed_displays)
    NotifyDisplayRemoved(display);

  for (size_t index : added_display_indices)
    NotifyDisplayAdded(active_display_list_[index]);

  active_display_list_.resize(active_display_list_size);
  is_updating_display_list_ = false;

  bool notify_primary_change =
      delegate_ ? old_primary.id() != screen_->GetPrimaryDisplay().id() : false;

  for (std::map<size_t, uint32_t>::iterator iter = display_changes.begin();
       iter != display_changes.end(); ++iter) {
    uint32_t metrics = iter->second;
    const Display& updated_display = active_display_list_[iter->first];

    if (notify_primary_change &&
        updated_display.id() == screen_->GetPrimaryDisplay().id()) {
      metrics |= DisplayObserver::DISPLAY_METRIC_PRIMARY;
      notify_primary_change = false;
    }
    NotifyMetricsChanged(updated_display, metrics);
  }

  uint32_t primary_metrics = 0;

  if (notify_primary_change) {
    // This happens when a primary display has moved to anther display without
    // bounds change.
    const Display& primary = screen_->GetPrimaryDisplay();
    if (primary.id() != old_primary.id()) {
      primary_metrics = DisplayObserver::DISPLAY_METRIC_PRIMARY;
      if (primary.size() != old_primary.size()) {
        primary_metrics |= (DisplayObserver::DISPLAY_METRIC_BOUNDS |
                            DisplayObserver::DISPLAY_METRIC_WORK_AREA);
      }
      if (primary.device_scale_factor() != old_primary.device_scale_factor())
        primary_metrics |= DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
    }
  }

  bool mirror_mode = IsInMirrorMode();
  if (mirror_mode != mirror_mode_for_metrics_) {
    primary_metrics |= DisplayObserver::DISPLAY_METRIC_MIRROR_STATE;
    mirror_mode_for_metrics_ = mirror_mode;
  }

  if (delegate_ && primary_metrics)
    NotifyMetricsChanged(screen_->GetPrimaryDisplay(), primary_metrics);

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();

  // Create the mirroring window asynchronously after all displays
  // are added so that it can mirror the display newly added. This can
  // happen when switching from dock mode to software mirror mode.
  CreateMirrorWindowAsyncIfAny();
}

const Display& DisplayManager::GetDisplayAt(size_t index) const {
  DCHECK_LT(index, active_display_list_.size());
  return active_display_list_[index];
}

const Display& DisplayManager::GetPrimaryDisplayCandidate() const {
  if (GetNumDisplays() != 2)
    return active_display_list_[0];
  const DisplayLayout& layout =
      layout_store_->GetRegisteredDisplayLayout(GetCurrentDisplayIdList());
  return GetDisplayForId(layout.primary_id);
}

size_t DisplayManager::GetNumDisplays() const {
  return active_display_list_.size();
}

bool DisplayManager::IsActiveDisplayId(int64_t display_id) const {
  return ContainsDisplayWithId(active_display_list_, display_id);
}

bool DisplayManager::IsInMirrorMode() const {
  // Either software or hardware mirror mode can be active at the same time.
  DCHECK(!IsInSoftwareMirrorMode() || !IsInHardwareMirrorMode());
  return IsInSoftwareMirrorMode() || IsInHardwareMirrorMode();
}

bool DisplayManager::IsInSoftwareMirrorMode() const {
  if (multi_display_mode_ != MIRRORING ||
      software_mirroring_display_list_.empty()) {
    return false;
  }

  // Software mirroring cannot coexist with hardware mirroring.
  DCHECK(hardware_mirroring_display_id_list_.empty());
  return true;
}

bool DisplayManager::IsInHardwareMirrorMode() const {
  if (hardware_mirroring_display_id_list_.empty())
    return false;

  // Hardware mirroring is not visible to the display manager, the display mode
  // should be EXTENDED.
  DCHECK(multi_display_mode_ == EXTENDED);

  // Hardware mirroring cannot coexist with software mirroring.
  DCHECK(software_mirroring_display_list_.empty());
  return true;
}

DisplayIdList DisplayManager::GetMirroringDestinationDisplayIdList() const {
  if (IsInSoftwareMirrorMode())
    return CreateDisplayIdList(software_mirroring_display_list_);
  if (IsInHardwareMirrorMode())
    return hardware_mirroring_display_id_list_;
  return DisplayIdList();
}

void DisplayManager::ClearMirroringSourceAndDestination() {
  mirroring_source_id_ = kInvalidDisplayId;
  hardware_mirroring_display_id_list_.clear();
  software_mirroring_display_list_.clear();
}

void DisplayManager::SetUnifiedDesktopEnabled(bool enable) {
  unified_desktop_enabled_ = enable;
  // There is no need to update the displays in mirror mode. Doing
  // this in hardware mirroring mode can cause crash because display
  // info in hardware mirroring comes from DisplayConfigurator.
  if (!IsInMirrorMode())
    ReconfigureDisplays();
}

bool DisplayManager::IsInUnifiedMode() const {
  return multi_display_mode_ == UNIFIED &&
         !software_mirroring_display_list_.empty();
}

void DisplayManager::SetUnifiedDesktopMatrix(
    const UnifiedDesktopLayoutMatrix& matrix) {
  current_unified_desktop_matrix_ = matrix;
  SetDefaultMultiDisplayModeForCurrentDisplays(UNIFIED);
}

const Display* DisplayManager::GetPrimaryMirroringDisplayForUnifiedDesktop()
    const {
  if (!IsInUnifiedMode())
    return nullptr;

  return &software_mirroring_display_list_[0];
}

int DisplayManager::GetMirroringDisplayRowIndexInUnifiedMatrix(
    int64_t display_id) const {
  DCHECK(IsInUnifiedMode());

  return mirroring_display_id_to_unified_matrix_row_.at(display_id);
}

int DisplayManager::GetUnifiedDesktopRowMaxHeight(int row_index) const {
  DCHECK(IsInUnifiedMode());

  return unified_display_rows_heights_.at(row_index);
}

const ManagedDisplayInfo& DisplayManager::GetDisplayInfo(
    int64_t display_id) const {
  DCHECK_NE(kInvalidDisplayId, display_id);

  std::map<int64_t, ManagedDisplayInfo>::const_iterator iter =
      display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

const Display DisplayManager::GetMirroringDisplayById(
    int64_t display_id) const {
  auto iter = std::find_if(software_mirroring_display_list_.begin(),
                           software_mirroring_display_list_.end(),
                           [display_id](const Display& display) {
                             return display.id() == display_id;
                           });
  return iter == software_mirroring_display_list_.end() ? Display() : *iter;
}

std::string DisplayManager::GetDisplayNameForId(int64_t id) {
  if (id == kInvalidDisplayId)
    return l10n_util::GetStringUTF8(IDS_DISPLAY_NAME_UNKNOWN);

  std::map<int64_t, ManagedDisplayInfo>::const_iterator iter =
      display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

int64_t DisplayManager::GetDisplayIdForUIScaling() const {
  // UI Scaling is effective on internal display.
  return Display::HasInternalDisplay() ? Display::InternalDisplayId()
                                       : kInvalidDisplayId;
}

void DisplayManager::SetMirrorMode(bool mirror) {
  if ((is_multi_mirroring_enabled_ && num_connected_displays() < 2) ||
      (!is_multi_mirroring_enabled_ && num_connected_displays() != 2)) {
    return;
  }

#if defined(OS_CHROMEOS)
  if (configure_displays_) {
    MultipleDisplayState new_state =
        mirror ? MULTIPLE_DISPLAY_STATE_DUAL_MIRROR
               : MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
    delegate_->display_configurator()->SetDisplayMode(new_state);
    return;
  }
#endif
  multi_display_mode_ =
      mirror ? MIRRORING : current_default_multi_display_mode_;
  ReconfigureDisplays();
}

void DisplayManager::AddRemoveDisplay() {
  DCHECK(!active_display_list_.empty());

  // DevDisplayController will have NativeDisplayDelegate add/remove a display
  // so that the full display configuration code runs.
  if (dev_display_controller_.is_bound()) {
    dev_display_controller_->ToggleAddRemoveDisplay();
    return;
  }

  DisplayInfoList new_display_info_list;
  const ManagedDisplayInfo& first_display =
      IsInUnifiedMode()
          ? GetDisplayInfo(software_mirroring_display_list_[0].id())
          : GetDisplayInfo(active_display_list_[0].id());
  new_display_info_list.push_back(first_display);
  // Add if there is only one display connected.
  if (num_connected_displays() == 1) {
    const int kVerticalOffsetPx = 100;
    // Layout the 2nd display below the primary as with the real device.
    gfx::Rect host_bounds = first_display.bounds_in_native();
    new_display_info_list.push_back(
        ManagedDisplayInfo::CreateFromSpec(base::StringPrintf(
            "%d+%d-600x%d", host_bounds.x(),
            host_bounds.bottom() + kVerticalOffsetPx, host_bounds.height())));
  }
  num_connected_displays_ = new_display_info_list.size();
  ClearMirroringSourceAndDestination();
  UpdateDisplaysWith(new_display_info_list);
}

void DisplayManager::ToggleDisplayScaleFactor() {
  DCHECK(!active_display_list_.empty());
  DisplayInfoList new_display_info_list;
  for (Displays::const_iterator iter = active_display_list_.begin();
       iter != active_display_list_.end(); ++iter) {
    ManagedDisplayInfo display_info = GetDisplayInfo(iter->id());
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  AddMirrorDisplayInfoIfAny(&new_display_info_list);
  UpdateDisplaysWith(new_display_info_list);
}

#if defined(OS_CHROMEOS)
void DisplayManager::SetSoftwareMirroring(bool enabled) {
  SetMultiDisplayMode(enabled ? MIRRORING
                              : current_default_multi_display_mode_);
}

bool DisplayManager::SoftwareMirroringEnabled() const {
  return multi_display_mode_ == MIRRORING;
}

void DisplayManager::SetTouchCalibrationData(
    int64_t display_id,
    const TouchCalibrationData::CalibrationPointPairQuad& point_pair_quad,
    const gfx::Size& display_bounds,
    const TouchDeviceIdentifier& touch_device_identifier) {
  // We do not proceed with setting the calibration and association if the
  // touch device identified by |touch_device_identifier| is an internal touch
  // device.
  if (IsInternalTouchscreenDevice(touch_device_identifier))
    return;

  // Id of the display the touch device in context is currently associated
  // with. This display id will be equal to |display_id| if no reassociation is
  // being performed.
  int64_t previous_display_id =
      touch_device_manager_->GetAssociatedDisplay(touch_device_identifier);

  bool update_add_support = false;
  bool update_remove_support = false;

  TouchCalibrationData calibration_data(point_pair_quad, display_bounds);
  touch_device_manager_->AddTouchCalibrationData(touch_device_identifier,
                                                 display_id, calibration_data);

  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    if (info.id() == display_id) {
      info.set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
      update_add_support = true;
    } else if (info.id() == previous_display_id) {
      // Since we are reassociating the touch device to another display, we need
      // to check whether the display it was previous connected to still
      // supports touch.
      if (!touch_device_manager_
               ->GetAssociatedTouchDevicesForDisplay(previous_display_id)
               .empty()) {
        info.set_touch_support(Display::TOUCH_SUPPORT_UNAVAILABLE);
        update_remove_support = true;
      }
    }
    display_info_list.push_back(info);
  }

  // Update the non active displays.
  if (!update_add_support) {
    display_info_[display_id].set_touch_support(
        Display::TOUCH_SUPPORT_AVAILABLE);
  }
  if (!update_remove_support &&
      !touch_device_manager_
           ->GetAssociatedTouchDevicesForDisplay(previous_display_id)
           .empty()) {
    display_info_[previous_display_id].set_touch_support(
        Display::TOUCH_SUPPORT_UNAVAILABLE);
  }
  // Update the active displays.
  if (update_add_support || update_remove_support)
    UpdateDisplaysWith(display_info_list);
}

void DisplayManager::ClearTouchCalibrationData(
    int64_t display_id,
    base::Optional<TouchDeviceIdentifier> touch_device_identifier) {
  if (touch_device_identifier) {
    touch_device_manager_->ClearTouchCalibrationData(*touch_device_identifier,
                                                     display_id);
  } else {
    touch_device_manager_->ClearAllTouchCalibrationData(display_id);
  }

  DisplayInfoList display_info_list;
  for (const auto& display : active_display_list_) {
    ManagedDisplayInfo info = GetDisplayInfo(display.id());
    display_info_list.push_back(info);
  }
  UpdateDisplaysWith(display_info_list);
}
#endif

void DisplayManager::SetDefaultMultiDisplayModeForCurrentDisplays(
    MultiDisplayMode mode) {
  DCHECK_NE(MIRRORING, mode);
  DisplayIdList list = GetCurrentDisplayIdList();
  layout_store_->UpdateMultiDisplayState(list, IsInMirrorMode(),
                                         mode == UNIFIED);
  ReconfigureDisplays();
}

void DisplayManager::SetMultiDisplayMode(MultiDisplayMode mode) {
  multi_display_mode_ = mode;
  ClearMirroringSourceAndDestination();
}

void DisplayManager::ReconfigureDisplays() {
  DisplayInfoList display_info_list;
  for (const Display& display : active_display_list_) {
    if (display.id() == kUnifiedDisplayId)
      continue;
    display_info_list.push_back(GetDisplayInfo(display.id()));
  }
  for (const Display& display : software_mirroring_display_list_)
    display_info_list.push_back(GetDisplayInfo(display.id()));
  ClearMirroringSourceAndDestination();
  UpdateDisplaysWith(display_info_list);
}

bool DisplayManager::UpdateDisplayBounds(int64_t display_id,
                                         const gfx::Rect& new_bounds) {
  if (!change_display_upon_host_resize_)
    return false;
  display_info_[display_id].SetBounds(new_bounds);
  // Don't notify observers if the mirrored window has changed.
  if (IsInSoftwareMirrorMode() &&
      std::find_if(software_mirroring_display_list_.begin(),
                   software_mirroring_display_list_.end(),
                   [display_id](const Display& display) {
                     return display.id() == display_id;
                   }) != software_mirroring_display_list_.end()) {
    return false;
  }

  // In unified mode then |active_display_list_| won't have a display for
  // |display_id| but |software_mirroring_display_list_| should. Reconfigure
  // the displays so the unified display size is recomputed.
  if (IsInUnifiedMode() &&
      ContainsDisplayWithId(software_mirroring_display_list_, display_id)) {
    DCHECK(!IsActiveDisplayId(display_id));
    ReconfigureDisplays();
    return true;
  }

  Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  display->SetSize(display_info_[display_id].size_in_pixel());
  BeginEndNotifier notifier(this);
  NotifyMetricsChanged(*display, DisplayObserver::DISPLAY_METRIC_BOUNDS);
  return true;
}

void DisplayManager::CreateMirrorWindowAsyncIfAny() {
  // Do not post a task if the software mirroring doesn't exist, or
  // during initialization when compositor's init task isn't posted yet.
  // ash::Shell::Init() will call this after the compositor is initialized.
  if (software_mirroring_display_list_.empty() || !delegate_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DisplayManager::CreateMirrorWindowIfAny,
                            weak_ptr_factory_.GetWeakPtr()));
}

void DisplayManager::UpdateInternalManagedDisplayModeListForTest() {
  if (!Display::HasInternalDisplay() ||
      display_info_.count(Display::InternalDisplayId()) == 0) {
    return;
  }
  ManagedDisplayInfo* info = &display_info_[Display::InternalDisplayId()];
  SetInternalManagedDisplayModeList(info);
}

bool DisplayManager::ZoomInternalDisplay(bool up) {
  int64_t display_id =
      IsInUnifiedMode() ? kUnifiedDisplayId : GetDisplayIdForUIScaling();
  const ManagedDisplayInfo& display_info = GetDisplayInfo(display_id);

  ManagedDisplayMode mode;
  bool result = false;
  if (IsInUnifiedMode()) {
    result = GetDisplayModeForNextResolution(display_info, up, &mode);
  } else {
    if (!IsActiveDisplayId(display_info.id()) ||
        !Display::IsInternalDisplayId(display_info.id())) {
      return false;
    }
    result = GetDisplayModeForNextUIScale(display_info, up, &mode);
  }

  return result ? SetDisplayMode(display_id, mode) : false;
}

bool DisplayManager::ResetDisplayToDefaultMode(int64_t id) {
  if (!IsActiveDisplayId(id) || !Display::IsInternalDisplayId(id))
    return false;

  const ManagedDisplayInfo& info = GetDisplayInfo(id);
  ManagedDisplayMode mode;
  if (GetDefaultDisplayMode(info, &mode))
    return SetDisplayMode(id, mode);
  return false;
}

void DisplayManager::ResetInternalDisplayZoom() {
  if (IsInUnifiedMode()) {
    const ManagedDisplayInfo& display_info = GetDisplayInfo(kUnifiedDisplayId);
    const ManagedDisplayInfo::ManagedDisplayModeList& modes =
        display_info.display_modes();
    auto iter = std::find_if(
        modes.begin(), modes.end(),
        [](const ManagedDisplayMode& mode) { return mode.native(); });
    SetDisplayMode(kUnifiedDisplayId, *iter);
  } else {
    ResetDisplayToDefaultMode(GetDisplayIdForUIScaling());
  }
}

void DisplayManager::CreateSoftwareMirroringDisplayInfo(
    DisplayInfoList* display_info_list) {
  // Use the internal display or 1st as the mirror source, then scale
  // the root window so that it matches the external display's
  // resolution. This is necessary in order for scaling to work while
  // mirrored.
  switch (multi_display_mode_) {
    case MIRRORING: {
      if ((is_multi_mirroring_enabled_ && display_info_list->size() < 2) ||
          (!is_multi_mirroring_enabled_ && display_info_list->size() != 2)) {
        return;
      }

      int64_t source_id = kInvalidDisplayId;
      if (Display::HasInternalDisplay()) {
        // Use the internal display as mirroring source.
        source_id = Display::InternalDisplayId();
        auto iter =
            std::find_if(display_info_list->begin(), display_info_list->end(),
                         [source_id](const ManagedDisplayInfo& info) {
                           return info.id() == source_id;
                         });
        if (iter == display_info_list->end()) {
          // It is possible that internal display is removed (e.g. Use
          // Chromebook in Dock mode with two or more external displays). In
          // this case, we use the first connected display as mirroring source.
          source_id = first_display_id_;
        }
      } else {
        // Use the first connected display as mirroring source
        source_id = first_display_id_;
      }
      DCHECK(source_id != kInvalidDisplayId);

      for (auto& info : *display_info_list) {
        if (source_id == info.id())
          continue;
        info.SetOverscanInsets(gfx::Insets());
        InsertAndUpdateDisplayInfo(info);
        software_mirroring_display_list_.emplace_back(
            CreateMirroringDisplayFromDisplayInfoById(info.id(), gfx::Point(),
                                                      1.0f));
      }

      // Remove all destination displays.
      display_info_list->erase(
          std::remove_if(display_info_list->begin(), display_info_list->end(),
                         [source_id](const ManagedDisplayInfo& info) {
                           return info.id() != source_id;
                         }),
          display_info_list->end());
      mirroring_source_id_ = source_id;
      break;
    }
    case UNIFIED:
      CreateUnifiedDesktopDisplayInfo(display_info_list);
      break;

    case EXTENDED:
      break;
  }
}

void DisplayManager::CreateUnifiedDesktopDisplayInfo(
    DisplayInfoList* display_info_list) {
  if (display_info_list->size() == 1)
    return;

  if (!ValidateMatrix(current_unified_desktop_matrix_) ||
      !ValidateMatrixForDisplayInfoList(*display_info_list,
                                        current_unified_desktop_matrix_)) {
    // Recreate the default matrix where displays are laid out horizontally from
    // left to right.
    current_unified_desktop_matrix_.clear();
    current_unified_desktop_matrix_.resize(1);
    for (const auto& info : *display_info_list)
      current_unified_desktop_matrix_[0].emplace_back(info.id());
  }

  software_mirroring_display_list_.clear();
  mirroring_display_id_to_unified_matrix_row_.clear();
  unified_display_rows_heights_.clear();

  const size_t num_rows = current_unified_desktop_matrix_.size();
  const size_t num_columns = current_unified_desktop_matrix_[0].size();

  // 1 - Find the maximum height per each row.
  std::vector<int> rows_max_heights;
  rows_max_heights.reserve(num_rows);
  for (const auto& row : current_unified_desktop_matrix_) {
    int max_height = std::numeric_limits<int>::min();
    for (const auto& id : row) {
      const ManagedDisplayInfo* info = FindInfoById(*display_info_list, id);
      DCHECK(info);

      max_height = std::max(max_height, info->size_in_pixel().height());
    }
    rows_max_heights.emplace_back(max_height);
  }

  // 2 - Use the maximum height per each row to calculate the scale value for
  //     each display in each row so that it fits the max row height. Use that
  //     to calculate the total bounds of each row after all displays has been
  //     scaled.

  // Holds the scale value of each display in the matrix.
  std::vector<std::vector<float>> scales;
  scales.resize(num_rows);

  // Holds the total bounds of each row in the matrix.
  std::vector<gfx::Rect> rows_bounds;
  rows_bounds.reserve(num_rows);

  // Calculate the bounds of each row, and the maximum row width.
  int max_total_width = std::numeric_limits<int>::min();
  for (size_t i = 0; i < num_rows; ++i) {
    const auto& row = current_unified_desktop_matrix_[i];
    const int max_row_height = rows_max_heights[i];
    gfx::Rect this_row_bounds;
    scales[i].resize(num_columns);
    for (size_t j = 0; j < num_columns; ++j) {
      const auto& id = row[j];
      const ManagedDisplayInfo* info = FindInfoById(*display_info_list, id);
      DCHECK(info);

      InsertAndUpdateDisplayInfo(*info);
      const float scale =
          info->size_in_pixel().height() / static_cast<float>(max_row_height);
      scales[i][j] = scale;

      const gfx::Point origin(this_row_bounds.right(), 0);
      const auto display_bounds = gfx::Rect(
          origin, gfx::ScaleToFlooredSize(info->size_in_pixel(), 1.0f / scale));
      this_row_bounds.Union(display_bounds);
    }
    rows_bounds.emplace_back(this_row_bounds);
    max_total_width = std::max(max_total_width, this_row_bounds.width());
  }

  // 3 - Using the maximum row width, adjust the display scales so that each
  //     row width fits the maximum row width.
  for (size_t i = 0; i < num_rows; ++i) {
    const auto& row_bound = rows_bounds[i];
    const float scale = row_bound.width() / static_cast<float>(max_total_width);
    auto& row_scales = scales[i];
    for (auto& display_scale : row_scales)
      display_scale *= scale;
  }

  // 4 - Now that we know the final scales, compute the unified display size by
  //     computing the unified display size of each row and then getting the
  //     union of all rows.
  gfx::Rect unified_bounds;  // Will hold the final unified bounds.
  std::vector<UnifiedDisplayModeParam> modes_param_list;
  modes_param_list.reserve(num_rows * num_columns);
  int internal_display_index = -1;
  for (size_t i = 0; i < num_rows; ++i) {
    const auto& row = current_unified_desktop_matrix_[i];
    gfx::Rect row_displays_bounds;
    for (size_t j = 0; j < num_columns; ++j) {
      const auto& id = row[j];
      if (internal_display_index == -1 && Display::IsInternalDisplayId(id))
        internal_display_index = i * num_columns + j;

      const ManagedDisplayInfo* info = FindInfoById(*display_info_list, id);
      DCHECK(info);

      const float scale = scales[i][j];
      const gfx::Point origin(row_displays_bounds.right(),
                              unified_bounds.bottom());
      // The display is scaled to fit the unified desktop size.
      Display display =
          CreateMirroringDisplayFromDisplayInfoById(id, origin, 1.0f / scale);

      row_displays_bounds.Union(display.bounds());
      modes_param_list.emplace_back(info->device_scale_factor(), scale, false);
      software_mirroring_display_list_.emplace_back(display);
    }

    unified_bounds.Union(row_displays_bounds);
  }

  // The index of the display that will be used for the default native mode.
  const int default_mode_param_index =
      internal_display_index != -1 ? internal_display_index : 0;
  modes_param_list[default_mode_param_index].is_default_mode = true;

  // 5 - Create the Unified display info and its modes.
  ManagedDisplayInfo unified_display_info(kUnifiedDisplayId, "Unified Desktop",
                                          false /* has_overscan */);
  ManagedDisplayMode native_mode(unified_bounds.size(), 60.0f, false, true, 1.0,
                                 1.0);
  ManagedDisplayInfo::ManagedDisplayModeList modes =
      CreateUnifiedManagedDisplayModeList(native_mode, modes_param_list);

  // Find the default mode.
  auto default_mode_iter =
      std::find_if(modes.begin(), modes.end(),
                   [](const ManagedDisplayMode mode) { return mode.native(); });
  DCHECK(default_mode_iter != modes.end());

  if (default_mode_iter != modes.end()) {
    const ManagedDisplayMode& default_mode = *default_mode_iter;
    unified_display_info.set_device_scale_factor(
        default_mode.device_scale_factor());
    unified_display_info.SetBounds(gfx::Rect(default_mode.size()));
  }

  unified_display_info.SetManagedDisplayModes(modes);

  // Forget the configured resolution if the original unified desktop resolution
  // has changed.
  if (display_info_.count(kUnifiedDisplayId) != 0 &&
      GetMaxNativeSize(display_info_[kUnifiedDisplayId]) !=
          unified_bounds.size()) {
    display_modes_.erase(kUnifiedDisplayId);
  }

  // 6 - Set the selected mode.
  ManagedDisplayMode selected_mode;
  if (GetSelectedModeForDisplayId(kUnifiedDisplayId, &selected_mode) &&
      FindDisplayMode(unified_display_info, selected_mode) !=
          unified_display_info.display_modes().end()) {
    unified_display_info.set_device_scale_factor(
        selected_mode.device_scale_factor());
    unified_display_info.SetBounds(gfx::Rect(selected_mode.size()));
  } else {
    display_modes_.erase(kUnifiedDisplayId);
  }

  const float unified_bounds_scale_y =
      unified_display_info.size_in_pixel().height() /
      static_cast<float>(unified_bounds.size().height());

  // 7 - Now that we know the final unified display bounds, update the displays
  //     in the |software_mirroring_display_list_| list so that they have the
  //     correct bounds.
  DCHECK_EQ(num_rows * num_columns, software_mirroring_display_list_.size());
  int last_bottom = 0;
  for (size_t i = 0; i < num_rows; ++i) {
    int last_right = 0;
    int max_height = std::numeric_limits<int>::min();
    for (size_t j = 0; j < num_columns; ++j) {
      Display& current_display =
          software_mirroring_display_list_[i * num_columns + j];
      gfx::SizeF scaled_size(current_display.bounds().size());
      scaled_size.Scale(unified_bounds_scale_y);
      const gfx::Point origin(last_right, last_bottom);
      current_display.set_bounds(
          gfx::Rect(origin, gfx::ToRoundedSize(scaled_size)));
      current_display.UpdateWorkAreaFromInsets(gfx::Insets());
      const gfx::Rect display_bounds = current_display.bounds();
      max_height = std::max(max_height, display_bounds.height());
      last_right = display_bounds.right();
      mirroring_display_id_to_unified_matrix_row_[current_display.id()] = i;
    }

    unified_display_rows_heights_.emplace_back(max_height);
    last_bottom += max_height;
  }

  DCHECK_EQ(num_rows, unified_display_rows_heights_.size());

  display_info_list->clear();
  display_info_list->emplace_back(unified_display_info);
  InsertAndUpdateDisplayInfo(unified_display_info);
}

Display* DisplayManager::FindDisplayForId(int64_t id) {
  auto iter =
      std::find_if(active_display_list_.begin(), active_display_list_.end(),
                   [id](const Display& display) { return display.id() == id; });
  if (iter != active_display_list_.end())
    return &(*iter);
  // TODO(oshima): This happens when windows in unified desktop have
  // been moved to a normal window. Fix this.
  if (id != kUnifiedDisplayId)
    DLOG(ERROR) << "Could not find display:" << id;
  return nullptr;
}

void DisplayManager::AddMirrorDisplayInfoIfAny(
    DisplayInfoList* display_info_list) {
  if (!IsInSoftwareMirrorMode())
    return;
  for (const auto& display : software_mirroring_display_list_)
    display_info_list->emplace_back(GetDisplayInfo(display.id()));
  software_mirroring_display_list_.clear();
}

void DisplayManager::InsertAndUpdateDisplayInfo(
    const ManagedDisplayInfo& new_info) {
  std::map<int64_t, ManagedDisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end()) {
    info->second.Copy(new_info);
  } else {
    display_info_[new_info.id()] = new_info;
    display_info_[new_info.id()].set_native(false);
    // FHD with 1.25 DSF behaves differently from other configuration.
    // It uses 1.25 DSF only when UI-Scale is set to 0.8.
    // For new users, use the UI-scale to 0.8 so that it will use DSF=1.25
    // internally.
    if (Display::IsInternalDisplayId(new_info.id()) &&
        new_info.bounds_in_native().height() == 1080 &&
        new_info.device_scale_factor() == 1.25f) {
      display_info_[new_info.id()].set_configured_ui_scale(0.8f);
    }
  }
  display_info_[new_info.id()].UpdateDisplaySize();
}

Display DisplayManager::CreateDisplayFromDisplayInfoById(int64_t id) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const ManagedDisplayInfo& display_info = display_info_[id];

  Display new_display(display_info.id());
  gfx::Rect bounds_in_native(display_info.size_in_pixel());
  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the bounds of non-primary display(s) will be updated
  // in |UpdateNonPrimaryDisplayBoundsForLayout| called in |UpdateDisplay|.
  new_display.SetScaleAndBounds(device_scale_factor,
                                gfx::Rect(bounds_in_native.size()));
  new_display.set_rotation(display_info.GetActiveRotation());
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
#if defined(OS_CHROMEOS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kUseMonitorColorSpace))
    new_display.set_color_space(display_info.color_space());
#else
  new_display.set_color_space(display_info.color_space());
#endif

  if (internal_display_has_accelerometer_ && Display::IsInternalDisplayId(id)) {
    new_display.set_accelerometer_support(
        Display::ACCELEROMETER_SUPPORT_AVAILABLE);
  } else {
    new_display.set_accelerometer_support(
        Display::ACCELEROMETER_SUPPORT_UNAVAILABLE);
  }
  return new_display;
}

Display DisplayManager::CreateMirroringDisplayFromDisplayInfoById(
    int64_t id,
    const gfx::Point& origin,
    float scale) {
  DCHECK(display_info_.find(id) != display_info_.end()) << "id=" << id;
  const ManagedDisplayInfo& display_info = display_info_[id];

  Display new_display(display_info.id());
  new_display.SetScaleAndBounds(
      1.0f, gfx::Rect(origin, gfx::ScaleToFlooredSize(
                                  display_info.size_in_pixel(), scale)));
  new_display.set_touch_support(display_info.touch_support());
  new_display.set_maximum_cursor_size(display_info.maximum_cursor_size());
  return new_display;
}

void DisplayManager::UpdateNonPrimaryDisplayBoundsForLayout(
    Displays* display_list,
    std::vector<size_t>* updated_indices) {
  if (display_list->size() == 1u)
    return;

  const DisplayLayout& layout = layout_store_->GetRegisteredDisplayLayout(
      CreateDisplayIdList(*display_list));

  // Ignore if a user has a old format (should be extremely rare)
  // and this will be replaced with DCHECK.
  if (layout.primary_id == kInvalidDisplayId)
    return;

  // display_list does not have translation set, so ApplyDisplayLayout cannot
  // provide accurate change information. We'll find the changes after the call.
  current_resolved_layout_ = layout.Copy();
  ApplyDisplayLayout(current_resolved_layout_.get(), display_list, nullptr);
  size_t num_displays = display_list->size();
  for (size_t index = 0; index < num_displays; ++index) {
    const Display& display = (*display_list)[index];
    int64_t id = display.id();
    const Display* active_display = FindDisplayForId(id);
    if (!active_display || (active_display->bounds() != display.bounds()))
      updated_indices->push_back(index);
  }
}

void DisplayManager::CreateMirrorWindowIfAny() {
  if (software_mirroring_display_list_.empty() || !delegate_) {
    if (!created_mirror_window_.is_null())
      base::ResetAndReturn(&created_mirror_window_).Run();
    return;
  }
  DisplayInfoList list;
  for (auto& display : software_mirroring_display_list_)
    list.push_back(GetDisplayInfo(display.id()));
  delegate_->CreateOrUpdateMirroringDisplay(list);
  if (!created_mirror_window_.is_null())
    base::ResetAndReturn(&created_mirror_window_).Run();
}

void DisplayManager::ApplyDisplayLayout(DisplayLayout* layout,
                                        Displays* display_list,
                                        std::vector<int64_t>* updated_ids) {
  if (multi_display_mode_ == UNIFIED) {
    // Applying the layout in unified mode doesn't make sense, since there's no
    // layout.
    return;
  }

  layout->ApplyToDisplayList(display_list, updated_ids,
                             kMinimumOverlapForInvalidOffset);
}

void DisplayManager::RunPendingTasksForTest() {
  if (software_mirroring_display_list_.empty())
    return;

  base::RunLoop run_loop;
  created_mirror_window_ = run_loop.QuitClosure();
  run_loop.Run();
}

void DisplayManager::NotifyMetricsChanged(const Display& display,
                                          uint32_t metrics) {
  for (auto& observer : observers_)
    observer.OnDisplayMetricsChanged(display, metrics);
}

void DisplayManager::NotifyDisplayAdded(const Display& display) {
  for (auto& observer : observers_)
    observer.OnDisplayAdded(display);
}

void DisplayManager::NotifyDisplayRemoved(const Display& display) {
  for (auto& observer : observers_)
    observer.OnDisplayRemoved(display);
}

void DisplayManager::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void DisplayManager::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

const Display& DisplayManager::GetSecondaryDisplay() const {
  CHECK_LE(2U, GetNumDisplays());
  return GetDisplayAt(0).id() == Screen::GetScreen()->GetPrimaryDisplay().id()
             ? GetDisplayAt(1)
             : GetDisplayAt(0);
}

}  // namespace display
