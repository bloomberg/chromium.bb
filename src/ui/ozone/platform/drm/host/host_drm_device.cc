// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/host_drm_device.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/drm_device_connector.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/host_cursor_proxy.h"

namespace ui {

HostDrmDevice::HostDrmDevice(DrmCursor* cursor) : cursor_(cursor) {
  DETACH_FROM_THREAD(on_io_thread_);
}

HostDrmDevice::~HostDrmDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

void HostDrmDevice::OnDrmServiceStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  DCHECK(!connected_);

  connected_ = true;

  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadReady();

  DCHECK(cursor_proxy_)
      << "We should have already created a cursor proxy previously";
  cursor_->SetDrmCursorProxy(std::move(cursor_proxy_));

  // TODO(rjkroege): Call ResetDrmCursorProxy when the mojo connection to the
  // DRM thread is broken.
}

void HostDrmDevice::SetDisplayManager(DrmDisplayHostManager* display_manager) {
  display_manager_ = display_manager;
}

void HostDrmDevice::AddGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  gpu_thread_observers_.AddObserver(observer);
  if (IsConnected())
    observer->OnGpuThreadReady();
}

void HostDrmDevice::RemoveGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  gpu_thread_observers_.RemoveObserver(observer);
}

bool HostDrmDevice::IsConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);

  // TODO(rjkroege): Need to set to connected_ to false when we lose the Viz
  // process connection.
  return connected_;
}

// Services needed for DrmDisplayHostMananger.
void HostDrmDevice::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_ = handler;
}

void HostDrmDevice::UnRegisterHandlerForDrmDisplayHostManager() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_ = nullptr;
}

bool HostDrmDevice::GpuCreateWindow(gfx::AcceleratedWidget widget,
                                    const gfx::Rect& initial_bounds) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->CreateWindow(widget, initial_bounds);
  return true;
}

bool HostDrmDevice::GpuDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->DestroyWindow(widget);
  return true;
}

bool HostDrmDevice::GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                           const gfx::Rect& bounds) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetWindowBounds(widget, bounds);

  return true;
}

bool HostDrmDevice::GpuRefreshNativeDisplays() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  auto callback =
      base::BindOnce(&HostDrmDevice::GpuRefreshNativeDisplaysCallback, this);
  drm_device_->RefreshNativeDisplays(std::move(callback));

  return true;
}

bool HostDrmDevice::GpuConfigureNativeDisplay(int64_t id,
                                              const display::DisplayMode& pmode,
                                              const gfx::Point& origin) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  auto mode = std::make_unique<display::DisplayMode>(
      pmode.size(), pmode.is_interlaced(), pmode.refresh_rate());
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuConfigureNativeDisplayCallback, this);

  drm_device_->ConfigureNativeDisplay(id, std::move(mode), origin,
                                      std::move(callback));

  return true;
}

bool HostDrmDevice::GpuDisableNativeDisplay(int64_t id) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuDisableNativeDisplayCallback, this);

  drm_device_->DisableNativeDisplay(id, std::move(callback));

  return true;
}

bool HostDrmDevice::GpuTakeDisplayControl() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuTakeDisplayControlCallback, this);

  drm_device_->TakeDisplayControl(std::move(callback));

  return true;
}

bool HostDrmDevice::GpuRelinquishDisplayControl() {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuRelinquishDisplayControlCallback, this);

  drm_device_->RelinquishDisplayControl(std::move(callback));

  return true;
}

bool HostDrmDevice::GpuAddGraphicsDeviceOnUIThread(const base::FilePath& path,
                                                   base::ScopedFD fd) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  base::File file(std::move(fd));

  drm_device_->AddGraphicsDevice(path, std::move(file));

  return true;
}

void HostDrmDevice::GpuAddGraphicsDeviceOnIOThread(const base::FilePath& path,
                                                   base::ScopedFD fd) {
  DCHECK_CALLED_ON_VALID_THREAD(on_io_thread_);
  DCHECK(drm_device_on_io_thread_.is_bound());
  base::File file(std::move(fd));
  drm_device_on_io_thread_->AddGraphicsDevice(path, std::move(file));
}

