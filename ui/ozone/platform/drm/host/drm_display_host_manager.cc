// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"

#include "base/thread_task_runner_handle.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_display_host.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_native_display_delegate.h"

namespace ui {

DrmDisplayHostManager::DrmDisplayHostManager(
    DrmGpuPlatformSupportHost* proxy,
    DeviceManager* device_manager,
    InputControllerEvdev* input_controller)
    : sender_(new HostManagerIPC(proxy, this)),
      core_(new DrmDisplayHostManagerCore(sender_.get(),
                                          device_manager,
                                          input_controller)) {}

DrmDisplayHostManager::~DrmDisplayHostManager() {
}

DrmDisplayHost* DrmDisplayHostManager::GetDisplay(int64_t display_id) {
  return core_->GetDisplay(display_id);
}

void DrmDisplayHostManager::AddDelegate(DrmNativeDisplayDelegate* delegate) {
  core_->AddDelegate(delegate);
}

void DrmDisplayHostManager::RemoveDelegate(DrmNativeDisplayDelegate* delegate) {
  core_->RemoveDelegate(delegate);
}

void DrmDisplayHostManager::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  core_->TakeDisplayControl(callback);
}

void DrmDisplayHostManager::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  core_->RelinquishDisplayControl(callback);
}

void DrmDisplayHostManager::UpdateDisplays(
    const GetDisplaysCallback& callback) {
  core_->UpdateDisplays(callback);
}

void DrmDisplayHostManager::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& send_callback) {
  // The GPU thread may be in a different or the same process.
  core_->GpuThreadStarted();
}

void DrmDisplayHostManager::OnChannelDestroyed(int host_id) {
}

bool DrmDisplayHostManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmDisplayHostManager, message)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_UpdateNativeDisplays, OnUpdateNativeDisplays)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayConfigured, OnDisplayConfigured)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateReceived, OnHDCPStateReceived)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateUpdated, OnHDCPStateUpdated)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayControlTaken, OnTakeDisplayControl)
  IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayControlRelinquished,
                      OnRelinquishDisplayControl)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DrmDisplayHostManager::OnUpdateNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& params) {
  core_->GpuHasUpdatedNativeDisplays(params);
}

void DrmDisplayHostManager::OnDisplayConfigured(int64_t display_id,
                                                bool status) {
  core_->GpuConfiguredDisplay(display_id, status);
}

void DrmDisplayHostManager::OnHDCPStateReceived(int64_t display_id,
                                                bool status,
                                                HDCPState state) {
  core_->GpuReceivedHDCPState(display_id, status, state);
}

void DrmDisplayHostManager::OnHDCPStateUpdated(int64_t display_id,
                                               bool status) {
  core_->GpuUpdatedHDCPState(display_id, status);
}

void DrmDisplayHostManager::OnTakeDisplayControl(bool status) {
  core_->GpuTookDisplayControl(status);
}

void DrmDisplayHostManager::OnRelinquishDisplayControl(bool status) {
  core_->GpuRelinquishedDisplayControl(status);
}

DrmDisplayHostManager::HostManagerIPC::HostManagerIPC(
    DrmGpuPlatformSupportHost* proxy,
    DrmDisplayHostManager* parent)
    : proxy_(proxy), parent_(parent) {}

DrmDisplayHostManager::HostManagerIPC::~HostManagerIPC() {
  proxy_->UnregisterHandler(parent_);
}

void DrmDisplayHostManager::HostManagerIPC::RegisterHandler() {
  proxy_->RegisterHandler(parent_);
}

DrmGpuPlatformSupportHost*
DrmDisplayHostManager::HostManagerIPC::GetGpuPlatformSupportHost() {
  return proxy_;
}

bool DrmDisplayHostManager::HostManagerIPC::RefreshNativeDisplays() {
  return proxy_->Send(new OzoneGpuMsg_RefreshNativeDisplays());
}

bool DrmDisplayHostManager::HostManagerIPC::TakeDisplayControl() {
  return proxy_->Send(new OzoneGpuMsg_TakeDisplayControl());
}

bool DrmDisplayHostManager::HostManagerIPC::RelinquishDisplayControl() {
  return proxy_->Send(new OzoneGpuMsg_RelinquishDisplayControl());
}

bool DrmDisplayHostManager::HostManagerIPC::AddGraphicsDevice(
    const base::FilePath& path,
    base::FileDescriptor fd) {
  return proxy_->Send(new OzoneGpuMsg_AddGraphicsDevice(path, fd));
}

bool DrmDisplayHostManager::HostManagerIPC::RemoveGraphicsDevice(
    const base::FilePath& path) {
  return proxy_->Send(new OzoneGpuMsg_RemoveGraphicsDevice(path));
}

}  // namespace ui
