// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen_impl_ozone.h"

#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace display {
namespace {

// TODO(rjkroege): Remove this code once ozone headless has the same
// display creation semantics as ozone drm.
// Some ozone platforms do not configure physical displays and so do not
// callback into this class via the implementation of NativeDisplayObserver.
// FixedSizeScreenConfiguration() short-circuits the implementation of display
// configuration in this case by calling the |callback| provided to
// ConfigurePhysicalDisplay() with a hard-coded |id| and |bounds|.
void FixedSizeScreenConfiguration(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
  callback.Run(1, gfx::Rect(1024, 768));
}

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
  display_configurator_.AddObserver(this);
  display_configurator_.Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(), false);
}

void PlatformScreenImplOzone::ConfigurePhysicalDisplay(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    callback_ = callback;
    display_configurator_.ForceInitialConfigure(kChromeOsBootColor);
  } else {
    // PostTask()ed to maximize control flow similarity with the ChromeOS case.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&FixedSizeScreenConfiguration, callback));
  }
}

void PlatformScreenImplOzone::OnDisplayModeChanged(
    const ui::DisplayConfigurator::DisplayStateList& displays) {
  // TODO(kylechar): Remove checks when multiple display support is added.
  CHECK(displays.size() == 1) << "Mus only supports one 1 display";
  CHECK(!callback_.is_null());

  gfx::Point origin;
  for (auto display : displays) {
    const ui::DisplayMode* current_mode = display->current_mode();
    gfx::Rect bounds(origin, current_mode->size());

    callback_.Run(display->display_id(), bounds);

    // Move the origin so that next display is to the right of current display.
    origin.Offset(current_mode->size().width(), 0);
  }

  callback_.Reset();
}

void PlatformScreenImplOzone::OnDisplayModeChangeFailed(
    const ui::DisplayConfigurator::DisplayStateList& displays,
    MultipleDisplayState failed_new_state) {
  LOG(ERROR) << "OnDisplayModeChangeFailed from DisplayConfigurator";
  callback_.Reset();
}

}  // namespace display
}  // namespace ui
