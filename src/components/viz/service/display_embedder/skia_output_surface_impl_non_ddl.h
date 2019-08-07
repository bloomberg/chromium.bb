// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_NON_DDL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_NON_DDL_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/viz/service/display_embedder/skia_output_surface_base.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/vulkan/buildflags.h"

#if BUILDFLAG(ENABLE_VULKAN)
class GrVkSecondaryCBDrawContext;
#endif

namespace gfx {
struct PresentationFeedback;
}

namespace gl {
class GLSurface;
}

namespace gpu {
class MailboxManager;
class SharedImageManager;
class SharedImageRepresentationFactory;
class SyncPointClientState;
class SyncPointManager;
class SyncPointOrderData;
}  // namespace gpu

namespace viz {

// A SkiaOutputSurface implementation for running SkiaRenderer on GpuThread.
// Comparing to SkiaOutputSurfaceImpl, it will issue skia draw operations
// against OS graphics API (GL, Vulkan, etc) instead of recording deferred
// display list first.
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceImplNonDDL
    : public SkiaOutputSurfaceBase {
 public:
  SkiaOutputSurfaceImplNonDDL(
      scoped_refptr<gl::GLSurface> gl_surface,
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageManager* shared_image_manager,
      gpu::SyncPointManager* sync_point_manager,
      bool need_swapbuffers_ack);
  ~SkiaOutputSurfaceImplNonDDL() override;

  // OutputSurface implementation:
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ResourceMetadata>& metadatas,
      SkYUVColorSpace yuv_color_space,
      sk_sp<SkColorSpace> dst_color_space,
      bool has_alpha) override;
  void SkiaSwapBuffers(OutputSurfaceFrame frame) override;
  SkCanvas* BeginPaintRenderPass(const RenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap,
                                 sk_sp<SkColorSpace> color_space) override;
  gpu::SyncToken SubmitPaint(base::OnceClosure on_finished) override;
  sk_sp<SkImage> MakePromiseSkImage(const ResourceMetadata& metadata) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const RenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;
  void RemoveRenderPassResource(std::vector<RenderPassId> ids) override;
  void CopyOutput(RenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request) override;

  // ExternalUseClient implementation:
  void ReleaseCachedResources(const std::vector<ResourceId>& ids) override;

 private:
  class ScopedGpuTask {
   public:
    explicit ScopedGpuTask(gpu::SyncPointOrderData* sync_point_order_data);
    ~ScopedGpuTask();

   private:
    gpu::SyncPointOrderData* const sync_point_order_data_;
    const uint32_t order_num_;

    DISALLOW_COPY_AND_ASSIGN(ScopedGpuTask);
  };

  GrContext* gr_context() { return shared_context_state_->gr_context(); }

  bool WaitSyncToken(const gpu::SyncToken& sync_token);
  std::unique_ptr<ImageContext> MakeSkImageFromSharedImage(
      const ResourceMetadata& metadata);
  bool GetGrBackendTexture(const ResourceMetadata& metadata,
                           GrBackendTexture* backend_texture);
  void FinishPaint(uint64_t sync_fence_release);
  void BufferPresented(const gfx::PresentationFeedback& feedback);
  void WaitSemaphores(std::vector<GrBackendSemaphore> semaphores);

  uint64_t sync_fence_release_ = 0;

  // Stuffs for running with |task_executor_| instead of |gpu_service_|.
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  gpu::MailboxManager* const mailbox_manager_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory> sir_factory_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  const bool need_swapbuffers_ack_;
  base::Optional<ScopedGpuTask> scoped_gpu_task_;

  unsigned int backing_framebuffer_object_ = 0;
  gfx::Size reshape_surface_size_;
  float reshape_device_scale_factor_ = 0.f;
  gfx::ColorSpace reshape_color_space_;
  bool reshape_has_alpha_ = false;
  bool reshape_use_stencil_ = false;

  // The current render pass id set by BeginPaintRenderPass.
  RenderPassId current_render_pass_id_ = 0;

  // The SkSurface for the framebuffer.
  sk_sp<SkSurface> sk_surface_;

#if BUILDFLAG(ENABLE_VULKAN)
  // The |draw_context_| for the current frame.
  GrVkSecondaryCBDrawContext* draw_context_ = nullptr;
#endif

  SkSurface* sk_current_surface_ = nullptr;

  // Offscreen SkSurfaces for render passes.
  base::flat_map<RenderPassId, sk_sp<SkSurface>> offscreen_sk_surfaces_;

  // Semaphores which need to be signalled for the current paint.
  std::vector<GrBackendSemaphore> pending_semaphores_;

  base::WeakPtrFactory<SkiaOutputSurfaceImplNonDDL> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImplNonDDL);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_NON_DDL_H_
