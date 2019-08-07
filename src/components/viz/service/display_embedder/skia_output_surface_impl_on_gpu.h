// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/latency/latency_tracker.h"

class SkDeferredDisplayList;

namespace base {
class WaitableEvent;
}

namespace gfx {
class ColorSpace;
}

namespace gl {
class GLApi;
class GLSurface;
}

namespace gpu {
class SyncPointClientState;
}

namespace ui {
#if defined(USE_OZONE)
class PlatformWindowSurface;
#endif
}  // namespace ui

namespace viz {

struct ImageContext;
class DirectContextProvider;
class GLRendererCopier;
class GpuServiceImpl;
class TextureDeleter;
class VulkanContextProvider;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// The SkiaOutputSurface implementation running on the GPU thread. This class
// should be created, used and destroyed on the GPU thread.
class SkiaOutputSurfaceImplOnGpu {
 public:
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  using BufferPresentedCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback& feedback)>;
  using ContextLostCallback = base::RepeatingCallback<void()>;

  SkiaOutputSurfaceImplOnGpu(
      GpuServiceImpl* gpu_service,
      gpu::SurfaceHandle surface_handle,
      const RendererSettings& renderer_settings_,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      const BufferPresentedCallback& buffer_presented_callback,
      const ContextLostCallback& context_lost_callback);

  ~SkiaOutputSurfaceImplOnGpu();

  gpu::CommandBufferId command_buffer_id() const {
    return sync_point_client_state_->command_buffer_id();
  }
  const OutputSurface::Capabilities capabilities() const {
    return output_device_->capabilities();
  }
  const base::WeakPtr<SkiaOutputSurfaceImplOnGpu>& weak_ptr() const {
    return weak_ptr_;
  }

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil,
               SkSurfaceCharacterization* characterization,
               base::WaitableEvent* event);
  void FinishPaintCurrentFrame(
      std::unique_ptr<SkDeferredDisplayList> ddl,
      std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
      std::vector<ImageContext*> image_contexts,
      std::vector<gpu::SyncToken> sync_tokens,
      uint64_t sync_fence_release,
      base::OnceClosure on_finished);
  void SwapBuffers(OutputSurfaceFrame frame);
  void EnsureBackbuffer() { output_device_->EnsureBackbuffer(); }
  void DiscardBackbuffer() { output_device_->DiscardBackbuffer(); }
  void FinishPaintRenderPass(RenderPassId id,
                             std::unique_ptr<SkDeferredDisplayList> ddl,
                             std::vector<ImageContext*> image_contexts,
                             std::vector<gpu::SyncToken> sync_tokens,
                             uint64_t sync_fence_release);
  void RemoveRenderPassResource(
      std::vector<std::unique_ptr<ImageContext>> image_contexts);
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request);

  void BeginAccessImages(const std::vector<ImageContext*>& image_contexts,
                         std::vector<GrBackendSemaphore>* begin_semaphores,
                         std::vector<GrBackendSemaphore>* end_semaphores);
  void EndAccessImages(const std::vector<ImageContext*>& image_contexts);

  sk_sp<GrContextThreadSafeProxy> GetGrContextThreadSafeProxy();
  const gl::GLVersionInfo* gl_version_info() const { return gl_version_info_; }

  void ReleaseSkImages(
      std::vector<std::unique_ptr<ImageContext>> image_contexts);

  bool was_context_lost() { return context_state_->context_lost(); }

  class ScopedUseContextProvider;

  void SetCapabilitiesForTesting(
      const OutputSurface::Capabilities& capabilities);

 private:
  class ScopedPromiseImageAccess;

  void InitializeForGLWithGpuService(GpuServiceImpl* gpu_service);
  void InitializeForVulkan(GpuServiceImpl* gpu_service);

  // Returns true if |texture_base| is a gles2::Texture and all necessary
  // operations completed successfully. In this case, |*size| is the size of
  // of level 0.
  bool BindOrCopyTextureIfNecessary(gpu::TextureBase* texture_base,
                                    gfx::Size* size);

  // Make context current for GL, and return false if the context is lost.
  // It will do nothing when Vulkan is used.
  bool MakeCurrent(bool need_fbo0);

  void PullTextureUpdates(std::vector<gpu::SyncToken> sync_token);

  void ReleaseFenceSyncAndPushTextureUpdates(uint64_t sync_fence_release);

  GrContext* gr_context() { return context_state_->gr_context(); }
  gpu::DecoderContext* decoder();

  void ScheduleDelayedWork();
  void PerformDelayedWork();

  bool is_using_vulkan() const { return !!vulkan_context_provider_; }

  SkSurface* output_sk_surface() const {
    return output_device_->draw_surface();
  }

  void CreateFallbackImage(ImageContext* context);

  const gpu::SurfaceHandle surface_handle_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  gpu::MailboxManager* const mailbox_manager_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  gpu::raster::GrShaderCache* const gr_shader_cache_;
  VulkanContextProvider* const vulkan_context_provider_;
  const RendererSettings renderer_settings_;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  BufferPresentedCallback buffer_presented_callback_;
  ContextLostCallback context_lost_callback_;

#if defined(USE_OZONE)
  // This should outlive gl_surface_ and vulkan_surface_.
  std::unique_ptr<ui::PlatformWindowSurface> window_surface_;
#endif

  gpu::GpuPreferences gpu_preferences_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  const gl::GLVersionInfo* gl_version_info_ = nullptr;

  std::unique_ptr<SkiaOutputDevice> output_device_;

  // Semaphore for SkiaOutputDevice::SwapBuffers() to wait on.
  GrBackendSemaphore swap_buffers_semaphore_;

  // Offscreen surfaces for render passes. It can only be accessed on GPU
  // thread.
  class OffscreenSurface {
   public:
    OffscreenSurface();
    OffscreenSurface(const OffscreenSurface& offscreen_surface) = delete;
    OffscreenSurface(OffscreenSurface&& offscreen_surface);
    OffscreenSurface& operator=(const OffscreenSurface& offscreen_surface) =
        delete;
    OffscreenSurface& operator=(OffscreenSurface&& offscreen_surface);
    ~OffscreenSurface();
    SkSurface* surface() const;
    sk_sp<SkPromiseImageTexture> fulfill();
    void set_surface(sk_sp<SkSurface> surface);

   private:
    sk_sp<SkSurface> surface_;
    sk_sp<SkPromiseImageTexture> promise_texture_;
  };
  base::flat_map<RenderPassId, OffscreenSurface> offscreen_surfaces_;

  ui::LatencyTracker latency_tracker_;

  scoped_refptr<base::SingleThreadTaskRunner> context_current_task_runner_;
  scoped_refptr<DirectContextProvider> context_provider_;
  std::unique_ptr<TextureDeleter> texture_deleter_;
  std::unique_ptr<GLRendererCopier> copier_;

  GpuServiceImpl* const gpu_service_;

  bool delayed_work_pending_ = false;

  gl::GLApi* api_ = nullptr;
  bool supports_alpha_ = false;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImplOnGpu> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImplOnGpu);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
