// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen_impl_ozone.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
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
  return base::MakeUnique<PlatformScreenImplOzone>();
}

PlatformScreenImplOzone::PlatformScreenImplOzone() {}

PlatformScreenImplOzone::~PlatformScreenImplOzone() {
  display_configurator_.RemoveObserver(this);
}

void PlatformScreenImplOzone::Init() {
  // We want display configuration to happen even off device to keep the control
  // flow similar.
  display_configurator_.set_configure_display(true);
  display_configurator_.AddObserver(this);
  display_configurator_.Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(), false);
}

void PlatformScreenImplOzone::ConfigurePhysicalDisplay(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
  callback_ = callback;

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

int64_t PlatformScreenImplOzone::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

void PlatformScreenImplOzone::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& displays) {
  if (displays.size() > 1) {
    LOG(ERROR)
        << "Mus doesn't really support multiple displays, expect it to crash";
  }

  // TODO(kylechar): Use DisplayLayout/DisplayLayoutStore here when possible.
  std::set<uint64_t> all_displays;
  for (ui::DisplaySnapshot* display : displays) {
    const int64_t id = display->display_id();

    all_displays.insert(id);

    if (displays_.find(id) != displays_.end())
      continue;

    const ui::DisplayMode* current_mode = display->current_mode();
    gfx::Rect bounds(next_display_origin_, current_mode->size());

    // Move the origin so that next display is to the right of current display.
    next_display_origin_.Offset(current_mode->size().width(), 0);

    // The first display added will be our primary display.
    if (displays_.empty())
      primary_display_id_ = id;

    // Keep track of what displays have already been added.
    displays_.insert(display->display_id());

    callback_.Run(id, bounds);
  }

  DCHECK(displays_ == all_displays) << "Removing displays is not supported.";
}

void PlatformScreenImplOzone::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    ui::MultipleDisplayState failed_new_state) {
  LOG(ERROR) << "OnDisplayModeChangeFailed from DisplayConfigurator";
}

}  // namespace display
