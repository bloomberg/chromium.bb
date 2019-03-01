// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include <stddef.h>
#include <utility>

#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

class DisplayComparator {
 public:
  explicit DisplayComparator(const DrmDisplay* display)
      : drm_(display->drm()),
        crtc_(display->crtc()),
        connector_(display->connector()) {}

  DisplayComparator(const scoped_refptr<DrmDevice>& drm,
                    uint32_t crtc,
                    uint32_t connector)
      : drm_(drm), crtc_(crtc), connector_(connector) {}

  bool operator()(const std::unique_ptr<DrmDisplay>& other) const {
    return drm_ == other->drm() && connector_ == other->connector() &&
           crtc_ == other->crtc();
  }

 private:
  scoped_refptr<DrmDevice> drm_;
  uint32_t crtc_;
  uint32_t connector_;
};

bool MatchMode(const display::DisplayMode& display_mode,
               const drmModeModeInfo& m) {
  return display_mode.size() == ModeSize(m) &&
         display_mode.refresh_rate() == ModeRefreshRate(m) &&
         display_mode.is_interlaced() == ModeIsInterlaced(m);
}

bool FindMatchingMode(const std::vector<drmModeModeInfo> modes,
                      const display::DisplayMode& display_mode,
                      drmModeModeInfo* mode) {
  for (const drmModeModeInfo& m : modes) {
    if (MatchMode(display_mode, m)) {
      *mode = m;
      return true;
    }
  }
  return false;
}

}  // namespace

DrmGpuDisplayManager::DrmGpuDisplayManager(ScreenManager* screen_manager,
                                           DrmDeviceManager* drm_device_manager)
    : screen_manager_(screen_manager), drm_device_manager_(drm_device_manager) {
}

DrmGpuDisplayManager::~DrmGpuDisplayManager() {
}

MovableDisplaySnapshots DrmGpuDisplayManager::GetDisplays() {
  MovableDisplaySnapshots params_list;
  std::vector<std::unique_ptr<DrmDisplay>> old_displays;
  old_displays.swap(displays_);
  HardwareDisplayControllerInfos old_hardware_infos;
  old_hardware_infos.swap(hardware_infos_);

  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  size_t device_index = 0;
  for (const auto& drm : devices) {
    bool support_all_displays;
    auto display_infos = QueryAvailableDisplayControllerInfos(
        drm->get_fd(), &support_all_displays);

    // Reallocate crtc with following strategy:
    // (1) if there exists any display without associated crtc, the most
    // recently connected display is denied the allocated crtc. Then the
    // remaining displays are configured with previously assigned crtc which is
    // recorded in |old_hardware_infos|. (2) if there are sufficient crtcs for
    // each display, do the normal configuration.
    if (!support_all_displays) {
      for (auto& display_info : display_infos) {
        // Connector id is unique. For each connected display,
        // check whether it is recently connected with device or not.
        auto hardware_info_it = std::find_if(
            old_hardware_infos.begin(), old_hardware_infos.end(),
            [&display_info](
                std::unique_ptr<HardwareDisplayControllerInfo>& hardware_info) {
              return display_info->connector()->connector_id ==
                     hardware_info->connector()->connector_id;
            });

        if (hardware_info_it == old_hardware_infos.end()) {
          // |display_info| corresponds to the most recently connected display.
          display_info->set_crtc(nullptr);
        } else {
          // |display_info| corresponds to the display which has been connected
          // with device before.
          auto display_it =
              std::find_if(old_displays.begin(), old_displays.end(),
                           DisplayComparator(
                               drm, (*hardware_info_it)->crtc()->crtc_id,
                               (*hardware_info_it)->connector()->connector_id));
          DCHECK(display_it != old_displays.end());
          displays_.push_back(std::move(*display_it));
          old_displays.erase(display_it);
          display_info->set_crtc((*hardware_info_it)->release_crtc());
          old_hardware_infos.erase(hardware_info_it);
        }
      }
    } else {
      for (auto& display_info : display_infos) {
        auto it = std::find_if(
            old_displays.begin(), old_displays.end(),
            DisplayComparator(drm, display_info->crtc()->crtc_id,
                              display_info->connector()->connector_id));
        if (it != old_displays.end()) {
          displays_.push_back(std::move(*it));
          old_displays.erase(it);
        } else {
          displays_.push_back(
              std::make_unique<DrmDisplay>(screen_manager_, drm));
        }
      }
    }

    MovableDisplaySnapshots sub_params_list =
        GenerateParamsList(display_infos, device_index);
    std::move(sub_params_list.begin(), sub_params_list.end(),
              std::back_inserter(params_list));
    device_index++;
  }

  NotifyScreenManager(displays_, old_displays);
  return params_list;
}

