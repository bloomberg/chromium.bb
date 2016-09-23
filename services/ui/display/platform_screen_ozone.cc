// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen_ozone.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace display {
namespace {

// Needed for DisplayConfigurator::ForceInitialConfigure.
const SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);

const float kInchInMm = 25.4f;

float ComputeDisplayDPI(const gfx::Size& pixel_size,
                        const gfx::Size& physical_size) {
  DCHECK(!physical_size.IsEmpty());
  return (pixel_size.width() / static_cast<float>(physical_size.width())) *
         kInchInMm;
}

// Finds the device scale factor based on the display DPI. Will use forced
// device scale factor if provided via command line.
float FindDeviceScaleFactor(float dpi) {
  if (Display::HasForceDeviceScaleFactor())
    return Display::GetForcedDeviceScaleFactor();

  // TODO(kylechar): If dpi > 150 then ash uses 1.25 now. Ignoring that for now.
  if (dpi > 200.0)
    return 2.0f;
  else
    return 1.0f;
}

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::MakeUnique<PlatformScreenOzone>();
}

PlatformScreenOzone::PlatformScreenOzone() {}

PlatformScreenOzone::~PlatformScreenOzone() {
  // We are shutting down and don't want to make anymore display changes.
  fake_display_controller_ = nullptr;
  display_configurator_.RemoveObserver(this);
}

void PlatformScreenOzone::AddInterfaces(shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::DisplayController>(this);
}

void PlatformScreenOzone::Init(PlatformScreenDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  std::unique_ptr<ui::NativeDisplayDelegate> native_display_delegate =
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();

  if (!base::SysInfo::IsRunningOnChromeOS()) {
    fake_display_controller_ =
        native_display_delegate->GetFakeDisplayController();
  }

  // We want display configuration to happen even off device to keep the control
  // flow similar.
  display_configurator_.set_configure_display(true);
  display_configurator_.AddObserver(this);
  display_configurator_.Init(std::move(native_display_delegate), false);
  display_configurator_.ForceInitialConfigure(kChromeOsBootColor);
}

void PlatformScreenOzone::RequestCloseDisplay(int64_t display_id) {
  if (!fake_display_controller_ || wait_for_display_config_update_)
    return;

  CachedDisplayIterator iter = GetCachedDisplayIterator(display_id);
  if (iter != cached_displays_.end()) {
    // Tell the NDD to remove the display. PlatformScreen will get an update
    // that the display configuration has changed and the display will be gone.
    wait_for_display_config_update_ =
        fake_display_controller_->RemoveDisplay(iter->id);
  }
}

int64_t PlatformScreenOzone::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

void PlatformScreenOzone::ToggleVirtualDisplay() {
  if (!fake_display_controller_ || wait_for_display_config_update_)
    return;

  if (cached_displays_.size() == 1) {
    const gfx::Size& pixel_size = cached_displays_[0].pixel_size;
    wait_for_display_config_update_ =
        fake_display_controller_->AddDisplay(pixel_size) !=
        Display::kInvalidDisplayID;
  } else if (cached_displays_.size() > 1) {
    wait_for_display_config_update_ =
        fake_display_controller_->RemoveDisplay(cached_displays_.back().id);
  } else {
    NOTREACHED();
  }
}

void PlatformScreenOzone::ProcessRemovedDisplays(
    const ui::DisplayConfigurator::DisplayStateList& snapshots) {
  std::vector<int64_t> current_ids;
  for (ui::DisplaySnapshot* snapshot : snapshots)
    current_ids.push_back(snapshot->display_id());

  // Find cached displays with no matching snapshot and mark as removed.
  for (DisplayInfo& display : cached_displays_) {
    if (std::find(current_ids.begin(), current_ids.end(), display.id) ==
        current_ids.end()) {
      display.removed = true;
      if (primary_display_id_ == display.id)
        primary_display_id_ = Display::kInvalidDisplayID;
    }
  }

  // If the primary display was removed find a new primary display id.
  if (primary_display_id_ == Display::kInvalidDisplayID) {
    for (const DisplayInfo& display : cached_displays_) {
      if (!display.removed) {
        primary_display_id_ = display.id;
        break;
      }
    }
  }
}

