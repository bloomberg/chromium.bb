// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_descriptor_posix.h"
#include "base/files/file.h"
#include "base/single_thread_task_runner.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/ozone/common/display_util.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_display_mode.h"
#include "ui/ozone/platform/drm/gpu/drm_display_snapshot.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

const char kContentProtection[] = "Content Protection";

struct ContentProtectionMapping {
  const char* name;
  HDCPState state;
};

const ContentProtectionMapping kContentProtectionStates[] = {
    {"Undesired", HDCP_STATE_UNDESIRED},
    {"Desired", HDCP_STATE_DESIRED},
    {"Enabled", HDCP_STATE_ENABLED}};

uint32_t GetContentProtectionValue(drmModePropertyRes* property,
                                   HDCPState state) {
  std::string name;
  for (size_t i = 0; i < arraysize(kContentProtectionStates); ++i) {
    if (kContentProtectionStates[i].state == state) {
      name = kContentProtectionStates[i].name;
      break;
    }
  }

  for (int i = 0; i < property->count_enums; ++i)
    if (name == property->enums[i].name)
      return i;

  NOTREACHED();
  return 0;
}

class DisplaySnapshotComparator {
 public:
  explicit DisplaySnapshotComparator(const DrmDisplaySnapshot* snapshot)
      : drm_(snapshot->drm()),
        crtc_(snapshot->crtc()),
        connector_(snapshot->connector()) {}

  DisplaySnapshotComparator(const scoped_refptr<DrmDevice>& drm,
                            uint32_t crtc,
                            uint32_t connector)
      : drm_(drm), crtc_(crtc), connector_(connector) {}

  bool operator()(const DrmDisplaySnapshot* other) const {
    return drm_ == other->drm() && connector_ == other->connector() &&
           crtc_ == other->crtc();
  }

 private:
  scoped_refptr<DrmDevice> drm_;
  uint32_t crtc_;
  uint32_t connector_;
};

std::string GetEnumNameForProperty(drmModeConnector* connector,
                                   drmModePropertyRes* property) {
  for (int prop_idx = 0; prop_idx < connector->count_props; ++prop_idx) {
    if (connector->props[prop_idx] != property->prop_id)
      continue;

    for (int enum_idx = 0; enum_idx < property->count_enums; ++enum_idx) {
      const drm_mode_property_enum& property_enum = property->enums[enum_idx];
      if (property_enum.value == connector->prop_values[prop_idx])
        return property_enum.name;
    }
  }

  NOTREACHED();
  return std::string();
}

}  // namespace

DrmGpuDisplayManager::DrmGpuDisplayManager(ScreenManager* screen_manager,
                                           DrmDeviceManager* drm_device_manager)
    : screen_manager_(screen_manager), drm_device_manager_(drm_device_manager) {
}

DrmGpuDisplayManager::~DrmGpuDisplayManager() {
}

std::vector<DisplaySnapshot_Params> DrmGpuDisplayManager::GetDisplays() {
  RefreshDisplayList();

  std::vector<DisplaySnapshot_Params> displays;
  for (size_t i = 0; i < cached_displays_.size(); ++i)
    displays.push_back(GetDisplaySnapshotParams(*cached_displays_[i]));

  return displays;
}

bool DrmGpuDisplayManager::ConfigureDisplay(
    int64_t id,
    const DisplayMode_Params& mode_param,
    const gfx::Point& origin) {
  DrmDisplaySnapshot* display = FindDisplaySnapshot(id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << id;
    return false;
  }

  const DisplayMode* mode = NULL;
  for (size_t i = 0; i < display->modes().size(); ++i) {
    if (mode_param.size == display->modes()[i]->size() &&
        mode_param.is_interlaced == display->modes()[i]->is_interlaced() &&
        mode_param.refresh_rate == display->modes()[i]->refresh_rate()) {
      mode = display->modes()[i];
      break;
    }
  }

  // If the display doesn't have the mode natively, then lookup the mode from
  // other displays and try using it on the current display (some displays
  // support panel fitting and they can use different modes even if the mode
  // isn't explicitly declared).
  if (!mode)
    mode = FindDisplayMode(mode_param.size, mode_param.is_interlaced,
                           mode_param.refresh_rate);

  if (!mode) {
    LOG(ERROR) << "Failed to find mode: size=" << mode_param.size.ToString()
               << " is_interlaced=" << mode_param.is_interlaced
               << " refresh_rate=" << mode_param.refresh_rate;
    return false;
  }

  bool success =
      Configure(*display, static_cast<const DrmDisplayMode*>(mode), origin);
  if (success) {
    display->set_origin(origin);
    display->set_current_mode(mode);
  }

  return success;
}

