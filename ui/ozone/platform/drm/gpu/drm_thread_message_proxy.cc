// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"

#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/display/types/display_snapshot_mojo.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

namespace ui {

DrmThreadMessageProxy::DrmThreadMessageProxy() : weak_ptr_factory_(this) {}

DrmThreadMessageProxy::~DrmThreadMessageProxy() {}

void DrmThreadMessageProxy::SetDrmThread(DrmThread* thread) {
  drm_thread_ = thread;
}

void DrmThreadMessageProxy::OnFilterAdded(IPC::Channel* channel) {
  sender_ = channel;

  // The DRM thread needs to be started late since we need to wait for the
  // sandbox to start.
  drm_thread_->Start();
}

bool DrmThreadMessageProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmThreadMessageProxy, message)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_DestroyWindow, OnDestroyWindow)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_WindowBoundsChanged, OnWindowBoundsChanged)

    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)

    IPC_MESSAGE_HANDLER(OzoneGpuMsg_RefreshNativeDisplays,
                        OnRefreshNativeDisplays)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_ConfigureNativeDisplay,
                        OnConfigureNativeDisplay)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_DisableNativeDisplay,
                        OnDisableNativeDisplay)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_TakeDisplayControl, OnTakeDisplayControl)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_RelinquishDisplayControl,
                        OnRelinquishDisplayControl)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_AddGraphicsDevice, OnAddGraphicsDevice)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_RemoveGraphicsDevice,
                        OnRemoveGraphicsDevice)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_GetHDCPState, OnGetHDCPState)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_SetHDCPState, OnSetHDCPState)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_SetColorCorrection, OnSetColorCorrection)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CheckOverlayCapabilities,
                        OnCheckOverlayCapabilities)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DrmThreadMessageProxy::OnCreateWindow(gfx::AcceleratedWidget widget) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::CreateWindow,
                            base::Unretained(drm_thread_), widget));
}

void DrmThreadMessageProxy::OnDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::DestroyWindow,
                            base::Unretained(drm_thread_), widget));
}

void DrmThreadMessageProxy::OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                                  const gfx::Rect& bounds) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::SetWindowBounds,
                            base::Unretained(drm_thread_), widget, bounds));
}

void DrmThreadMessageProxy::OnCursorSet(gfx::AcceleratedWidget widget,
                                        const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location,
                                        int frame_delay_ms) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetCursor, base::Unretained(drm_thread_), widget,
                 bitmaps, location, frame_delay_ms));
}

void DrmThreadMessageProxy::OnCursorMove(gfx::AcceleratedWidget widget,
                                         const gfx::Point& location) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::MoveCursor,
                            base::Unretained(drm_thread_), widget, location));
}

void DrmThreadMessageProxy::OnCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays) {
  DCHECK(drm_thread_->IsRunning());
  auto callback =
      base::BindOnce(&DrmThreadMessageProxy::OnCheckOverlayCapabilitiesCallback,
                     weak_ptr_factory_.GetWeakPtr());

  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::CheckOverlayCapabilities,
                                base::Unretained(drm_thread_), widget, overlays,
                                std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnRefreshNativeDisplays() {
  DCHECK(drm_thread_->IsRunning());
  auto callback =
      base::BindOnce(&DrmThreadMessageProxy::OnRefreshNativeDisplaysCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RefreshNativeDisplays,
                     base::Unretained(drm_thread_), std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnConfigureNativeDisplay(
    int64_t id,
    const DisplayMode_Params& mode,
    const gfx::Point& origin) {
  DCHECK(drm_thread_->IsRunning());
  auto dmode = CreateDisplayModeFromParams(mode);
  auto callback =
      base::BindOnce(&DrmThreadMessageProxy::OnConfigureNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::ConfigureNativeDisplay,
                     base::Unretained(drm_thread_), id, std::move(dmode),
                     origin, std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnDisableNativeDisplay(int64_t id) {
  DCHECK(drm_thread_->IsRunning());
  auto callback =
      base::BindOnce(&DrmThreadMessageProxy::OnDisableNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::DisableNativeDisplay,
                                base::Unretained(drm_thread_), id,
                                std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnTakeDisplayControl() {
  DCHECK(drm_thread_->IsRunning());
  auto callback =
      base::BindOnce(&DrmThreadMessageProxy::OnTakeDisplayControlCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::TakeDisplayControl,
                     base::Unretained(drm_thread_), std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnRelinquishDisplayControl() {
  DCHECK(drm_thread_->IsRunning());
  auto callback =
      base::BindOnce(&DrmThreadMessageProxy::OnRelinquishDisplayControlCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RelinquishDisplayControl,
                     base::Unretained(drm_thread_), std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnAddGraphicsDevice(
    const base::FilePath& path,
    const base::FileDescriptor& fd) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::AddGraphicsDevice,
                            base::Unretained(drm_thread_), path, fd));
}

void DrmThreadMessageProxy::OnRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::RemoveGraphicsDevice,
                            base::Unretained(drm_thread_), path));
}

void DrmThreadMessageProxy::OnGetHDCPState(int64_t display_id) {
  DCHECK(drm_thread_->IsRunning());
  auto callback = base::BindOnce(&DrmThreadMessageProxy::OnGetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::GetHDCPState, base::Unretained(drm_thread_),
                     display_id, std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnSetHDCPState(int64_t display_id,
                                           display::HDCPState state) {
  DCHECK(drm_thread_->IsRunning());
  auto callback = base::BindOnce(&DrmThreadMessageProxy::OnSetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::SetHDCPState, base::Unretained(drm_thread_),
                     display_id, state, std::move(safe_callback)));
}

void DrmThreadMessageProxy::OnSetColorCorrection(
    int64_t id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetColorCorrection, base::Unretained(drm_thread_),
                 id, degamma_lut, gamma_lut, correction_matrix));
}

void DrmThreadMessageProxy::OnCheckOverlayCapabilitiesCallback(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays,
    const std::vector<OverlayCheckReturn_Params>& returns) const {
  sender_->Send(
      new OzoneHostMsg_OverlayCapabilitiesReceived(widget, overlays, returns));
}

void DrmThreadMessageProxy::OnRefreshNativeDisplaysCallback(
    MovableDisplaySnapshots displays) const {
  sender_->Send(new OzoneHostMsg_UpdateNativeDisplays(
      CreateParamsFromSnapshot(displays)));
}

void DrmThreadMessageProxy::OnConfigureNativeDisplayCallback(
    int64_t display_id,
    bool success) const {
  sender_->Send(new OzoneHostMsg_DisplayConfigured(display_id, success));
}

void DrmThreadMessageProxy::OnDisableNativeDisplayCallback(int64_t display_id,
                                                           bool success) const {
  sender_->Send(new OzoneHostMsg_DisplayConfigured(display_id, success));
}

void DrmThreadMessageProxy::OnTakeDisplayControlCallback(bool success) const {
  sender_->Send(new OzoneHostMsg_DisplayControlTaken(success));
}

void DrmThreadMessageProxy::OnRelinquishDisplayControlCallback(
    bool success) const {
  sender_->Send(new OzoneHostMsg_DisplayControlRelinquished(success));
}

void DrmThreadMessageProxy::OnGetHDCPStateCallback(
    int64_t display_id,
    bool success,
    display::HDCPState state) const {
  sender_->Send(new OzoneHostMsg_HDCPStateReceived(display_id, success, state));
}

void DrmThreadMessageProxy::OnSetHDCPStateCallback(int64_t display_id,
                                                   bool success) const {
  sender_->Send(new OzoneHostMsg_HDCPStateUpdated(display_id, success));
}

}  // namespace ui
