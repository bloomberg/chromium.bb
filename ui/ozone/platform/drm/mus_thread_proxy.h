// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_MUS_THREAD_PROXY_H_
#define UI_OZONE_PLATFORM_DRM_MUS_THREAD_PROXY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/drm/gpu/inter_thread_messaging_proxy.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/gpu_thread_adapter.h"
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/gpu_adapter.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace display {
class DisplaySnapshotMojo;
}

namespace service_manager {
class Connector;
}

namespace ui {

class DrmDisplayHostManager;
class DrmOverlayManager;
class DrmThread;
class GpuThreadObserver;
class MusThreadProxy;

// TODO(rjkroege): Originally we had planned on running the window server, gpu,
// compositor and drm threads together in the same process. However, system
// security requires separating event handling and embedding decisions (the
// window server) into a process separate from the viz server
// (//services/viz/README.md). The separated implementation remains incomplete
// and will be completed in subsequent CLs. At that point, this class will
// become the viz host for ozone services and a separate class will contain the
// viz service (DRM interface.)
class MusThreadProxy : public GpuThreadAdapter,
                       public InterThreadMessagingProxy {
 public:
  MusThreadProxy(DrmCursor* cursor, service_manager::Connector* connector);
  ~MusThreadProxy() override;

  void StartDrmThread();
  void ProvideManagers(DrmDisplayHostManager* display_manager,
                       DrmOverlayManager* overlay_manager);

  // InterThreadMessagingProxy.
  // TODO(rjkroege): Remove when mojo everywhere.
  void SetDrmThread(DrmThread* thread) override;

  // This is the core functionality. They are invoked when we have a main
  // thread, a gpu thread and we have called initialize on both.
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
  void RunObservers();
  void DispatchObserversFromDrmThread();

  void GpuCheckOverlayCapabilitiesCallback(
      const gfx::AcceleratedWidget& widget,
      const OverlaySurfaceCandidateList& overlays,
      const OverlayStatusList& returns) const;

  void GpuConfigureNativeDisplayCallback(int64_t display_id,
                                         bool success) const;

  void GpuRefreshNativeDisplaysCallback(
      std::vector<std::unique_ptr<display::DisplaySnapshotMojo>> displays)
      const;
  void GpuDisableNativeDisplayCallback(int64_t display_id, bool success) const;
  void GpuTakeDisplayControlCallback(bool success) const;
  void GpuRelinquishDisplayControlCallback(bool success) const;
  void GpuGetHDCPStateCallback(int64_t display_id,
                               bool success,
                               display::HDCPState state) const;
  void GpuSetHDCPStateCallback(int64_t display_id, bool success) const;

  scoped_refptr<base::SingleThreadTaskRunner> ws_task_runner_;

  // Mojo implementation of the GpuAdapter.
  ui::ozone::mojom::GpuAdapterPtr gpu_adapter_;

  // TODO(rjkroege): Remove this in a subsequent CL (http://crbug.com/620927)
  DrmThread* drm_thread_;  // Not owned.

  // Guards  for multi-theaded access to drm_thread_.
  base::Lock lock_;

  DrmDisplayHostManager* display_manager_;  // Not owned.
  DrmOverlayManager* overlay_manager_;      // Not owned.
  DrmCursor* cursor_;                       // Not owned.

  service_manager::Connector* connector_;

  base::ObserverList<GpuThreadObserver> gpu_thread_observers_;

  base::ThreadChecker on_window_server_thread_;

  base::WeakPtrFactory<MusThreadProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MusThreadProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_MUS_THREAD_PROXY_H_
