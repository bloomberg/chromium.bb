// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/gpu_adapter.mojom.h"
#include "ui/ozone/public/swap_completion_callback.h"

namespace base {
class FilePath;
}

namespace display {
class DisplayMode;
struct GammaRampRGBEntry;
}

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class DrmDeviceManager;
class DrmGpuDisplayManager;
class GbmBuffer;
class ScanoutBufferGenerator;
class ScreenManager;

struct OverlayPlane;

// Holds all the DRM related state and performs all DRM related operations.
//
// The DRM thread is used to insulate DRM operations from potential blocking
// behaviour on the GPU main thread in order to reduce the potential for jank
// (for example jank in the cursor if the GPU main thread is performing heavy
// operations). The inverse is also true as blocking operations on the DRM
// thread (such as modesetting) no longer block the GPU main thread.
class DrmThread : public base::Thread,
                  public ozone::mojom::DeviceCursor,
                  public ozone::mojom::GpuAdapter {
 public:
  DrmThread();
  ~DrmThread() override;

  void Start();

  // Must be called on the DRM thread.
  void CreateBuffer(gfx::AcceleratedWidget widget,
                    const gfx::Size& size,
                    gfx::BufferFormat format,
                    gfx::BufferUsage usage,
                    scoped_refptr<GbmBuffer>* buffer);
  void CreateBufferFromFds(gfx::AcceleratedWidget widget,
                           const gfx::Size& size,
                           gfx::BufferFormat format,
                           std::vector<base::ScopedFD>&& fds,
                           const std::vector<gfx::NativePixmapPlane>& planes,
                           scoped_refptr<GbmBuffer>* buffer);

  void GetScanoutFormats(gfx::AcceleratedWidget widget,
                         std::vector<gfx::BufferFormat>* scanout_formats);
  void SchedulePageFlip(gfx::AcceleratedWidget widget,
                        const std::vector<OverlayPlane>& planes,
                        SwapCompletionOnceCallback callback);
  void GetVSyncParameters(
      gfx::AcceleratedWidget widget,
      const gfx::VSyncProvider::UpdateVSyncCallback& callback);

  void CreateWindow(const gfx::AcceleratedWidget& widget) override;
  void DestroyWindow(const gfx::AcceleratedWidget& widget) override;
  void SetWindowBounds(const gfx::AcceleratedWidget& widget,
                       const gfx::Rect& bounds) override;
  void SetCursor(const gfx::AcceleratedWidget& widget,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& location,
                 int32_t frame_delay_ms) override;
  void MoveCursor(const gfx::AcceleratedWidget& widget,
                  const gfx::Point& location) override;
  void CheckOverlayCapabilities(
      const gfx::AcceleratedWidget& widget,
      const OverlaySurfaceCandidateList& overlays,
      base::OnceCallback<void(const gfx::AcceleratedWidget&,
                              const OverlaySurfaceCandidateList&,
                              const OverlayStatusList&)> callback) override;
  void RefreshNativeDisplays(
      base::OnceCallback<void(MovableDisplaySnapshots)> callback) override;
  void ConfigureNativeDisplay(
      int64_t id,
      std::unique_ptr<display::DisplayMode> mode,
      const gfx::Point& origin,
      base::OnceCallback<void(int64_t, bool)> callback) override;
  void DisableNativeDisplay(
      int64_t id,
      base::OnceCallback<void(int64_t, bool)> callback) override;
  void TakeDisplayControl(base::OnceCallback<void(bool)> callback) override;
  void RelinquishDisplayControl(
      base::OnceCallback<void(bool)> callback) override;
  void AddGraphicsDevice(const base::FilePath& path, base::File file) override;
  void RemoveGraphicsDevice(const base::FilePath& path) override;
  void GetHDCPState(int64_t display_id,
                    base::OnceCallback<void(int64_t, bool, display::HDCPState)>
                        callback) override;
  void SetHDCPState(int64_t display_id,
                    display::HDCPState state,
                    base::OnceCallback<void(int64_t, bool)> callback) override;
  void SetColorCorrection(
      int64_t display_id,
      const std::vector<display::GammaRampRGBEntry>& degamma_lut,
      const std::vector<display::GammaRampRGBEntry>& gamma_lut,
      const std::vector<float>& correction_matrix) override;

  // base::Thread:
  void Init() override;

  // Mojo support for DeviceCursorRequest.
  void AddBinding(ozone::mojom::DeviceCursorRequest request);

  // Mojo support for GpuAdapter requests.
  void AddBindingGpu(ozone::mojom::GpuAdapterRequest request);

 private:
  std::unique_ptr<DrmDeviceManager> device_manager_;
  std::unique_ptr<ScanoutBufferGenerator> buffer_generator_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmGpuDisplayManager> display_manager_;

  // The mojo implementation requires a BindingSet because the DrmThread serves
  // requests from two different client threads.
  mojo::BindingSet<ozone::mojom::DeviceCursor> bindings_;

  // The mojo implementation of GpuAdapter can use a simple binding.
  mojo::Binding<ozone::mojom::GpuAdapter> binding_;

  DISALLOW_COPY_AND_ASSIGN(DrmThread);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_THREAD_H_