void PlatformScreenOzone::ProcessModifiedDisplays(
    const ui::DisplayConfigurator::DisplayStateList& snapshots) {
  for (ui::DisplaySnapshot* snapshot : snapshots) {
    auto iter = GetCachedDisplayIterator(snapshot->display_id());
    if (iter != cached_displays_.end()) {
      DisplayInfo& display_info = *iter;
      DisplayInfo new_info = DisplayInfoFromSnapshot(*snapshot);

      if (new_info.bounds.size() != display_info.bounds.size() ||
          new_info.device_scale_factor != display_info.device_scale_factor) {
        display_info = new_info;
        display_info.modified = true;
      }
    }
  }
}

void PlatformScreenOzone::UpdateCachedDisplays() {
  // Walk through cached displays after processing the snapshots to find any
  // removed or modified displays. This ensures that we only send one update per
  // display to the delegate.
  next_display_origin_.SetPoint(0, 0);
  for (auto iter = cached_displays_.begin(); iter != cached_displays_.end();) {
    DisplayInfo& display_info = *iter;
    if (display_info.removed) {
      // Update delegate and remove from cache.
      delegate_->OnDisplayRemoved(display_info.id);
      iter = cached_displays_.erase(iter);
    } else {
      // Check if the display origin needs to be updated.
      if (next_display_origin_ != display_info.bounds.origin()) {
        display_info.bounds.set_origin(next_display_origin_);
        display_info.modified = true;
      }
      next_display_origin_.Offset(display_info.bounds.width(), 0);

      // Check if the window bounds have changed and update delegate.
      if (display_info.modified) {
        display_info.modified = false;
        delegate_->OnDisplayModified(display_info.id, display_info.bounds,
                                     display_info.pixel_size,
                                     display_info.device_scale_factor);
      }
      ++iter;
    }
  }
}

void PlatformScreenOzone::AddNewDisplays(
    const ui::DisplayConfigurator::DisplayStateList& snapshots) {
  for (ui::DisplaySnapshot* snapshot : snapshots) {
    const int64_t id = snapshot->display_id();

    // Check if display already exists and skip.
    if (GetCachedDisplayIterator(id) != cached_displays_.end())
      continue;

    // If we have no primary display then this one should be it.
    if (primary_display_id_ == Display::kInvalidDisplayID)
      primary_display_id_ = id;

    DisplayInfo display_info = DisplayInfoFromSnapshot(*snapshot);

    // Move the origin so that next display is to the right of current display.
    next_display_origin_.Offset(display_info.bounds.width(), 0);

    cached_displays_.push_back(display_info);
    delegate_->OnDisplayAdded(display_info.id, display_info.bounds,
                              display_info.pixel_size,
                              display_info.device_scale_factor);
  }
}

PlatformScreenOzone::CachedDisplayIterator
PlatformScreenOzone::GetCachedDisplayIterator(int64_t display_id) {
  return std::find_if(cached_displays_.begin(), cached_displays_.end(),
                      [display_id](const DisplayInfo& display_info) {
                        return display_info.id == display_id;
                      });
}

PlatformScreenOzone::DisplayInfo PlatformScreenOzone::DisplayInfoFromSnapshot(
    const ui::DisplaySnapshot& snapshot) {
  const ui::DisplayMode* current_mode = snapshot.current_mode();
  DCHECK(current_mode);

  DisplayInfo display_info;
  display_info.id = snapshot.display_id();
  display_info.pixel_size = current_mode->size();
  display_info.device_scale_factor = FindDeviceScaleFactor(
      ComputeDisplayDPI(current_mode->size(), snapshot.physical_size()));
  // Get DIP size based on device scale factor. We are assuming the
  // ui scale factor is always 1.0 here for now.
  gfx::Size scaled_size = gfx::ScaleToRoundedSize(
      current_mode->size(), 1.0f / display_info.device_scale_factor);
  display_info.bounds = gfx::Rect(next_display_origin_, scaled_size);
  return display_info;
}

void PlatformScreenOzone::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& displays) {
  ProcessRemovedDisplays(displays);
  ProcessModifiedDisplays(displays);
  UpdateCachedDisplays();
  AddNewDisplays(displays);
  wait_for_display_config_update_ = false;
}

void PlatformScreenOzone::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState failed_new_state) {
  LOG(ERROR) << "OnDisplayModeChangeFailed from DisplayConfigurator";
  wait_for_display_config_update_ = false;
}

void PlatformScreenOzone::Create(const shell::Identity& remote_identity,
                                 mojom::DisplayControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace display
