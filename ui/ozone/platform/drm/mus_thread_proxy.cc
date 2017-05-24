// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/mus_thread_proxy.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_thread.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

namespace ui {

CursorProxyThread::CursorProxyThread(MusThreadProxy* mus_thread_proxy)
    : mus_thread_proxy_(mus_thread_proxy) {}
CursorProxyThread::~CursorProxyThread() {}

void CursorProxyThread::CursorSet(gfx::AcceleratedWidget window,
                                  const std::vector<SkBitmap>& bitmaps,
                                  const gfx::Point& point,
                                  int frame_delay_ms) {
  mus_thread_proxy_->CursorSet(window, bitmaps, point, frame_delay_ms);
}
void CursorProxyThread::Move(gfx::AcceleratedWidget window,
                             const gfx::Point& point) {
  mus_thread_proxy_->Move(window, point);
}
void CursorProxyThread::InitializeOnEvdev() {
  mus_thread_proxy_->InitializeOnEvdev();
}

MusThreadProxy::MusThreadProxy()
    : ws_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      drm_thread_(nullptr),
      weak_ptr_factory_(this) {}

void MusThreadProxy::InitializeOnEvdev() {}

MusThreadProxy::~MusThreadProxy() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadRetired();
}

// This is configured on the GPU thread.
void MusThreadProxy::SetDrmThread(DrmThread* thread) {
  base::AutoLock acquire(lock_);
  drm_thread_ = thread;
}

void MusThreadProxy::ProvideManagers(DrmDisplayHostManager* display_manager,
                                     DrmOverlayManager* overlay_manager) {
  display_manager_ = display_manager;
  overlay_manager_ = overlay_manager;
}

void MusThreadProxy::StartDrmThread() {
  DCHECK(drm_thread_);
  drm_thread_->Start();

  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&MusThreadProxy::DispatchObserversFromDrmThread,
                            base::Unretained(this)));
}

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
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::CreateWindow,
                            base::Unretained(drm_thread_), widget));
  return true;
}

bool MusThreadProxy::GpuDestroyWindow(gfx::AcceleratedWidget widget) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::DestroyWindow,
                            base::Unretained(drm_thread_), widget));
  return true;
}

bool MusThreadProxy::GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                            const gfx::Rect& bounds) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::SetWindowBounds,
                            base::Unretained(drm_thread_), widget, bounds));
  return true;
}

void MusThreadProxy::CursorSet(gfx::AcceleratedWidget widget,
                               const std::vector<SkBitmap>& bitmaps,
                               const gfx::Point& location,
                               int frame_delay_ms) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetCursor, base::Unretained(drm_thread_), widget,
                 bitmaps, location, frame_delay_ms));
}

void MusThreadProxy::Move(gfx::AcceleratedWidget widget,
                          const gfx::Point& location) {
  // NOTE: Input events skip the main thread to avoid jank.
  DCHECK(drm_thread_->IsRunning());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::MoveCursor,
                            base::Unretained(drm_thread_), widget, location));
}

// Services needed for DrmOverlayManager.
void MusThreadProxy::RegisterHandlerForDrmOverlayManager(
    DrmOverlayManager* handler) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_ = handler;
}

void MusThreadProxy::UnRegisterHandlerForDrmOverlayManager() {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_ = nullptr;
}

bool MusThreadProxy::GpuCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuCheckOverlayCapabilitiesCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::CheckOverlayCapabilities,
                                base::Unretained(drm_thread_), widget, overlays,
                                std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuRefreshNativeDisplays() {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuRefreshNativeDisplaysCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RefreshNativeDisplays,
                     base::Unretained(drm_thread_), std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuConfigureNativeDisplay(int64_t id,
                                               const DisplayMode_Params& pmode,
                                               const gfx::Point& origin) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());

  auto mode = CreateDisplayModeFromParams(pmode);
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuConfigureNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::ConfigureNativeDisplay,
                     base::Unretained(drm_thread_), id, std::move(mode), origin,
                     std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuDisableNativeDisplay(int64_t id) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuDisableNativeDisplayCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DrmThread::DisableNativeDisplay,
                                base::Unretained(drm_thread_), id,
                                std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuTakeDisplayControl() {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback = base::BindOnce(&MusThreadProxy::GpuTakeDisplayControlCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::TakeDisplayControl,
                     base::Unretained(drm_thread_), std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuRelinquishDisplayControl() {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback =
      base::BindOnce(&MusThreadProxy::GpuRelinquishDisplayControlCallback,
                     weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::RelinquishDisplayControl,
                     base::Unretained(drm_thread_), std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuAddGraphicsDevice(const base::FilePath& path,
                                          const base::FileDescriptor& fd) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::AddGraphicsDevice,
                            base::Unretained(drm_thread_), path, fd));
  return true;
}

bool MusThreadProxy::GpuRemoveGraphicsDevice(const base::FilePath& path) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&DrmThread::RemoveGraphicsDevice,
                            base::Unretained(drm_thread_), path));
  return true;
}

bool MusThreadProxy::GpuGetHDCPState(int64_t display_id) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  auto callback = base::BindOnce(&MusThreadProxy::GpuGetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::GetHDCPState, base::Unretained(drm_thread_),
                     display_id, std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuSetHDCPState(int64_t display_id,
                                     display::HDCPState state) {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  DCHECK(drm_thread_->IsRunning());
  auto callback = base::BindOnce(&MusThreadProxy::GpuSetHDCPStateCallback,
                                 weak_ptr_factory_.GetWeakPtr());
  auto safe_callback = CreateSafeOnceCallback(std::move(callback));
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::SetHDCPState, base::Unretained(drm_thread_),
                     display_id, state, std::move(safe_callback)));
  return true;
}

bool MusThreadProxy::GpuSetColorCorrection(
    int64_t id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  DCHECK(drm_thread_->IsRunning());
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  drm_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DrmThread::SetColorCorrection, base::Unretained(drm_thread_),
                 id, degamma_lut, gamma_lut, correction_matrix));
  return true;
}

void MusThreadProxy::GpuCheckOverlayCapabilitiesCallback(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  overlay_manager_->GpuSentOverlayResult(widget, overlays);
}

void MusThreadProxy::GpuConfigureNativeDisplayCallback(int64_t display_id,
                                                       bool success) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuConfiguredDisplay(display_id, success);
}

void MusThreadProxy::GpuRefreshNativeDisplaysCallback(
    const std::vector<DisplaySnapshot_Params>& displays) const {
  DCHECK(on_window_server_thread_.CalledOnValidThread());
  display_manager_->GpuHasUpdatedNativeDisplays(displays);
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
