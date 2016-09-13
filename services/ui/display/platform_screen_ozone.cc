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

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::MakeUnique<PlatformScreenOzone>();
}

PlatformScreenOzone::PlatformScreenOzone() {}

PlatformScreenOzone::~PlatformScreenOzone() {
  display_configurator_.RemoveObserver(this);
}

void PlatformScreenOzone::AddInterfaces(shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::DisplayController>(this);
}

void PlatformScreenOzone::Init(PlatformScreenDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;

  // We want display configuration to happen even off device to keep the control
  // flow similar.
  display_configurator_.set_configure_display(true);
  display_configurator_.AddObserver(this);
  display_configurator_.Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(), false);

  if (base::SysInfo::IsRunningOnChromeOS()) {
    display_configurator_.ForceInitialConfigure(kChromeOsBootColor);
  } else {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("multi-display")) {
      // This really doesn't work properly. Use at your own risk.
      display_configurator_.AddVirtualDisplay(gfx::Size(800, 800));
      display_configurator_.AddVirtualDisplay(gfx::Size(800, 800));
    } else {
      display_configurator_.AddVirtualDisplay(gfx::Size(1024, 768));
    }
  }
}

int64_t PlatformScreenOzone::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

void PlatformScreenOzone::ToggleVirtualDisplay() {
  if (base::SysInfo::IsRunningOnChromeOS())
    return;

  // TODO(kylechar): Convert to use VirtualDisplayDelegate once landed.
  if (cached_displays_.size() == 1) {
    display_configurator_.AddVirtualDisplay(cached_displays_[0].bounds.size());
  } else if (cached_displays_.size() > 1) {
    display_configurator_.RemoveVirtualDisplay(cached_displays_.back().id);
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
        primary_display_id_ = display::Display::kInvalidDisplayID;
    }
  }

  // If the primary display was removed find a new primary display id.
  if (primary_display_id_ == display::Display::kInvalidDisplayID) {
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
      const ui::DisplayMode* current_mode = snapshot->current_mode();
      if (current_mode->size() != display_info.bounds.size()) {
        display_info.bounds.set_size(current_mode->size());
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
        delegate_->OnDisplayModified(display_info.id, display_info.bounds);
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

    const ui::DisplayMode* current_mode = snapshot->current_mode();
    gfx::Rect bounds(next_display_origin_, current_mode->size());

    // Move the origin so that next display is to the right of current display.
    next_display_origin_.Offset(current_mode->size().width(), 0);

    // If we have no primary display then this one should be it.
    if (primary_display_id_ == display::Display::kInvalidDisplayID)
      primary_display_id_ = id;

    cached_displays_.push_back(DisplayInfo(id, bounds));
    delegate_->OnDisplayAdded(id, bounds);
  }
}

PlatformScreenOzone::CachedDisplayIterator
PlatformScreenOzone::GetCachedDisplayIterator(int64_t display_id) {
  return std::find_if(cached_displays_.begin(), cached_displays_.end(),
                      [display_id](const DisplayInfo& display_info) {
                        return display_info.id == display_id;
                      });
}

void PlatformScreenOzone::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& displays) {
  ProcessRemovedDisplays(displays);
  ProcessModifiedDisplays(displays);
  UpdateCachedDisplays();
  AddNewDisplays(displays);
}

void PlatformScreenOzone::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState failed_new_state) {
  LOG(ERROR) << "OnDisplayModeChangeFailed from DisplayConfigurator";
}

void PlatformScreenOzone::Create(const shell::Identity& remote_identity,
                                 mojom::DisplayControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace display
