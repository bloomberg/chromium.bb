// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/mus_thread_proxy.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/display/types/display_snapshot_mojo.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/cursor_proxy_mojo.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

namespace ui {

MusThreadProxy::MusThreadProxy(DrmCursor* cursor,
                               service_manager::Connector* connector)
    : ws_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      drm_thread_(nullptr),
      cursor_(cursor),
      connector_(connector),
      weak_ptr_factory_(this) {
  // Bind the viz process pointer here.
  connector->BindInterface(ui::mojom::kServiceName, &gpu_adapter_);
}

MusThreadProxy::~MusThreadProxy() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

// TODO(rjkroege): Relocate to a viz process specific class.
void MusThreadProxy::SetDrmThread(DrmThread* thread) {
  base::AutoLock acquire(lock_);
  drm_thread_ = thread;
}

void MusThreadProxy::ProvideManagers(DrmDisplayHostManager* display_manager,
                                     DrmOverlayManager* overlay_manager) {
  display_manager_ = display_manager;
  overlay_manager_ = overlay_manager;
}

// TODO(rjkroege): Relocate to a viz process specific class.
void MusThreadProxy::StartDrmThread() {
  DCHECK(drm_thread_);
  drm_thread_->Start();

  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MusThreadProxy::DispatchObserversFromDrmThread,
                            base::Unretained(this)));
}

// TODO(rjkroege): Relocate to a viz process specific class.
void MusThreadProxy::DispatchObserversFromDrmThread() {
  ws_task_runner_->PostTask(FROM_HERE, base::Bind(&MusThreadProxy::RunObservers,
                                                  base::Unretained(this)));
}

void MusThreadProxy::RunObservers() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  for (GpuThreadObserver& observer : gpu_thread_observers_) {
    // TODO(rjkroege): This needs to be different when gpu process split
    // happens.
    observer.OnGpuProcessLaunched();
    observer.OnGpuThreadReady();
  }

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. This means that we need to special case it
  // and notify it after all other observers/handlers are notified.
  cursor_->SetDrmCursorProxy(base::MakeUnique<CursorProxyMojo>(connector_));

  // TODO(rjkroege): Call ResetDrmCursorProxy when the mojo connection to the
  // DRM thread is broken.
}

void MusThreadProxy::AddGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());

  gpu_thread_observers_.AddObserver(observer);
  if (IsConnected()) {
    // TODO(rjkroege): This needs to be different when gpu process split
    // happens.
    observer->OnGpuProcessLaunched();
    observer->OnGpuThreadReady();
  }
}

void MusThreadProxy::RemoveGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  gpu_thread_observers_.RemoveObserver(observer);
}

bool MusThreadProxy::IsConnected() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  base::AutoLock acquire(lock_);
  if (drm_thread_)
    return drm_thread_->IsRunning();
  return false;
}

// Services needed for DrmDisplayHostMananger.
void MusThreadProxy::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_ = handler;
}

void MusThreadProxy::UnRegisterHandlerForDrmDisplayHostManager() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_ = nullptr;
}

bool MusThreadProxy::GpuCreateWindow(gfx::AcceleratedWidget widget) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;

  gpu_adapter_->CreateWindow(widget);
  return true;
}

bool MusThreadProxy::GpuDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;

  gpu_adapter_->DestroyWindow(widget);
  return true;
}

bool MusThreadProxy::GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                            const gfx::Rect& bounds) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;

  gpu_adapter_->SetWindowBounds(widget, bounds);
  return true;
}

// Services needed for DrmOverlayManager.
void MusThreadProxy::RegisterHandlerForDrmOverlayManager(
    DrmOverlayManager* handler) {
  // TODO(rjkroege): Permit overlay manager to run in viz when the display
  // compositor runs in viz.
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_ = handler;
}

void MusThreadProxy::UnRegisterHandlerForDrmOverlayManager() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_ = nullptr;
}

bool MusThreadProxy::GpuCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& overlays) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;

  auto callback =
      base::BindOnce(&MusThreadProxy::GpuCheckOverlayCapabilitiesCallback,
                     weak_ptr_factory_.GetWeakPtr());

  gpu_adapter_->CheckOverlayCapabilities(widget, overlays, std::move(callback));
  return true;
}

