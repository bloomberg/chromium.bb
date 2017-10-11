// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/host_drm_device.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"
#include "ui/ozone/platform/drm/host/host_cursor_proxy.h"

namespace ui {

HostDrmDevice::HostDrmDevice(DrmCursor* cursor,
                             service_manager::Connector* connector)
    : cursor_(cursor), connector_(connector), weak_ptr_factory_(this) {
  // Bind the viz process pointer here.
  // TODO(rjkroege): Reconnect on error as that would indicate that the Viz
  // process has failed.
  connector->BindInterface(ui::mojom::kServiceName, &drm_device_ptr_);
}

HostDrmDevice::~HostDrmDevice() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

void HostDrmDevice::AsyncStartDrmDevice() {
  auto callback = base::BindOnce(&HostDrmDevice::OnDrmServiceStartedCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->StartDrmDevice(std::move(callback));
}

void HostDrmDevice::BlockingStartDrmDevice() {
  // Wait until startup related tasks posted to this thread that must precede
  // blocking on
  base::RunLoop().RunUntilIdle();

  bool success;
  drm_device_ptr_->StartDrmDevice(&success);
  CHECK(success)
      << "drm thread failed to successfully start in single process mode.";
  if (!connected_)
    OnDrmServiceStartedCallback(true);
  return;
}

void HostDrmDevice::OnDrmServiceStartedCallback(bool success) {
  // This can be called multiple times in the course of single-threaded startup.
  if (connected_)
    return;
  if (success == true) {
    connected_ = true;
    RunObservers();
  }
  // TODO(rjkroege): Handle failure of launching a viz process.
}

void HostDrmDevice::ProvideManagers(DrmDisplayHostManager* display_manager,
                                    DrmOverlayManager* overlay_manager) {
  display_manager_ = display_manager;
  overlay_manager_ = overlay_manager;
}

void HostDrmDevice::RunObservers() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  for (GpuThreadObserver& observer : gpu_thread_observers_) {
    observer.OnGpuProcessLaunched();
    observer.OnGpuThreadReady();
  }

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. This means that we need to special case it
  // and notify it after all other observers/handlers are notified.
  cursor_->SetDrmCursorProxy(base::MakeUnique<HostCursorProxy>(connector_));

  // TODO(rjkroege): Call ResetDrmCursorProxy when the mojo connection to the
  // DRM thread is broken.
}

void HostDrmDevice::AddGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  gpu_thread_observers_.AddObserver(observer);
  if (IsConnected()) {
    observer->OnGpuProcessLaunched();
    observer->OnGpuThreadReady();
  }
}

void HostDrmDevice::RemoveGpuThreadObserver(GpuThreadObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  gpu_thread_observers_.RemoveObserver(observer);
}

bool HostDrmDevice::IsConnected() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);

  // TODO(rjkroege): Need to set to connected_ to false when we lose the Viz
  // process connection.
  return connected_;
}

// Services needed for DrmDisplayHostMananger.
void HostDrmDevice::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_ = handler;
}

void HostDrmDevice::UnRegisterHandlerForDrmDisplayHostManager() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_ = nullptr;
}

bool HostDrmDevice::GpuCreateWindow(gfx::AcceleratedWidget widget) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;

  drm_device_ptr_->CreateWindow(widget);
  return true;
}

bool HostDrmDevice::GpuDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;

  drm_device_ptr_->DestroyWindow(widget);
  return true;
}

bool HostDrmDevice::GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                           const gfx::Rect& bounds) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;

  drm_device_ptr_->SetWindowBounds(widget, bounds);
  return true;
}

// Services needed for DrmOverlayManager.
void HostDrmDevice::RegisterHandlerForDrmOverlayManager(
    DrmOverlayManager* handler) {
  // TODO(rjkroege): Permit overlay manager to run in viz when the display
  // compositor runs in viz.
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  overlay_manager_ = handler;
}

void HostDrmDevice::UnRegisterHandlerForDrmOverlayManager() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  overlay_manager_ = nullptr;
}

