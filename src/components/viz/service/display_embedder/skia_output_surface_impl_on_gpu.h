// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_render_copy_results.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/service/display_context.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class ColorSpace;
}

namespace gl {
class GLSurface;
}

namespace gpu {
class SharedImageRepresentationFactory;
class SharedImageFactory;
class SyncPointClientState;
}  // namespace gpu

namespace ui {
#if defined(USE_OZONE)
class PlatformWindowSurface;
#endif
}  // namespace ui

namespace viz {

class AsyncReadResultHelper;
class AsyncReadResultLock;
class DawnContextProvider;
class ImageContextImpl;
class VulkanContextProvider;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// The SkiaOutputSurface implementation running on the GPU thread. This class
// should be created, used and destroyed on the GPU thread.
class SkiaOutputSurfaceImplOnGpu
    : public gpu::ImageTransportSurfaceDelegate,
      public gpu::SharedContextState::ContextLostObserver {
 public:
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size,
                                   gfx::GpuFenceHandle release_fence)>;
  using BufferPresentedCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback& feedback)>;
  using ContextLostCallback = base::OnceClosure;

  // |gpu_vsync_callback| must be safe to call on any thread. The other
  // callbacks will only be called via |deps->PostTaskToClientThread|.
  static std::unique_ptr<SkiaOutputSurfaceImplOnGpu> Create(
      SkiaOutputSurfaceDependency* deps,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      GpuVSyncCallback gpu_vsync_callback);

  SkiaOutputSurfaceImplOnGpu(
      base::PassKey<SkiaOutputSurfaceImplOnGpu> pass_key,
      SkiaOutputSurfaceDependency* deps,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      GpuVSyncCallback gpu_vsync_callback);

  SkiaOutputSurfaceImplOnGpu(const SkiaOutputSurfaceImplOnGpu&) = delete;
  SkiaOutputSurfaceImplOnGpu& operator=(const SkiaOutputSurfaceImplOnGpu&) =
      delete;

  ~SkiaOutputSurfaceImplOnGpu() override;

  gpu::CommandBufferId command_buffer_id() const {
    return shared_gpu_deps_->command_buffer_id();
  }

  const OutputSurface::Capabilities& capabilities() const {
    return output_device_->capabilities();
  }
  const base::WeakPtr<SkiaOutputSurfaceImplOnGpu>& weak_ptr() const {
    return weak_ptr_;
  }
  gl::GLSurface* gl_surface() const { return gl_surface_.get(); }

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               bool use_stencil,
               gfx::OverlayTransform transform);
  void FinishPaintCurrentFrame(sk_sp<SkDeferredDisplayList> ddl,
                               sk_sp<SkDeferredDisplayList> overdraw_ddl,
                               std::vector<ImageContextImpl*> image_contexts,
                               std::vector<gpu::SyncToken> sync_tokens,
                               base::OnceClosure on_finished,
                               absl::optional<gfx::Rect> draw_rectangle,
                               bool allocate_frame_buffer);
  void ScheduleOutputSurfaceAsOverlay(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane&
          output_surface_plane);
  void SwapBuffers(OutputSurfaceFrame frame, bool release_frame_buffer);
  void AllocateFrameBuffers(size_t n);
  void ReleaseFrameBuffers(size_t n);

  void SetDependenciesResolvedTimings(base::TimeTicks task_ready);
  void SetDrawTimings(base::TimeTicks task_ready);

  // Runs |deferred_framebuffer_draw_closure| when SwapBuffers() or CopyOutput()
  // will not.
  void SwapBuffersSkipped();
  void EnsureBackbuffer();
  void DiscardBackbuffer();
  void FinishPaintRenderPass(const gpu::Mailbox& mailbox,
                             sk_sp<SkDeferredDisplayList> ddl,
                             std::vector<ImageContextImpl*> image_contexts,
                             std::vector<gpu::SyncToken> sync_tokens,
                             base::OnceClosure on_finished);
  // Deletes resources for RenderPasses in |ids|. Also takes ownership of
  // |images_contexts| and destroys them on GPU thread.
  void RemoveRenderPassResource(
      std::vector<AggregatedRenderPassId> ids,
      std::vector<std::unique_ptr<ImageContextImpl>> image_contexts);
  void CopyOutput(AggregatedRenderPassId id,
                  copy_output::RenderPassGeometry geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request,
                  const gpu::Mailbox& mailbox);

  void BeginAccessImages(const std::vector<ImageContextImpl*>& image_contexts,
                         std::vector<GrBackendSemaphore>* begin_semaphores,
                         std::vector<GrBackendSemaphore>* end_semaphores);
  void ResetStateOfImages();
  void EndAccessImages(const base::flat_set<ImageContextImpl*>& image_contexts);

  sk_sp<GrContextThreadSafeProxy> GetGrContextThreadSafeProxy();
  size_t max_resource_cache_bytes() const { return max_resource_cache_bytes_; }
  void ReleaseImageContexts(
      std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
          image_contexts);
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays,
                        std::vector<ImageContextImpl*> image_contexts,
                        base::OnceClosure on_finished);

  void SetEnableDCLayers(bool enable);
  void SetGpuVSyncEnabled(bool enabled);

  void SetFrameRate(float frame_rate);
  bool was_context_lost() { return context_state_->context_lost(); }

  void SetCapabilitiesForTesting(
      const OutputSurface::Capabilities& capabilities);

  bool IsDisplayedAsOverlay();

  // gpu::SharedContextState::ContextLostObserver implementation:
  void OnContextLost() override;

  // gpu::ImageTransportSurfaceDelegate implementation:
