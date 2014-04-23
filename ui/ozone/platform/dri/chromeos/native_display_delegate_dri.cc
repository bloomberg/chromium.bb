// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"

#include "ui/ozone/platform/dri/chromeos/display_mode_dri.h"
#include "ui/ozone/platform/dri/chromeos/display_snapshot_dri.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {
const size_t kMaxDisplayCount = 2;
}  // namespace

NativeDisplayDelegateDri::NativeDisplayDelegateDri(
    DriSurfaceFactory* surface_factory)
    : surface_factory_(surface_factory) {}

NativeDisplayDelegateDri::~NativeDisplayDelegateDri() {}

void NativeDisplayDelegateDri::Initialize() {
  gfx::SurfaceFactoryOzone::HardwareState state =
      surface_factory_->InitializeHardware();

  CHECK_EQ(gfx::SurfaceFactoryOzone::INITIALIZED, state)
      << "Failed to initialize hardware";
}

void NativeDisplayDelegateDri::GrabServer() {}

void NativeDisplayDelegateDri::UngrabServer() {}

void NativeDisplayDelegateDri::SyncWithServer() {}

void NativeDisplayDelegateDri::SetBackgroundColor(uint32_t color_argb) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateDri::ForceDPMSOn() {
  for (size_t i = 0; i < cached_displays_.size(); ++i) {
    DisplaySnapshotDri* dri_output = cached_displays_[i];
    if (dri_output->dpms_property())
      surface_factory_->drm()->SetProperty(
          dri_output->connector(),
          dri_output->dpms_property()->prop_id,
          DRM_MODE_DPMS_ON);
  }
}

std::vector<DisplaySnapshot*> NativeDisplayDelegateDri::GetDisplays() {
  cached_displays_.clear();
  cached_modes_.clear();

  drmModeRes* resources = drmModeGetResources(
      surface_factory_->drm()->get_fd());
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(
          surface_factory_->drm()->get_fd(),
          resources);
  for (size_t i = 0;
       i < displays.size() && cached_displays_.size() < kMaxDisplayCount; ++i) {
    DisplaySnapshotDri* display = new DisplaySnapshotDri(
        surface_factory_->drm(),
        displays[i]->connector(),
        displays[i]->crtc(),
        i);
    cached_displays_.push_back(display);
    // Modes can be shared between different displays, so we need to keep track
    // of them independently for cleanup.
    cached_modes_.insert(cached_modes_.end(),
                         display->modes().begin(),
                         display->modes().end());
  }

  drmModeFreeResources(resources);

  std::vector<DisplaySnapshot*> generic_displays(cached_displays_.begin(),
                                                 cached_displays_.end());
  return generic_displays;
}

void NativeDisplayDelegateDri::AddMode(const DisplaySnapshot& output,
                                       const DisplayMode* mode) {}

bool NativeDisplayDelegateDri::Configure(const DisplaySnapshot& output,
                                         const DisplayMode* mode,
                                         const gfx::Point& origin) {
  const DisplaySnapshotDri& dri_output =
      static_cast<const DisplaySnapshotDri&>(output);

  VLOG(1) << "DRM configuring: crtc=" << dri_output.crtc()
          << " connector=" << dri_output.connector()
          << " origin=" << origin.ToString()
          << " size=" << mode->size().ToString();

  if (mode) {
    if (!surface_factory_->CreateHardwareDisplayController(
        dri_output.connector(),
        dri_output.crtc(),
        static_cast<const DisplayModeDri*>(mode)->mode_info())) {
      VLOG(1) << "Failed to configure: crtc=" << dri_output.crtc()
              << " connector=" << dri_output.connector();
      return false;
    }
  } else {
    if (!surface_factory_->DisableHardwareDisplayController(
        dri_output.crtc())) {
      VLOG(1) << "Failed to disable crtc=" << dri_output.crtc();
      return false;
    }
  }

  return true;
}

void NativeDisplayDelegateDri::CreateFrameBuffer(const gfx::Size& size) {}

bool NativeDisplayDelegateDri::GetHDCPState(const DisplaySnapshot& output,
                                            HDCPState* state) {
  NOTIMPLEMENTED();
  return false;
}

bool NativeDisplayDelegateDri::SetHDCPState(const DisplaySnapshot& output,
                                            HDCPState state) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<ui::ColorCalibrationProfile>
NativeDisplayDelegateDri::GetAvailableColorCalibrationProfiles(
    const ui::DisplaySnapshot& output) {
  NOTIMPLEMENTED();
  return std::vector<ui::ColorCalibrationProfile>();
}

bool NativeDisplayDelegateDri::SetColorCalibrationProfile(
    const ui::DisplaySnapshot& output,
    ui::ColorCalibrationProfile new_profile) {
  NOTIMPLEMENTED();
  return false;
}

void NativeDisplayDelegateDri::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void NativeDisplayDelegateDri::RemoveObserver(NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