bool DrmGpuDisplayManager::TakeDisplayControl() {
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  bool status = true;
  for (const auto& drm : devices)
    status &= drm->SetMaster();

  // Roll-back any successful operation.
  if (!status) {
    LOG(ERROR) << "Failed to take control of the display";
    RelinquishDisplayControl();
  }

  return status;
}

void DrmGpuDisplayManager::RelinquishDisplayControl() {
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  for (const auto& drm : devices)
    drm->DropMaster();
}

bool DrmGpuDisplayManager::ConfigureDisplay(
    int64_t display_id,
    const display::DisplayMode& display_mode,
    const gfx::Point& origin) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return false;
  }

  drmModeModeInfo mode;
  bool mode_found = FindMatchingMode(display->modes(), display_mode, &mode);
  if (!mode_found) {
    // If the display doesn't have the mode natively, then lookup the mode from
    // other displays and try using it on the current display (some displays
    // support panel fitting and they can use different modes even if the mode
    // isn't explicitly declared).
    for (const auto& other_display : displays_) {
      mode_found =
          FindMatchingMode(other_display->modes(), display_mode, &mode);
      if (mode_found)
        break;
    }
  }

  if (!mode_found) {
    LOG(ERROR) << "Failed to find mode: size=" << display_mode.size().ToString()
               << " is_interlaced=" << display_mode.is_interlaced()
               << " refresh_rate=" << display_mode.refresh_rate();
    return false;
  }

  return display->Configure(&mode, origin);
}

bool DrmGpuDisplayManager::DisableDisplay(int64_t display_id) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return false;
  }

  return display->Configure(nullptr, gfx::Point());
}

bool DrmGpuDisplayManager::GetHDCPState(int64_t display_id,
                                        display::HDCPState* state) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return false;
  }

  return display->GetHDCPState(state);
}

bool DrmGpuDisplayManager::SetHDCPState(int64_t display_id,
                                        display::HDCPState state) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return false;
  }

  return display->SetHDCPState(state);
}

void DrmGpuDisplayManager::SetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return;
  }

  display->SetColorMatrix(color_matrix);
}

void DrmGpuDisplayManager::SetBackgroundColor(
    int64_t display_id,
    const uint64_t background_color) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID" << display_id;
    return;
  }

  display->SetBackgroundColor(background_color);
}

void DrmGpuDisplayManager::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  DrmDisplay* display = FindDisplay(display_id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return;
  }
  display->SetGammaCorrection(degamma_lut, gamma_lut);
}

HardwareDisplayControllerInfos
DrmGpuDisplayManager::QueryAvailableDisplayControllerInfos(
    int fd,
    bool* support_all_displays) const {
  return GetAvailableDisplayControllerInfos(fd, support_all_displays);
}

MovableDisplaySnapshots DrmGpuDisplayManager::GenerateParamsList(
    HardwareDisplayControllerInfos& display_infos,
    size_t device_index) {
  MovableDisplaySnapshots params_list;
  auto drm = drm_device_manager_->GetDrmDevices()[device_index];

  // Generate |param_list| with updated |displays_|. Notice that the display
  // connection without crtc allocated is not included in |displays_| but
  // contained in |params_list|. Because Chrome side should be notified of it.
  size_t display_index = 0;
  for (auto& display_info : display_infos) {
    if (display_info->has_associated_crtc()) {
      params_list.push_back(
          displays_[display_index++]->Update(display_info.get(), device_index));
      hardware_infos_.push_back(std::move(display_info));
    } else {
      params_list.push_back(std::make_unique<DrmDisplay>(screen_manager_, drm)
                                ->Update(display_info.get(), device_index));
    }
  }

  return params_list;
}

void DrmGpuDisplayManager::NotifyScreenManager(
    const std::vector<std::unique_ptr<DrmDisplay>>& new_displays,
    const std::vector<std::unique_ptr<DrmDisplay>>& old_displays) const {
  for (const auto& old_display : old_displays) {
    auto it = std::find_if(new_displays.begin(), new_displays.end(),
                           DisplayComparator(old_display.get()));

    if (it == new_displays.end()) {
      screen_manager_->RemoveDisplayController(old_display->drm(),
                                               old_display->crtc());
    }
  }

  for (const auto& new_display : new_displays) {
    auto it = std::find_if(old_displays.begin(), old_displays.end(),
                           DisplayComparator(new_display.get()));

    if (it == old_displays.end()) {
      screen_manager_->AddDisplayController(
          new_display->drm(), new_display->crtc(), new_display->connector());
    }
  }
}

DrmDisplay* DrmGpuDisplayManager::FindDisplay(int64_t display_id) {
  for (const auto& display : displays_) {
    if (display->display_id() == display_id)
      return display.get();
  }

  return nullptr;
}

}  // namespace ui