bool DrmGpuDisplayManager::DisableDisplay(int64_t id) {
  DrmDisplaySnapshot* display = FindDisplaySnapshot(id);
  bool success = false;
  if (display)
    success = Configure(*display, NULL, gfx::Point());
  else
    LOG(ERROR) << "There is no display with ID " << id;

  return success;
}

bool DrmGpuDisplayManager::TakeDisplayControl() {
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  for (const auto& drm : devices) {
    if (!drm->SetMaster()) {
      LOG(ERROR) << "Failed to take control of the display";
      return false;
    }
  }
  return true;
}

bool DrmGpuDisplayManager::RelinquishDisplayControl() {
  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  for (const auto& drm : devices) {
    if (!drm->DropMaster()) {
      LOG(ERROR) << "Failed to relinquish control of the display";
      return false;
    }
  }
  return true;
}

void DrmGpuDisplayManager::SetGammaRamp(
    int64_t id,
    const std::vector<GammaRampRGBEntry>& lut) {
  DrmDisplaySnapshot* display = FindDisplaySnapshot(id);
  if (!display) {
    LOG(ERROR) << "There is no display with ID " << id;
    return;
  }

  if (!display->drm()->SetGammaRamp(display->crtc(), lut)) {
    LOG(ERROR) << "Failed to set gamma ramp for display: crtc_id = "
               << display->crtc() << " size = " << lut.size();
  }
}

DrmDisplaySnapshot* DrmGpuDisplayManager::FindDisplaySnapshot(int64_t id) {
  for (size_t i = 0; i < cached_displays_.size(); ++i)
    if (cached_displays_[i]->display_id() == id)
      return cached_displays_[i];

  return NULL;
}

const DrmDisplayMode* DrmGpuDisplayManager::FindDisplayMode(
    const gfx::Size& size,
    bool is_interlaced,
    float refresh_rate) {
  for (size_t i = 0; i < cached_modes_.size(); ++i)
    if (cached_modes_[i]->size() == size &&
        cached_modes_[i]->is_interlaced() == is_interlaced &&
        cached_modes_[i]->refresh_rate() == refresh_rate)
      return static_cast<const DrmDisplayMode*>(cached_modes_[i]);

  return NULL;
}

void DrmGpuDisplayManager::RefreshDisplayList() {
  ScopedVector<DrmDisplaySnapshot> old_displays(cached_displays_.Pass());
  ScopedVector<const DisplayMode> old_modes(cached_modes_.Pass());

  const DrmDeviceVector& devices = drm_device_manager_->GetDrmDevices();
  for (const auto& drm : devices) {
    ScopedVector<HardwareDisplayControllerInfo> displays =
        GetAvailableDisplayControllerInfos(drm->get_fd());
    for (size_t i = 0; i < displays.size(); ++i) {
      DrmDisplaySnapshot* display = new DrmDisplaySnapshot(
          drm, displays[i]->connector(), displays[i]->crtc(), i);

      // If the display exists make sure to sync up the new snapshot with the
      // old one to keep the user configured details.
      auto it = std::find_if(
          old_displays.begin(), old_displays.end(),
          DisplaySnapshotComparator(drm, displays[i]->crtc()->crtc_id,
                                    displays[i]->connector()->connector_id));
      // Origin is only used within the platform code to keep track of the
      // display location.
      if (it != old_displays.end())
        display->set_origin((*it)->origin());

      cached_displays_.push_back(display);
      cached_modes_.insert(cached_modes_.end(), display->modes().begin(),
                           display->modes().end());
    }
  }

  NotifyScreenManager(cached_displays_.get(), old_displays.get());
}

