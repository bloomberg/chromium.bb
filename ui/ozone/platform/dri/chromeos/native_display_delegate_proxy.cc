// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/chromeos/native_display_delegate_proxy.h"

#include "base/logging.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/events/ozone/device/device_event.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/ozone/common/chromeos/display_snapshot_proxy.h"
#include "ui/ozone/common/chromeos/display_util.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/dri/gpu_platform_support_host_gbm.h"

namespace ui {

NativeDisplayDelegateProxy::NativeDisplayDelegateProxy(
    GpuPlatformSupportHostGbm* proxy,
    DeviceManager* device_manager)
    : proxy_(proxy),
      device_manager_(device_manager) {
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
}

void NativeDisplayDelegateProxy::GrabServer() {}

void NativeDisplayDelegateProxy::UngrabServer() {}

void NativeDisplayDelegateProxy::SyncWithServer() {}

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
                                         const DisplayMode* mode) {}

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

void NativeDisplayDelegateProxy::CreateFrameBuffer(const gfx::Size& size) {}

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

void NativeDisplayDelegateProxy::OnChannelEstablished(
    int host_id, IPC::Sender* sender) {
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
  IPC_MESSAGE_HANDLER(OzoneHostMsg_UpdateNativeDisplays,
                      OnUpdateNativeDisplays)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NativeDisplayDelegateProxy::OnUpdateNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& displays) {
  displays_.clear();
  for (size_t i = 0; i < displays.size(); ++i)
    displays_.push_back(new DisplaySnapshotProxy(displays[i]));

  FOR_EACH_OBSERVER(
      NativeDisplayObserver, observers_, OnConfigurationChanged());
}

}  // namespace ui
