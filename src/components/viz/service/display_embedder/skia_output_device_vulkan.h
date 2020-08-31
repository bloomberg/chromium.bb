// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

namespace gpu {
class VulkanSurface;
}

namespace viz {

class VulkanContextProvider;

class SkiaOutputDeviceVulkan final : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceVulkan(
      util::PassKey<SkiaOutputDeviceVulkan>,
      VulkanContextProvider* context_provider,
      gpu::SurfaceHandle surface_handle,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceVulkan() override;

  static std::unique_ptr<SkiaOutputDeviceVulkan> Create(
      VulkanContextProvider* context_provider,
      gpu::SurfaceHandle surface_handle,
      gpu::MemoryTracker* memory_tracker,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);

#if defined(OS_WIN)
  gpu::SurfaceHandle GetChildSurfaceHandle();
#endif
  // SkiaOutputDevice implementation:
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;

 private:
  struct SkSurfaceSizePair {
   public:
    SkSurfaceSizePair();
    SkSurfaceSizePair(const SkSurfaceSizePair& other);
    ~SkSurfaceSizePair();
    sk_sp<SkSurface> sk_surface;
    uint64_t bytes_allocated = 0u;
  };

  bool Initialize();
  bool RecreateSwapChain(const gfx::Size& size,
                         sk_sp<SkColorSpace> color_space,
                         gfx::OverlayTransform transform);

  VulkanContextProvider* const context_provider_;

  const gpu::SurfaceHandle surface_handle_;
  std::unique_ptr<gpu::VulkanSurface> vulkan_surface_;

  base::Optional<gpu::VulkanSwapChain::ScopedWrite> scoped_write_;

#if DCHECK_IS_ON()
  bool image_modified_ = false;
#endif

  // SkSurfaces for swap chain images.
  std::vector<SkSurfaceSizePair> sk_surface_size_pairs_;

  sk_sp<SkColorSpace> color_space_;
  bool is_new_swapchain_ = true;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceVulkan);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_VULKAN_H_
