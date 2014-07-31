// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/chromeos/native_display_delegate_dri.h"

#include "base/bind.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/display/types/chromeos/native_display_observer.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/ozone/platform/dri/chromeos/display_mode_dri.h"
#include "ui/ozone/platform/dri/chromeos/display_snapshot_dri.h"
#include "ui/ozone/platform/dri/dri_console_buffer.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/screen_manager.h"

namespace ui {

namespace {
const size_t kMaxDisplayCount = 2;
}  // namespace

NativeDisplayDelegateDri::NativeDisplayDelegateDri(
    DriWrapper* dri,
    ScreenManager* screen_manager,
    DeviceManager* device_manager)
    : dri_(dri),
      screen_manager_(screen_manager),
      device_manager_(device_manager) {
}

NativeDisplayDelegateDri::~NativeDisplayDelegateDri() {
  if (device_manager_)
    device_manager_->RemoveObserver(this);
}

DisplaySnapshot* NativeDisplayDelegateDri::FindDisplaySnapshot(int64_t id) {
  for (size_t i = 0; i < cached_displays_.size(); ++i)
    if (cached_displays_[i]->display_id() == id)
      return cached_displays_[i];

  return NULL;
}

const DisplayMode* NativeDisplayDelegateDri::FindDisplayMode(
    const gfx::Size& size,
    bool is_interlaced,
    float refresh_rate) {
  for (size_t i = 0; i < cached_modes_.size(); ++i)
    if (cached_modes_[i]->size() == size &&
        cached_modes_[i]->is_interlaced() == is_interlaced &&
        cached_modes_[i]->refresh_rate() == refresh_rate)
      return cached_modes_[i];

  return NULL;
}

void NativeDisplayDelegateDri::Initialize() {
  if (device_manager_)
    device_manager_->AddObserver(this);

  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(dri_->get_fd());

  // By default all displays show the same console buffer.
  console_buffer_.reset(
      new DriConsoleBuffer(dri_, displays[0]->crtc()->buffer_id));
  if (!console_buffer_->Initialize()) {
    VLOG(1) << "Failed to initialize console buffer";
    console_buffer_.reset();
  } else {
    // Clear the console buffer such that restarting Chrome will show a
    // predetermined background.
    //
    // Black was chosen since Chrome's first buffer paints start with a black
    // background.
    console_buffer_->canvas()->clear(SK_ColorBLACK);
  }
}

void NativeDisplayDelegateDri::GrabServer() {}

void NativeDisplayDelegateDri::UngrabServer() {}

void NativeDisplayDelegateDri::SyncWithServer() {}

void NativeDisplayDelegateDri::SetBackgroundColor(uint32_t color_argb) {
  if (console_buffer_)
    console_buffer_->canvas()->clear(color_argb);
}

void NativeDisplayDelegateDri::ForceDPMSOn() {
  for (size_t i = 0; i < cached_displays_.size(); ++i) {
    DisplaySnapshotDri* dri_output = cached_displays_[i];
    if (dri_output->dpms_property())
      dri_->SetProperty(dri_output->connector(),
                        dri_output->dpms_property()->prop_id,
                        DRM_MODE_DPMS_ON);
  }
}

std::vector<DisplaySnapshot*> NativeDisplayDelegateDri::GetDisplays() {
  ScopedVector<DisplaySnapshotDri> old_displays(cached_displays_.Pass());
  cached_modes_.clear();

  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(dri_->get_fd());
  for (size_t i = 0;
       i < displays.size() && cached_displays_.size() < kMaxDisplayCount; ++i) {
    DisplaySnapshotDri* display = new DisplaySnapshotDri(
        dri_, displays[i]->connector(), displays[i]->crtc(), i);
    cached_displays_.push_back(display);
    cached_modes_.insert(cached_modes_.end(),
                         display->modes().begin(),
                         display->modes().end());
  }

  NotifyScreenManager(cached_displays_.get(), old_displays.get());

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
          << " size=" << (mode ? mode->size().ToString() : "0x0");

  if (mode) {
    if (!screen_manager_->ConfigureDisplayController(
            dri_output.crtc(),
            dri_output.connector(),
            origin,
            static_cast<const DisplayModeDri*>(mode)->mode_info())) {
      VLOG(1) << "Failed to configure: crtc=" << dri_output.crtc()
              << " connector=" << dri_output.connector();
      return false;
    }
  } else {
    if (!screen_manager_->DisableDisplayController(dri_output.crtc())) {
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

void NativeDisplayDelegateDri::OnDeviceEvent(const DeviceEvent& event) {
  if (event.device_type() != DeviceEvent::DISPLAY)
    return;

  if (event.action_type() == DeviceEvent::CHANGE) {
    VLOG(1) << "Got display changed event";
    FOR_EACH_OBSERVER(
        NativeDisplayObserver, observers_, OnConfigurationChanged());
  }
}

void NativeDisplayDelegateDri::NotifyScreenManager(
    const std::vector<DisplaySnapshotDri*>& new_displays,
    const std::vector<DisplaySnapshotDri*>& old_displays) const {
  for (size_t i = 0; i < old_displays.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < new_displays.size(); ++j) {
      if (old_displays[i]->connector() == new_displays[j]->connector() &&
          old_displays[i]->crtc() == new_displays[j]->crtc()) {
        found = true;
        break;
      }
    }

    if (!found)
      screen_manager_->RemoveDisplayController(old_displays[i]->crtc());
  }
}

}  // namespace ui