bool DrmGpuDisplayManager::Configure(const DrmDisplaySnapshot& output,
                                     const DrmDisplayMode* mode,
                                     const gfx::Point& origin) {
  VLOG(1) << "DRM configuring: device=" << output.drm()->device_path().value()
          << " crtc=" << output.crtc() << " connector=" << output.connector()
          << " origin=" << origin.ToString()
          << " size=" << (mode ? mode->size().ToString() : "0x0");

  if (mode) {
    if (!screen_manager_->ConfigureDisplayController(
            output.drm(), output.crtc(), output.connector(), origin,
            mode->mode_info())) {
      VLOG(1) << "Failed to configure: device="
              << output.drm()->device_path().value()
              << " crtc=" << output.crtc()
              << " connector=" << output.connector();
      return false;
    }
  } else {
    if (!screen_manager_->DisableDisplayController(output.drm(),
                                                   output.crtc())) {
      VLOG(1) << "Failed to disable device="
              << output.drm()->device_path().value()
              << " crtc=" << output.crtc();
      return false;
    }
  }

  return true;
}

bool DrmGpuDisplayManager::GetHDCPState(int64_t display_id, HDCPState* state) {
  DrmDisplaySnapshot* output = FindDisplaySnapshot(display_id);
  if (!output) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return false;
  }

  ScopedDrmConnectorPtr connector(
      output->drm()->GetConnector(output->connector()));
  if (!connector) {
    PLOG(ERROR) << "Failed to get connector " << output->connector();
    return false;
  }

  ScopedDrmPropertyPtr hdcp_property(
      output->drm()->GetProperty(connector.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(ERROR) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  std::string name =
      GetEnumNameForProperty(connector.get(), hdcp_property.get());
  for (size_t i = 0; i < arraysize(kContentProtectionStates); ++i) {
    if (name == kContentProtectionStates[i].name) {
      *state = kContentProtectionStates[i].state;
      VLOG(3) << "HDCP state: " << *state << " (" << name << ")";
      return true;
    }
  }

  LOG(ERROR) << "Unknown content protection value '" << name << "'";
  return false;
}

bool DrmGpuDisplayManager::SetHDCPState(int64_t display_id, HDCPState state) {
  DrmDisplaySnapshot* output = FindDisplaySnapshot(display_id);
  if (!output) {
    LOG(ERROR) << "There is no display with ID " << display_id;
    return false;
  }

  ScopedDrmConnectorPtr connector(
      output->drm()->GetConnector(output->connector()));
  if (!connector) {
    PLOG(ERROR) << "Failed to get connector " << output->connector();
    return false;
  }

  ScopedDrmPropertyPtr hdcp_property(
      output->drm()->GetProperty(connector.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(ERROR) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  return output->drm()->SetProperty(
      output->connector(), hdcp_property->prop_id,
      GetContentProtectionValue(hdcp_property.get(), state));
}

void DrmGpuDisplayManager::NotifyScreenManager(
    const std::vector<DrmDisplaySnapshot*>& new_displays,
    const std::vector<DrmDisplaySnapshot*>& old_displays) const {
  for (size_t i = 0; i < old_displays.size(); ++i) {
    const std::vector<DrmDisplaySnapshot*>::const_iterator it =
        std::find_if(new_displays.begin(), new_displays.end(),
                     DisplaySnapshotComparator(old_displays[i]));

    if (it == new_displays.end()) {
      screen_manager_->RemoveDisplayController(old_displays[i]->drm(),
                                               old_displays[i]->crtc());
    }
  }

  for (size_t i = 0; i < new_displays.size(); ++i) {
    const std::vector<DrmDisplaySnapshot*>::const_iterator it =
        std::find_if(old_displays.begin(), old_displays.end(),
                     DisplaySnapshotComparator(new_displays[i]));

    if (it == old_displays.end()) {
      screen_manager_->AddDisplayController(new_displays[i]->drm(),
                                            new_displays[i]->crtc(),
                                            new_displays[i]->connector());
    }
  }
}

}  // namespace ui