bool HostDrmDevice::GpuCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& overlays) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;

  auto callback =
      base::BindOnce(&HostDrmDevice::GpuCheckOverlayCapabilitiesCallback,
                     weak_ptr_factory_.GetWeakPtr());

  drm_device_ptr_->CheckOverlayCapabilities(widget, overlays,
                                            std::move(callback));
  return true;
}

bool HostDrmDevice::GpuRefreshNativeDisplays() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuRefreshNativeDisplaysCallback,
                     weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->RefreshNativeDisplays(std::move(callback));
  return true;
}

bool HostDrmDevice::GpuConfigureNativeDisplay(int64_t id,
                                              const DisplayMode_Params& pmode,
                                              const gfx::Point& origin) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;

  // TODO(rjkroege): Remove the use of mode here.
  auto mode = CreateDisplayModeFromParams(pmode);
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuConfigureNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());

  drm_device_ptr_->ConfigureNativeDisplay(id, std::move(mode), origin,
                                          std::move(callback));
  return true;
}

bool HostDrmDevice::GpuDisableNativeDisplay(int64_t id) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuDisableNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->DisableNativeDisplay(id, std::move(callback));
  return true;
}

bool HostDrmDevice::GpuTakeDisplayControl() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuTakeDisplayControlCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->TakeDisplayControl(std::move(callback));
  return true;
}

bool HostDrmDevice::GpuRelinquishDisplayControl() {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  auto callback =
      base::BindOnce(&HostDrmDevice::GpuRelinquishDisplayControlCallback,
                     weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->TakeDisplayControl(std::move(callback));
  return true;
}

bool HostDrmDevice::GpuAddGraphicsDevice(const base::FilePath& path,
                                         base::ScopedFD fd) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  base::File file(fd.release());
  drm_device_ptr_->AddGraphicsDevice(path, std::move(file));
  return true;
}

bool HostDrmDevice::GpuRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  drm_device_ptr_->RemoveGraphicsDevice(std::move(path));
  return true;
}

bool HostDrmDevice::GpuGetHDCPState(int64_t display_id) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuGetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->GetHDCPState(display_id, std::move(callback));
  return true;
}

bool HostDrmDevice::GpuSetHDCPState(int64_t display_id,
                                    display::HDCPState state) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;
  auto callback = base::BindOnce(&HostDrmDevice::GpuSetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  drm_device_ptr_->SetHDCPState(display_id, state, std::move(callback));
  return true;
}

bool HostDrmDevice::GpuSetColorCorrection(
    int64_t id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  if (!IsConnected())
    return false;

  drm_device_ptr_->SetColorCorrection(id, degamma_lut, gamma_lut,
                                      correction_matrix);

  return true;
}

void HostDrmDevice::GpuCheckOverlayCapabilitiesCallback(
    const gfx::AcceleratedWidget& widget,
    const OverlaySurfaceCandidateList& overlays,
    const OverlayStatusList& returns) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  overlay_manager_->GpuSentOverlayResult(widget, overlays, returns);
}

void HostDrmDevice::GpuConfigureNativeDisplayCallback(int64_t display_id,
                                                      bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

// TODO(rjkroege): Remove the unnecessary conversion back into params.
void HostDrmDevice::GpuRefreshNativeDisplaysCallback(
    std::vector<std::unique_ptr<display::DisplaySnapshot>> displays) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuHasUpdatedNativeDisplays(
      CreateDisplaySnapshotParams(displays));
}

void HostDrmDevice::GpuDisableNativeDisplayCallback(int64_t display_id,
                                                    bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void HostDrmDevice::GpuTakeDisplayControlCallback(bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuTookDisplayControl(success);
}

void HostDrmDevice::GpuRelinquishDisplayControlCallback(bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuRelinquishedDisplayControl(success);
}

void HostDrmDevice::GpuGetHDCPStateCallback(int64_t display_id,
                                            bool success,
                                            display::HDCPState state) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuReceivedHDCPState(display_id, success, state);
}

void HostDrmDevice::GpuSetHDCPStateCallback(int64_t display_id,
                                            bool success) const {
  DCHECK_CALLED_ON_VALID_THREAD(on_window_server_thread_);
  display_manager_->GpuUpdatedHDCPState(display_id, success);
}

}  // namespace ui