bool HostDrmDevice::GpuRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->RemoveGraphicsDevice(std::move(path));

  return true;
}

bool HostDrmDevice::GpuGetHDCPState(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuGetHDCPStateCallback, this);

  drm_device_->GetHDCPState(display_id, std::move(callback));

  return true;
}

bool HostDrmDevice::GpuSetHDCPState(int64_t display_id,
                                    display::HDCPState state) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuSetHDCPStateCallback, this);

  drm_device_->SetHDCPState(display_id, state, std::move(callback));

  return true;
}

bool HostDrmDevice::GpuSetColorMatrix(int64_t display_id,
                                      const std::vector<float>& color_matrix) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetColorMatrix(display_id, color_matrix);
  return true;
}

bool HostDrmDevice::GpuSetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetGammaCorrection(display_id, degamma_lut, gamma_lut);
  return true;
}

bool HostDrmDevice::GpuSetPrivacyScreen(int64_t display_id, bool enabled) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  if (!IsConnected())
    return false;

  drm_device_->SetPrivacyScreen(display_id, enabled);
  return true;
}

void HostDrmDevice::GpuConfigureNativeDisplayCallback(int64_t display_id,
                                                      bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void HostDrmDevice::GpuRefreshNativeDisplaysCallback(
    MovableDisplaySnapshots displays) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuHasUpdatedNativeDisplays(std::move(displays));
}

void HostDrmDevice::GpuDisableNativeDisplayCallback(int64_t display_id,
                                                    bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void HostDrmDevice::GpuTakeDisplayControlCallback(bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuTookDisplayControl(success);
}

void HostDrmDevice::GpuRelinquishDisplayControlCallback(bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuRelinquishedDisplayControl(success);
}

void HostDrmDevice::GpuGetHDCPStateCallback(int64_t display_id,
                                            bool success,
                                            display::HDCPState state) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuReceivedHDCPState(display_id, success, state);
}

void HostDrmDevice::GpuSetHDCPStateCallback(int64_t display_id,
                                            bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);
  display_manager_->GpuUpdatedHDCPState(display_id, success);
}

void HostDrmDevice::OnGpuServiceLaunchedOnIOThread(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(on_io_thread_);
  // The observers might send IPC messages from the IO thread during the call to
  // OnGpuProcessLaunched.
  drm_device_on_io_thread_.Bind(std::move(drm_device));
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuProcessLaunched();
  // In the single-threaded mode, there won't be separate UI and IO threads.
  if (ui_runner->BelongsToCurrentThread()) {
    OnGpuServiceLaunchedOnUIThread(drm_device_on_io_thread_.Unbind());
  } else {
    ui_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&HostDrmDevice::OnGpuServiceLaunchedOnUIThread, this,
                       drm_device_on_io_thread_.Unbind()));
  }
}

void HostDrmDevice::OnGpuServiceLaunchedOnUIThread(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device) {
  DCHECK_CALLED_ON_VALID_THREAD(on_ui_thread_);

  // We can get into this state if a new instance of GpuProcessHost is created
  // before the old one is destroyed.
  if (IsConnected())
    OnGpuServiceLost();

  drm_device_.Bind(std::move(drm_device));

  // Create two DeviceCursor connections: one for the UI thread and one for the
  // IO thread.
  mojo::PendingAssociatedRemote<ui::ozone::mojom::DeviceCursor> cursor_ui,
      cursor_io;
  drm_device_->GetDeviceCursor(cursor_ui.InitWithNewEndpointAndPassReceiver());
  drm_device_->GetDeviceCursor(cursor_io.InitWithNewEndpointAndPassReceiver());

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. As a result, it has a Remote for both the UI
  // and I/O thread. cursor_io is already bound correctly to an I/O thread by
  // GpuProcessHost.
  cursor_proxy_ = std::make_unique<HostCursorProxy>(std::move(cursor_ui),
                                                    std::move(cursor_io));

  OnDrmServiceStarted();
}

void HostDrmDevice::OnGpuServiceLost() {
  cursor_proxy_.reset();
  connected_ = false;
  drm_device_.reset();
  // TODO(rjkroege): OnGpuThreadRetired is not currently used.
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

}  // namespace ui
