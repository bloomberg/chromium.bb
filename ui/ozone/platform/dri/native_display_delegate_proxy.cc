// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/native_display_delegate_proxy.h"

#include <stdio.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/common/display_util.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/display_manager.h"
#include "ui/ozone/platform/dri/dri_gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

const int64_t kDummyDisplayId = 1;

class DriDisplaySnapshotProxy : public DisplaySnapshotProxy {
 public:
  DriDisplaySnapshotProxy(const DisplaySnapshot_Params& params,
                          DisplayManager* display_manager)
      : DisplaySnapshotProxy(params), display_manager_(display_manager) {
    display_manager_->RegisterDisplay(this);
  }

  ~DriDisplaySnapshotProxy() override {
    display_manager_->UnregisterDisplay(this);
  }

 private:
  DisplayManager* display_manager_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DriDisplaySnapshotProxy);
};

DisplaySnapshotProxy* CreateSnapshotFromSpec(const std::string& spec,
                                             const std::string& physical_spec,
                                             DisplayManager* display_manager) {
  if (spec.empty())
    return nullptr;

  int width = 0;
  int height = 0;
  if (sscanf(spec.c_str(), "%dx%d", &width, &height) < 2)
    return nullptr;

  if (width == 0 || height == 0)
    return nullptr;

  int physical_width = 0;
  int physical_height = 0;
  sscanf(physical_spec.c_str(), "%dx%d", &physical_width, &physical_height);

  DisplayMode_Params mode_param;
  mode_param.size = gfx::Size(width, height);
  mode_param.refresh_rate = 60;

  DisplaySnapshot_Params display_param;
  display_param.display_id = kDummyDisplayId;
  display_param.modes.push_back(mode_param);
  display_param.type = DISPLAY_CONNECTION_TYPE_INTERNAL;
  display_param.physical_size = gfx::Size(physical_width, physical_height);
  display_param.has_current_mode = true;
  display_param.current_mode = mode_param;
  display_param.has_native_mode = true;
  display_param.native_mode = mode_param;

  return new DriDisplaySnapshotProxy(display_param, display_manager);
}

}  // namespace

NativeDisplayDelegateProxy::NativeDisplayDelegateProxy(
    DriGpuPlatformSupportHost* proxy,
    DeviceManager* device_manager,
    DisplayManager* display_manager)
    : proxy_(proxy),
      device_manager_(device_manager),
      display_manager_(display_manager) {
  proxy_->RegisterHandler(this);
}

NativeDisplayDelegateProxy::~NativeDisplayDelegateProxy() {
  if (device_manager_)
    device_manager_->RemoveObserver(this);

  proxy_->UnregisterHandler(this);
}

void NativeDisplayDelegateProxy::Initialize() {
  if (device_manager_)
    device_manager_->AddObserver(this);

  if (!displays_.empty())
    return;

  CommandLine* cmd = CommandLine::ForCurrentProcess();
  DisplaySnapshotProxy* display = CreateSnapshotFromSpec(
      cmd->GetSwitchValueASCII(switches::kOzoneInitialDisplayBounds),
      cmd->GetSwitchValueASCII(switches::kOzoneInitialDisplayPhysicalSizeMm),
      display_manager_);
  if (display)
    displays_.push_back(display);
}

void NativeDisplayDelegateProxy::GrabServer() {
}

void NativeDisplayDelegateProxy::UngrabServer() {
}

bool NativeDisplayDelegateProxy::TakeDisplayControl() {
  proxy_->Send(new OzoneGpuMsg_TakeDisplayControl());
  return true;
}

bool NativeDisplayDelegateProxy::RelinquishDisplayControl() {
  proxy_->Send(new OzoneGpuMsg_RelinquishDisplayControl());
  return true;
}

void NativeDisplayDelegateProxy::SyncWithServer() {
}

void NativeDisplayDelegateProxy::SetBackgroundColor(uint32_t color_argb) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateProxy::ForceDPMSOn() {
  proxy_->Send(new OzoneGpuMsg_ForceDPMSOn());
}

std::vector<DisplaySnapshot*> NativeDisplayDelegateProxy::GetDisplays() {
  return displays_.get();
}

void NativeDisplayDelegateProxy::AddMode(const DisplaySnapshot& output,
                                         const DisplayMode* mode) {
}

bool NativeDisplayDelegateProxy::Configure(const DisplaySnapshot& output,
                                           const DisplayMode* mode,
                                           const gfx::Point& origin) {
  // TODO(dnicoara) Should handle an asynchronous response.
  if (mode)
    proxy_->Send(new OzoneGpuMsg_ConfigureNativeDisplay(
        output.display_id(), GetDisplayModeParams(*mode), origin));
  else
    proxy_->Send(new OzoneGpuMsg_DisableNativeDisplay(output.display_id()));

  return true;
}

void NativeDisplayDelegateProxy::CreateFrameBuffer(const gfx::Size& size) {
}

bool NativeDisplayDelegateProxy::GetHDCPState(const DisplaySnapshot& output,
                                              HDCPState* state) {
  NOTIMPLEMENTED();
  return false;
}

bool NativeDisplayDelegateProxy::SetHDCPState(const DisplaySnapshot& output,
                                              HDCPState state) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<ColorCalibrationProfile>
NativeDisplayDelegateProxy::GetAvailableColorCalibrationProfiles(
    const DisplaySnapshot& output) {
  NOTIMPLEMENTED();
  return std::vector<ColorCalibrationProfile>();
}

bool NativeDisplayDelegateProxy::SetColorCalibrationProfile(
    const DisplaySnapshot& output,
    ColorCalibrationProfile new_profile) {
  NOTIMPLEMENTED();
  return false;
}

void NativeDisplayDelegateProxy::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void NativeDisplayDelegateProxy::RemoveObserver(
    NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NativeDisplayDelegateProxy::OnDeviceEvent(const DeviceEvent& event) {
  if (event.device_type() != DeviceEvent::DISPLAY)
    return;

  if (event.action_type() == DeviceEvent::CHANGE) {
    VLOG(1) << "Got display changed event";
    proxy_->Send(new OzoneGpuMsg_RefreshNativeDisplays(
        std::vector<DisplaySnapshot_Params>()));
  }
}

void NativeDisplayDelegateProxy::OnChannelEstablished(int host_id,
                                                      IPC::Sender* sender) {
  std::vector<DisplaySnapshot_Params> display_params;
  for (size_t i = 0; i < displays_.size(); ++i)
    display_params.push_back(GetDisplaySnapshotParams(*displays_[i]));

  // Force an initial configure such that the browser process can get the actual
  // state. Pass in the current display state since the GPU process may have
  // crashed and we want to re-synchronize the state between processes.
  proxy_->Send(new OzoneGpuMsg_RefreshNativeDisplays(display_params));
}

void NativeDisplayDelegateProxy::OnChannelDestroyed(int host_id) {
}

bool NativeDisplayDelegateProxy::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(NativeDisplayDelegateProxy, message)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_UpdateNativeDisplays, OnUpdateNativeDisplays)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NativeDisplayDelegateProxy::OnUpdateNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& displays) {
  displays_.clear();
  for (size_t i = 0; i < displays.size(); ++i)
    displays_.push_back(
        new DriDisplaySnapshotProxy(displays[i], display_manager_));

  FOR_EACH_OBSERVER(NativeDisplayObserver, observers_,
                    OnConfigurationChanged());
}

}  // namespace ui