#if defined(OS_WIN)
  void DidCreateAcceleratedSurfaceChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  const gpu::gles2::FeatureInfo* GetFeatureInfo() const override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params,
                              gfx::GpuFenceHandle release_fence) override;
  void BufferPresented(const gfx::PresentationFeedback& feedback) override;
  GpuVSyncCallback GetGpuVSyncCallback() override;
  base::TimeDelta GetGpuBlockedTimeSinceLastSwap() override;

  void PostTaskToClientThread(base::OnceClosure closure) {
    dependency_->PostTaskToClientThread(std::move(closure));
  }

  void ReadbackDone() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_GT(num_readbacks_pending_, 0);
    num_readbacks_pending_--;
  }

  // Make context current for GL, and return false if the context is lost.
  // It will do nothing when Vulkan is used.
  bool MakeCurrent(bool need_framebuffer);

  void ReleaseFenceSync(uint64_t sync_fence_release);

  void PreserveChildSurfaceControls();

  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver);

  const scoped_refptr<AsyncReadResultLock> GetAsyncReadResultLock() const;

  void AddAsyncReadResultHelperWithLock(AsyncReadResultHelper* helper);
  void RemoveAsyncReadResultHelperWithLock(AsyncReadResultHelper* helper);

 private:
  class DisplayContext;

  struct PlaneAccessData {
    PlaneAccessData();
    PlaneAccessData(PlaneAccessData&& other);
    PlaneAccessData& operator=(PlaneAccessData&& other);
    ~PlaneAccessData();

    SkISize size;
    gpu::Mailbox mailbox;
    std::unique_ptr<gpu::SharedImageRepresentationSkia> representation;
    std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedWriteAccess>
        scoped_write;

    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
  };

  bool Initialize();
  bool InitializeForGL();
  bool InitializeForVulkan();
  bool InitializeForDawn();

  // Provided as a callback to |device_|.
  void DidSwapBuffersCompleteInternal(gpu::SwapBuffersCompleteParams params,
                                      const gfx::Size& pixel_size,
                                      gfx::GpuFenceHandle release_fence);

  DidSwapBufferCompleteCallback GetDidSwapBuffersCompleteCallback();

  void MarkContextLost(ContextLostReason reason);

  void RunDestroyCopyOutputResourcesOnGpuThread(
      ReleaseCallback* callback,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  void DestroyCopyOutputResourcesOnGpuThread(
      std::unique_ptr<gpu::SharedImageRepresentationSkia> representation,
      scoped_refptr<gpu::SharedContextState> context_state,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  void SwapBuffersInternal(absl::optional<OutputSurfaceFrame> frame);
  void PostSubmit(absl::optional<OutputSurfaceFrame> frame);

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

  bool is_using_vulkan() const {
    return !!vulkan_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kVulkan;
  }
  bool is_using_dawn() const {
    return !!dawn_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kDawn;
  }

  // Helper for `CopyOutput()` method, handles the RGBA format.
  void CopyOutputRGBA(SkSurface* surface,
                      copy_output::RenderPassGeometry geometry,
                      const gfx::ColorSpace& color_space,
                      const SkIRect& src_rect,
                      SkSurface::RescaleMode rescale_mode,
                      bool is_downscale_or_identity_in_both_dimensions,
                      std::unique_ptr<CopyOutputRequest> request);

  void CopyOutputRGBAInMemory(SkSurface* surface,
                              copy_output::RenderPassGeometry geometry,
                              const gfx::ColorSpace& color_space,
                              const SkIRect& src_rect,
                              SkSurface::RescaleMode rescale_mode,
                              bool is_downscale_or_identity_in_both_dimensions,
                              std::unique_ptr<CopyOutputRequest> request);

  void CopyOutputNV12(SkSurface* surface,
                      copy_output::RenderPassGeometry geometry,
                      const gfx::ColorSpace& color_space,
                      const SkIRect& src_rect,
                      SkSurface::RescaleMode rescale_mode,
                      bool is_downscale_or_identity_in_both_dimensions,
                      std::unique_ptr<CopyOutputRequest> request);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods:
  std::unique_ptr<gpu::SharedImageRepresentationSkia>
  CreateSharedImageRepresentationSkia(ResourceFormat resource_format,
                                      const gfx::Size& size,
                                      const gfx::ColorSpace& color_space);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods, renders
  // |surface| into |dest_surface|'s canvas, cropping and scaling the results
  // appropriately. |source_selection| is the area of the |surface| that will be
  // rendered to the destination.
  // |begin_semaphores| will be submitted to the GPU backend prior to issuing
  // draw calls to the |dest_surface|.
  // |end_semaphores| will be submitted to the GPU backend alongside the draw
  // calls to the |dest_surface|.
  bool RenderSurface(SkSurface* surface,
                     const SkIRect& source_selection,
                     absl::optional<SkVector> scaling,
                     bool is_downscale_or_identity_in_both_dimensions,
                     SkSurface* dest_surface,
                     std::vector<GrBackendSemaphore>& begin_semaphores,
                     std::vector<GrBackendSemaphore>& end_semaphores);

  // Creates surfaces needed to store the data in NV12 format.
  // |plane_access_datas| will be populated with information needed to access
  // the NV12 planes.
  bool CreateSurfacesForNV12Planes(
      const SkYUVAInfo& yuva_info,
      const gfx::ColorSpace& color_space,
      std::array<PlaneAccessData, CopyOutputResult::kNV12MaxPlanes>&
          plane_access_datas);
  // Imports surfaces needed to store the data in NV12 format from a blit
  // request. |plane_access_datas| will be populated with information needed to
  // access the NV12 planes.
  bool ImportSurfacesForNV12Planes(
      const BlitRequest& blit_request,
      std::array<PlaneAccessData, CopyOutputResult::kNV12MaxPlanes>&
          plane_access_datas);

  // Schedules a task to check if any skia readback requests have completed
  // after a short delay. Will not schedule a task if there is already a
  // scheduled task or no readback requests are pending.
  void ScheduleCheckReadbackCompletion();

  // Checks if any skia readback requests have completed. If there are still
  // pending readback requests after checking then it will reschedule itself
  // after a short delay.
  void CheckReadbackCompletion();

  void ReleaseAsyncReadResultHelpers();

#if defined(OS_APPLE) || defined(USE_OZONE)
  std::unique_ptr<gpu::SharedImageRepresentationSkia>
  GetOrCreateRenderPassOverlayBacking(
      const SkSurfaceCharacterization& characterization);
#endif

  class ReleaseCurrent {
   public:
    ReleaseCurrent(scoped_refptr<gl::GLSurface> gl_surface,
                   scoped_refptr<gpu::SharedContextState> context_state);
    ~ReleaseCurrent();

   private:
    scoped_refptr<gl::GLSurface> gl_surface_;
    scoped_refptr<gpu::SharedContextState> context_state_;
  };

  // This must remain the first member variable to ensure that other member
  // dtors are called first.
  absl::optional<ReleaseCurrent> release_current_last_;

  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  raw_ptr<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu> shared_gpu_deps_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  const raw_ptr<VulkanContextProvider> vulkan_context_provider_;
  const raw_ptr<DawnContextProvider> dawn_context_provider_;
  const RendererSettings renderer_settings_;

  // Should only be run on the client thread with PostTaskToClientThread().
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  BufferPresentedCallback buffer_presented_callback_;
  ContextLostCallback context_lost_callback_;
  GpuVSyncCallback gpu_vsync_callback_;

  // ImplOnGpu::CopyOutput can create SharedImages via ImplOnGpu's
  // SharedImageFactory. Clients can use these images via CopyOutputResult and
  // when done, release the resources by invoking the provided callback. If
  // ImplOnGpu is already destroyed, however, there is no way of running the
  // release callback from the client, so this vector holds all pending release
  // callbacks so resources can still be cleaned up in the dtor.
  std::vector<std::unique_ptr<ReleaseCallback>> release_on_gpu_callbacks_;

  // Helper, creates a release callback for the passed in |representation|.
  ReleaseCallback CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
      std::unique_ptr<gpu::SharedImageRepresentationSkia> representation);

#if defined(USE_OZONE)
  // This should outlive gl_surface_ and vulkan_surface_.
  std::unique_ptr<ui::PlatformWindowSurface> window_surface_;
#endif

  gpu::GpuPreferences gpu_preferences_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  size_t max_resource_cache_bytes_ = 0u;

  std::unique_ptr<DisplayContext> display_context_;
  bool context_is_lost_ = false;

  class PromiseImageAccessHelper {
   public:
    explicit PromiseImageAccessHelper(SkiaOutputSurfaceImplOnGpu* impl_on_gpu);

    PromiseImageAccessHelper(const PromiseImageAccessHelper&) = delete;
    PromiseImageAccessHelper& operator=(const PromiseImageAccessHelper&) =
        delete;

    ~PromiseImageAccessHelper();

    void BeginAccess(std::vector<ImageContextImpl*> image_contexts,
                     std::vector<GrBackendSemaphore>* begin_semaphores,
                     std::vector<GrBackendSemaphore>* end_semaphores);
    void EndAccess();

   private:
    const raw_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;
    base::flat_set<ImageContextImpl*> image_contexts_;
  };
  PromiseImageAccessHelper promise_image_access_helper_{this};
  base::flat_set<ImageContextImpl*> image_contexts_with_end_access_state_;

  std::unique_ptr<SkiaOutputDevice> output_device_;
  std::unique_ptr<SkiaOutputDevice::ScopedPaint> scoped_output_device_paint_;

  absl::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
      output_surface_plane_;

  // Micro-optimization to get to issuing GPU SwapBuffers as soon as possible.
  std::vector<sk_sp<SkDeferredDisplayList>> destroy_after_swap_;

  bool waiting_for_full_damage_ = false;

  int num_readbacks_pending_ = 0;
  bool readback_poll_pending_ = false;

  // Lock for |async_read_result_helpers_|.
  scoped_refptr<AsyncReadResultLock> async_read_result_lock_;

  // Tracking for ongoing AsyncReadResults.
  base::flat_set<AsyncReadResultHelper*> async_read_result_helpers_;

#if defined(OS_APPLE) || defined(USE_OZONE)
  using UniqueBackingPtr = std::unique_ptr<gpu::SharedImageRepresentationSkia>;
  class BackingComparator {
   public:
    using is_transparent = void;
    bool operator()(const UniqueBackingPtr& lhs,
                    const UniqueBackingPtr& rhs) const {
      return lhs->mailbox() < rhs->mailbox();
    }
    bool operator()(const UniqueBackingPtr& lhs,
                    const gpu::Mailbox& rhs) const {
      return lhs->mailbox() < rhs;
    }
    bool operator()(const gpu::Mailbox& lhs,
                    const UniqueBackingPtr& rhs) const {
      return lhs < rhs->mailbox();
    }
  };
  // Render pass overlay backings are in flight.
  // The base::flat_set uses backing->mailbox() as the unique key.
  base::flat_set<UniqueBackingPtr, BackingComparator>
      in_flight_render_pass_overlay_backings_;

  // Render pass overlay backings are available for reusing.
  std::vector<std::unique_ptr<gpu::SharedImageRepresentationSkia>>
      available_render_pass_overlay_backings_;
#endif

  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImplOnGpu> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