bool MusThreadProxy::GpuRefreshNativeDisplays() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuRefreshNativeDisplaysCallback,
                     weak_ptr_factory_.GetWeakPtr());
  gpu_adapter_->RefreshNativeDisplays(std::move(callback));
  return true;
}

bool MusThreadProxy::GpuConfigureNativeDisplay(int64_t id,
                                               const DisplayMode_Params& pmode,
                                               const gfx::Point& origin) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;

  // TODO(rjkroege): Remove the use of mode here.
  auto mode = CreateDisplayModeFromParams(pmode);
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuConfigureNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());

  gpu_adapter_->ConfigureNativeDisplay(id, std::move(mode), origin,
                                       std::move(callback));
  return true;
}

bool MusThreadProxy::GpuDisableNativeDisplay(int64_t id) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuDisableNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());
  gpu_adapter_->DisableNativeDisplay(id, std::move(callback));
  return true;
}

bool MusThreadProxy::GpuTakeDisplayControl() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  auto callback = base::BindOnce(&MusThreadProxy::GpuTakeDisplayControlCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  gpu_adapter_->TakeDisplayControl(std::move(callback));
  return true;
}

bool MusThreadProxy::GpuRelinquishDisplayControl() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuRelinquishDisplayControlCallback,
                     weak_ptr_factory_.GetWeakPtr());
  gpu_adapter_->TakeDisplayControl(std::move(callback));
  return true;
}

bool MusThreadProxy::GpuAddGraphicsDevice(const base::FilePath& path,
                                          base::ScopedFD fd) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  base::File file(fd.release());
  gpu_adapter_->AddGraphicsDevice(path, std::move(file));
  return true;
}

bool MusThreadProxy::GpuRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  gpu_adapter_->RemoveGraphicsDevice(std::move(path));
  return true;
}

bool MusThreadProxy::GpuGetHDCPState(int64_t display_id) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  auto callback = base::BindOnce(&MusThreadProxy::GpuGetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  gpu_adapter_->GetHDCPState(display_id, std::move(callback));
  return true;
}

bool MusThreadProxy::GpuSetHDCPState(int64_t display_id,
                                     display::HDCPState state) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;
  auto callback = base::BindOnce(&MusThreadProxy::GpuSetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  gpu_adapter_->SetHDCPState(display_id, state, std::move(callback));
  return true;
}

bool MusThreadProxy::GpuSetColorCorrection(
    int64_t id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  if (!drm_thread_ || !drm_thread_->IsRunning())
    return false;

  gpu_adapter_->SetColorCorrection(id, degamma_lut, gamma_lut,
                                   correction_matrix);

  return true;
}

void MusThreadProxy::GpuCheckOverlayCapabilitiesCallback(
    const gfx::AcceleratedWidget& widget,
    const OverlaySurfaceCandidateList& overlays,
    const OverlayStatusList& returns) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_->GpuSentOverlayResult(widget, overlays, returns);
}

void MusThreadProxy::GpuConfigureNativeDisplayCallback(int64_t display_id,
                                                       bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

// TODO(rjkroege): Remove the unnecessary conversion back into params.
void MusThreadProxy::GpuRefreshNativeDisplaysCallback(
    std::vector<std::unique_ptr<display::DisplaySnapshotMojo>> displays) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuHasUpdatedNativeDisplays(
      CreateParamsFromSnapshot(displays));
}

void MusThreadProxy::GpuDisableNativeDisplayCallback(int64_t display_id,
                                                     bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void MusThreadProxy::GpuTakeDisplayControlCallback(bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuTookDisplayControl(success);
}

void MusThreadProxy::GpuRelinquishDisplayControlCallback(bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuRelinquishedDisplayControl(success);
}

void MusThreadProxy::GpuGetHDCPStateCallback(int64_t display_id,
                                             bool success,
                                             display::HDCPState state) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuReceivedHDCPState(display_id, success, state);
}

void MusThreadProxy::GpuSetHDCPStateCallback(int64_t display_id,
                                             bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuUpdatedHDCPState(display_id, success);
}

}  // namespace ui
