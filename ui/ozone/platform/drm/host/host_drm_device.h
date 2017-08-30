// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/drm_device.mojom.h"

namespace display {
class DisplaySnapshot;
}

namespace service_manager {
class Connector;
}

namespace ui {
class DrmDisplayHostManager;
class DrmOverlayManager;
class GpuThreadObserver;

// This is the Viz host-side library for the DRM device service provided by the
// viz process.
class HostDrmDevice : public GpuThreadAdapter {
 public:
  HostDrmDevice(DrmCursor* cursor, service_manager::Connector* connector);
  ~HostDrmDevice() override;

  // Start the DRM service. Runs the |OnDrmServiceStartedCallback| when the
  // service has launched and initiates the remaining startup.
  void AsyncStartDrmDevice();

  // Blocks until the DRM service has come up. Use this entry point only when
  // supporting launch of the service where the ozone UI and GPU
  // reponsibilities are performed by the same underlying thread.
  void BlockingStartDrmDevice();

  void ProvideManagers(DrmDisplayHostManager* display_manager,
                       DrmOverlayManager* overlay_manager);

  // GpuThreadAdapter
  void AddGpuThreadObserver(GpuThreadObserver* observer) override;
  void RemoveGpuThreadObserver(GpuThreadObserver* observer) override;
  bool IsConnected() override;

  // Services needed for DrmDisplayHostMananger.
  void RegisterHandlerForDrmDisplayHostManager(
      DrmDisplayHostManager* handler) override;
  void UnRegisterHandlerForDrmDisplayHostManager() override;

  bool GpuTakeDisplayControl() override;
  bool GpuRefreshNativeDisplays() override;
  bool GpuRelinquishDisplayControl() override;
  bool GpuAddGraphicsDevice(const base::FilePath& path,
                            base::ScopedFD fd) override;
  bool GpuRemoveGraphicsDevice(const base::FilePath& path) override;

  // Services needed for DrmOverlayManager.
  void RegisterHandlerForDrmOverlayManager(DrmOverlayManager* handler) override;
  void UnRegisterHandlerForDrmOverlayManager() override;
  bool GpuCheckOverlayCapabilities(
      gfx::AcceleratedWidget widget,
      const OverlaySurfaceCandidateList& new_params) override;

  // Services needed by DrmDisplayHost
  bool GpuConfigureNativeDisplay(int64_t display_id,
                                 const ui::DisplayMode_Params& display_mode,
                                 const gfx::Point& point) override;
  bool GpuDisableNativeDisplay(int64_t display_id) override;
  bool GpuGetHDCPState(int64_t display_id) override;
  bool GpuSetHDCPState(int64_t display_id, display::HDCPState state) override;
  bool GpuSetColorCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut,
      const std::vector<float>& correction_matrix) override;

  // Services needed by DrmWindowHost
  bool GpuDestroyWindow(gfx::AcceleratedWidget widget) override;
  bool GpuCreateWindow(gfx::AcceleratedWidget widget) override;
  bool GpuWindowBoundsChanged(gfx::AcceleratedWidget widget,
                              const gfx::Rect& bounds) override;

 private:
  void OnDrmServiceStartedCallback(bool success);
  void PollForSingleThreadReady(int previous_delay);
  void RunObservers();

  void GpuCheckOverlayCapabilitiesCallback(
      const gfx::AcceleratedWidget& widget,
      const OverlaySurfaceCandidateList& overlays,
      const OverlayStatusList& returns) const;

  void GpuConfigureNativeDisplayCallback(int64_t display_id,
                                         bool success) const;

  void GpuRefreshNativeDisplaysCallback(
      std::vector<std::unique_ptr<display::DisplaySnapshot>> displays) const;
  void GpuDisableNativeDisplayCallback(int64_t display_id, bool success) const;
  void GpuTakeDisplayControlCallback(bool success) const;
  void GpuRelinquishDisplayControlCallback(bool success) const;
  void GpuGetHDCPStateCallback(int64_t display_id,
                               bool success,
                               display::HDCPState state) const;
  void GpuSetHDCPStateCallback(int64_t display_id, bool success) const;

  // Mojo implementation of the DrmDevice.
  ui::ozone::mojom::DrmDevicePtr drm_device_ptr_;

  DrmDisplayHostManager* display_manager_;  // Not owned.
  DrmOverlayManager* overlay_manager_;      // Not owned.
  DrmCursor* cursor_;                       // Not owned.

  service_manager::Connector* connector_;
  THREAD_CHECKER(on_window_server_thread_);

  bool connected_ = false;
  base::ObserverList<GpuThreadObserver> gpu_thread_observers_;

  base::WeakPtrFactory<HostDrmDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostDrmDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_HOST_DRM_DEVICE_H_
