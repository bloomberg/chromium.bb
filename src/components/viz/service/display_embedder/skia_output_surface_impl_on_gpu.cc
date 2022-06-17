// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include <memory>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/crash_logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/blit_request.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/output_presenter_gl.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/display_embedder/skia_output_device_webview.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_render_copy_results.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "skia/ext/legacy_display_globals.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkDeferredDisplayList.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/viz/service/display_embedder/skia_output_device_vulkan_secondary_cb.h"
#endif
#endif

#if defined(USE_OZONE)
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_OZONE_PLATFORM_X11
#endif
#endif

#if (BUILDFLAG(ENABLE_VULKAN) || BUILDFLAG(SKIA_USE_DAWN)) && \
    defined(USE_OZONE_PLATFORM_X11)
#include "components/viz/service/display_embedder/skia_output_device_x11.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "components/viz/common/gpu/dawn_context_provider.h"
#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display_embedder/skia_output_device_dawn.h"
#endif
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "components/viz/service/display_embedder/output_presenter_fuchsia.h"
#endif

namespace viz {

namespace {

template <typename... Args>
void PostAsyncTaskRepeatedly(
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
    const base::RepeatingCallback<void(Args...)>& callback,
    Args... args) {
  // Callbacks generated by this function may be executed asynchronously
  // (e.g. by presentation feedback) after |impl_on_gpu| has been destroyed.
  if (impl_on_gpu)
    impl_on_gpu->PostTaskToClientThread(base::BindOnce(callback, args...));
}

template <typename... Args>
base::RepeatingCallback<void(Args...)> CreateSafeRepeatingCallback(
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
    const base::RepeatingCallback<void(Args...)>& callback) {
  return base::BindRepeating(&PostAsyncTaskRepeatedly<Args...>, impl_on_gpu,
                             callback);
}

void FailedSkiaFlush(base::StringPiece msg) {
  static auto* kCrashKey = base::debug::AllocateCrashKeyString(
      "sk_flush_failed", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(kCrashKey, msg);
  LOG(ERROR) << msg;
}

#if BUILDFLAG(ENABLE_VULKAN)
// Returns whether SkiaOutputDeviceX11 can be instantiated on this platform.
bool MayFallBackToSkiaOutputDeviceX11() {
#if defined(USE_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .skia_can_fall_back_to_x11;
#else
  return false;
#endif  // defined(USE_OZONE)
}
#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace

SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::PromiseImageAccessHelper(
    SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
    : impl_on_gpu_(impl_on_gpu) {}

SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::
    ~PromiseImageAccessHelper() {
  DCHECK(image_contexts_.empty() || impl_on_gpu_->was_context_lost());
}

void SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::BeginAccess(
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // GL doesn't need semaphores.
  if (!impl_on_gpu_->context_state_->GrContextIsGL()) {
    DCHECK(begin_semaphores);
    DCHECK(end_semaphores);
    begin_semaphores->reserve(image_contexts.size());
    // We may need one more space for the swap buffer semaphore.
    end_semaphores->reserve(image_contexts.size() + 1);
  }
  image_contexts_.reserve(image_contexts.size() + image_contexts_.size());
  image_contexts_.insert(image_contexts.begin(), image_contexts.end());
  impl_on_gpu_->BeginAccessImages(std::move(image_contexts), begin_semaphores,
                                  end_semaphores);
}

void SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::EndAccess() {
  impl_on_gpu_->EndAccessImages(image_contexts_);
  image_contexts_.clear();
}

namespace {

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    SkiaOutputSurfaceDependency* deps,
    gpu::CommandBufferId command_buffer_id,
    gpu::SequenceId sequence_id) {
  return deps->GetSyncPointManager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      sequence_id);
}

std::unique_ptr<gpu::SharedImageFactory> CreateSharedImageFactory(
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker) {
  return std::make_unique<gpu::SharedImageFactory>(
      deps->GetGpuPreferences(), deps->GetGpuDriverBugWorkarounds(),
      deps->GetGpuFeatureInfo(), deps->GetSharedContextState().get(),
      deps->GetMailboxManager(), deps->GetSharedImageManager(),
      deps->GetGpuImageFactory(), memory_tracker,
      /*enable_wrapped_sk_image=*/true,
      /*is_for_display_compositor=*/true);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(SkiaOutputSurfaceDependency* deps,
                                       gpu::MemoryTracker* memory_tracker) {
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      deps->GetSharedImageManager(), memory_tracker);
}

}  // namespace

SkiaOutputSurfaceImplOnGpu::ReleaseCurrent::ReleaseCurrent(
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::SharedContextState> context_state)
    : gl_surface_(gl_surface), context_state_(context_state) {}

SkiaOutputSurfaceImplOnGpu::ReleaseCurrent::~ReleaseCurrent() {
  if (context_state_ && gl_surface_)
    context_state_->ReleaseCurrent(gl_surface_.get());
}

class SkiaOutputSurfaceImplOnGpu::DisplayContext : public gpu::DisplayContext {
 public:
  DisplayContext(SkiaOutputSurfaceDependency* deps,
                 SkiaOutputSurfaceImplOnGpu* owner)
      : dependency_(deps), owner_(owner) {
    dependency_->RegisterDisplayContext(this);
  }
  ~DisplayContext() override { dependency_->UnregisterDisplayContext(this); }

  DisplayContext(const DisplayContext&) = delete;
  DisplayContext& operator=(const DisplayContext&) = delete;

  // gpu::DisplayContext implementation
  void MarkContextLost() override {
    owner_->MarkContextLost(CONTEXT_LOST_UNKNOWN);
  }

 private:
  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  const raw_ptr<SkiaOutputSurfaceImplOnGpu> owner_;
};

// static
std::unique_ptr<SkiaOutputSurfaceImplOnGpu> SkiaOutputSurfaceImplOnGpu::Create(
    SkiaOutputSurfaceDependency* deps,
    const RendererSettings& renderer_settings,
    const gpu::SequenceId sequence_id,
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    BufferPresentedCallback buffer_presented_callback,
    ContextLostCallback context_lost_callback,
    GpuVSyncCallback gpu_vsync_callback) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::Create");

  auto context_state = deps->GetSharedContextState();
  if (!context_state)
    return nullptr;

  // Even with Vulkan/Dawn compositing, the SharedImageFactory constructor
  // always initializes a GL-backed SharedImage factory to fall back on.
  // Creating the SharedImageBackingFactoryGLTexture invokes GL API calls, so
  // we need to ensure there is a current GL context.
  if (!context_state->MakeCurrent(nullptr, true /* need_gl */)) {
    LOG(ERROR) << "Failed to make current during initialization.";
    return nullptr;
  }
  context_state->set_need_context_state_reset(true);

  auto impl_on_gpu = std::make_unique<SkiaOutputSurfaceImplOnGpu>(
      base::PassKey<SkiaOutputSurfaceImplOnGpu>(), deps,
      context_state->feature_info(), renderer_settings, sequence_id,
      shared_gpu_deps, std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(gpu_vsync_callback));
  if (!impl_on_gpu->Initialize())
    return nullptr;

  return impl_on_gpu;
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    base::PassKey<SkiaOutputSurfaceImplOnGpu> /* pass_key */,
    SkiaOutputSurfaceDependency* deps,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    const RendererSettings& renderer_settings,
    const gpu::SequenceId sequence_id,
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    BufferPresentedCallback buffer_presented_callback,
    ContextLostCallback context_lost_callback,
    GpuVSyncCallback gpu_vsync_callback)
    : dependency_(std::move(deps)),
      shared_gpu_deps_(shared_gpu_deps),
      feature_info_(std::move(feature_info)),
      sync_point_client_state_(
          CreateSyncPointClientState(dependency_,
                                     shared_gpu_deps_->command_buffer_id(),
                                     sequence_id)),
      shared_image_factory_(
          CreateSharedImageFactory(dependency_,
                                   shared_gpu_deps_->memory_tracker())),
      shared_image_representation_factory_(
          CreateSharedImageRepresentationFactory(
              dependency_,
              shared_gpu_deps_->memory_tracker())),
      vulkan_context_provider_(dependency_->GetVulkanContextProvider()),
      dawn_context_provider_(dependency_->GetDawnContextProvider()),
      renderer_settings_(renderer_settings),
      did_swap_buffer_complete_callback_(
          std::move(did_swap_buffer_complete_callback)),
      context_lost_callback_(std::move(context_lost_callback)),
      gpu_vsync_callback_(std::move(gpu_vsync_callback)),
      gpu_preferences_(dependency_->GetGpuPreferences()),
      display_context_(std::make_unique<DisplayContext>(deps, this)),
      async_read_result_lock_(base::MakeRefCounted<AsyncReadResultLock>()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  buffer_presented_callback_ = CreateSafeRepeatingCallback(
      weak_ptr_, std::move(buffer_presented_callback));
}

void SkiaOutputSurfaceImplOnGpu::ReleaseAsyncReadResultHelpers() {
  base::AutoLock auto_lock(async_read_result_lock_->lock());
  for (auto* helper : async_read_result_helpers_)
    helper->reset();
  async_read_result_helpers_.clear();
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We need to have context current or lost during the destruction.
  bool has_context = false;
  if (context_state_) {
    context_state_->RemoveContextLostObserver(this);
    has_context = MakeCurrent(/*need_framebuffer=*/false);
    if (has_context) {
      release_current_last_.emplace(gl_surface_, context_state_);
    }
  }

  for (auto& callback : release_on_gpu_callbacks_)
    std::move(*callback).Run(gpu::SyncToken(), /*is_lost=*/true);

  // |output_device_| may still need |shared_image_factory_|, so release it
  // first.
  output_device_.reset();

  // Since SharedImageFactory also has a reference to ImplOnGpu's member
  // SharedContextState, we need to explicitly invoke the factory's destructor
  // before deleting ImplOnGpu's other member variables.
  shared_image_factory_.reset();
  if (has_context) {
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }
    // This ensures any outstanding callbacks for promise images are
    // performed.
    gr_context()->flushAndSubmit();
  }

  sync_point_client_state_->Destroy();

  // Release all ongoing AsyncReadResults.
  ReleaseAsyncReadResultHelpers();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(
    const SkSurfaceCharacterization& characterization,
    const gfx::ColorSpace& color_space,
    float device_scale_factor,
    gfx::OverlayTransform transform) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::Reshape");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gr_context());

  if (context_is_lost_)
    return;

  size_ = gfx::SkISizeToSize(characterization.dimensions());
  if (!output_device_->Reshape(characterization, color_space,
                               device_scale_factor, transform)) {
    MarkContextLost(CONTEXT_LOST_RESHAPE_FAILED);
  }
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    sk_sp<SkDeferredDisplayList> ddl,
    sk_sp<SkDeferredDisplayList> overdraw_ddl,
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
    absl::optional<gfx::Rect> draw_rectangle) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!scoped_output_device_paint_);

  if (context_is_lost_)
    return;

  if (!ddl) {
    MarkContextLost(CONTEXT_LOST_UNKNOWN);
    return;
  }

  if (draw_rectangle) {
    if (!output_device_->SetDrawRectangle(*draw_rectangle)) {
      MarkContextLost(
          ContextLostReason::CONTEXT_LOST_SET_DRAW_RECTANGLE_FAILED);
      return;
    }
  }

  // We do not reset scoped_output_device_paint_ after drawing the ddl until
  // SwapBuffers() is called, because we may need access to output_sk_surface()
  // for CopyOutput().
  scoped_output_device_paint_ = output_device_->BeginScopedPaint();
  if (!scoped_output_device_paint_) {
    MarkContextLost(ContextLostReason::CONTEXT_LOST_BEGIN_PAINT_FAILED);
    return;
  }

  dependency_->ScheduleGrContextCleanup();

  {
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }

    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    promise_image_access_helper_.BeginAccess(
        std::move(image_contexts), &begin_semaphores, &end_semaphores);
    if (!begin_semaphores.empty()) {
      auto result = scoped_output_device_paint_->Wait(
          begin_semaphores.size(), begin_semaphores.data(),
          /*delete_semaphores_after_wait=*/false);
      DCHECK(result);
    }

    // Draw will only fail if the SkSurface and SkDDL are incompatible.
    bool draw_success = scoped_output_device_paint_->Draw(ddl);
#if defined(USE_OZONE)
    if (!draw_success)
      DLOG(ERROR) << "output_sk_surface()->draw() failed.";
#else
    DCHECK(draw_success);
#endif  // USE_OZONE

    destroy_after_swap_.emplace_back(std::move(ddl));

    if (overdraw_ddl) {
      sk_sp<SkSurface> overdraw_surface = SkSurface::MakeRenderTarget(
          gr_context(), overdraw_ddl->characterization(), SkBudgeted::kNo);
      overdraw_surface->draw(overdraw_ddl);
      destroy_after_swap_.emplace_back(std::move(overdraw_ddl));

      SkPaint paint;
      sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

      sk_sp<SkColorFilter> colorFilter = SkiaHelper::MakeOverdrawColorFilter();
      paint.setColorFilter(colorFilter);
      // TODO(xing.xu): move below to the thread where skia record happens.
      scoped_output_device_paint_->GetCanvas()->drawImage(
          overdraw_image.get(), 0, 0, SkSamplingOptions(), &paint);
    }

    auto end_paint_semaphores =
        scoped_output_device_paint_->TakeEndPaintSemaphores();
    end_semaphores.insert(end_semaphores.end(), end_paint_semaphores.begin(),
                          end_paint_semaphores.end());

#if BUILDFLAG(ENABLE_VULKAN)
    // Semaphores for release fences for vulkan should be created before flush.
    if (!return_release_fence_cb.is_null() && is_using_vulkan()) {
      const bool result = CreateAndStoreExternalSemaphoreVulkan(end_semaphores);
      // A release fence will be created on submit as some platforms may use
      // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT handle types for their
      // external semaphore. That handle type has COPY transference. Vulkan spec
      // says that semaphore has to be signaled, or have an associated semaphore
      // signal operation pending execution. Thus, delay importing the handle
      // and creating the fence until commands are submitted.
      pending_release_fence_cbs_.emplace_back(
          result ? end_semaphores.back() : GrBackendSemaphore(),
          std::move(return_release_fence_cb));
    }
#endif

    const bool end_semaphores_empty = end_semaphores.empty();

    auto result = scoped_output_device_paint_->Flush(vulkan_context_provider_,
                                                     std::move(end_semaphores),
                                                     std::move(on_finished));

    if (result != GrSemaphoresSubmitted::kYes &&
        !(begin_semaphores.empty() && end_semaphores_empty)) {
      // TODO(penghuang): handle vulkan device lost.
      FailedSkiaFlush("output_sk_surface()->flush() failed.");
      return;
    }

    gfx::GpuFenceHandle release_fence;
    if (!return_release_fence_cb.is_null() && is_using_gl()) {
      DCHECK(release_fence.is_null());
      release_fence = CreateReleaseFenceForGL();
    }

    if (!return_release_fence_cb.is_null() && is_using_dawn())
      NOTIMPLEMENTED() << "Release fences with dawn are not supported.";

    if (!return_release_fence_cb.is_null()) {
      // Returning fences for Vulkan is delayed. See the comment above.
      DCHECK(!is_using_vulkan());
      PostTaskToClientThread(base::BindOnce(std::move(return_release_fence_cb),
                                            std::move(release_fence)));
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane&
        output_surface_plane) {
  DCHECK(!output_surface_plane_);
  output_surface_plane_ = output_surface_plane;
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(OutputSurfaceFrame frame) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::SwapBuffers");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SwapBuffersInternal(std::move(frame));
}

void SkiaOutputSurfaceImplOnGpu::EnsureMinNumberOfBuffers(int n) {
  if (!output_device_->EnsureMinNumberOfBuffers(n)) {
    MarkContextLost(CONTEXT_LOST_ALLOCATE_FRAME_BUFFERS_FAILED);
  }
}

void SkiaOutputSurfaceImplOnGpu::SetDependenciesResolvedTimings(
    base::TimeTicks task_ready) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  output_device_->SetDependencyTimings(task_ready);
}

void SkiaOutputSurfaceImplOnGpu::SetDrawTimings(base::TimeTicks task_posted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  output_device_->SetDrawTimings(task_posted, base::TimeTicks::Now());
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffersSkipped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SwapBuffersInternal(absl::nullopt);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    const gpu::Mailbox& mailbox,
    sk_sp<SkDeferredDisplayList> ddl,
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);

  if (context_is_lost_)
    return;

  if (!ddl) {
    MarkContextLost(CONTEXT_LOST_UNKNOWN);
    return;
  }

  auto backing_representation =
      shared_image_representation_factory_->ProduceSkia(mailbox,
                                                        context_state_.get());
  DCHECK(backing_representation);

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  const auto& characterization = ddl->characterization();
  auto scoped_access = backing_representation->BeginScopedWriteAccess(
      characterization.sampleCount(), characterization.surfaceProps(),
      &begin_semaphores, &end_semaphores,
      gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!scoped_access) {
    MarkContextLost(CONTEXT_LOST_UNKNOWN);
    return;
  }

  SkSurface* surface = scoped_access->surface();
  DCHECK(surface);

  {
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }
    promise_image_access_helper_.BeginAccess(
        std::move(image_contexts), &begin_semaphores, &end_semaphores);
    if (!begin_semaphores.empty()) {
      auto result =
          surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                        /*deleteSemaphoresAfterWait=*/false);
      DCHECK(result);
    }
    surface->draw(ddl);
    backing_representation->SetCleared();
    destroy_after_swap_.emplace_back(std::move(ddl));

#if BUILDFLAG(ENABLE_VULKAN)
    // Semaphores for release fences for vulkan should be created before flush.
    if (!return_release_fence_cb.is_null() && is_using_vulkan()) {
      const bool result = CreateAndStoreExternalSemaphoreVulkan(end_semaphores);
      // A release fence will be created on submit as some platforms may use
      // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT handle types for their
      // external semaphore. That handle type has COPY transference. Vulkan spec
      // says that semaphore has to be signaled, or have an associated semaphore
      // signal operation pending execution. Thus, delay importing the handle
      // and creating the fence until commands are submitted.
      pending_release_fence_cbs_.emplace_back(
          result ? end_semaphores.back() : GrBackendSemaphore(),
          std::move(return_release_fence_cb));
    }
#endif

    GrFlushInfo flush_info = {
        .fNumSemaphores = end_semaphores.size(),
        .fSignalSemaphores = end_semaphores.data(),
    };
    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_,
                                          &flush_info);
    if (on_finished)
      gpu::AddCleanupTaskForSkiaFlush(std::move(on_finished), &flush_info);

    auto result = surface->flush(flush_info);
    if (result != GrSemaphoresSubmitted::kYes &&
        !(begin_semaphores.empty() && end_semaphores.empty())) {
      // TODO(penghuang): handle vulkan device lost.
      FailedSkiaFlush("offscreen.surface()->flush() failed.");
      return;
    }

    // If GL is used, create the release fence after flush.
    gfx::GpuFenceHandle release_fence;
    if (!return_release_fence_cb.is_null() && is_using_gl()) {
      DCHECK(release_fence.is_null());
      release_fence = CreateReleaseFenceForGL();
    }

    if (!return_release_fence_cb.is_null() && is_using_dawn())
      NOTIMPLEMENTED() << "Release fences with dawn are not supported.";

    if (!return_release_fence_cb.is_null()) {
      // Returning fences for Vulkan is delayed. See the comment above.
      DCHECK(!is_using_vulkan());
      PostTaskToClientThread(base::BindOnce(std::move(return_release_fence_cb),
                                            std::move(release_fence)));
    }

    bool sync_cpu =
        gpu::ShouldVulkanSyncCpuForSkiaSubmit(vulkan_context_provider_);
    if (sync_cpu) {
      gr_context()->submit(true);
    }
  }
}

std::unique_ptr<gpu::SharedImageRepresentationSkia>
SkiaOutputSurfaceImplOnGpu::CreateSharedImageRepresentationSkia(
    ResourceFormat resource_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space) {
  constexpr uint32_t kUsage = gpu::SHARED_IMAGE_USAGE_GLES2 |
                              gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                              gpu::SHARED_IMAGE_USAGE_RASTER |
                              gpu::SHARED_IMAGE_USAGE_DISPLAY;

  gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();
  bool result = shared_image_factory_->CreateSharedImage(
      mailbox, resource_format, size, color_space, kBottomLeft_GrSurfaceOrigin,
      kUnpremul_SkAlphaType, gpu::kNullSurfaceHandle, kUsage);
  if (!result) {
    DLOG(ERROR) << "Failed to create shared image.";
    return nullptr;
  }

  auto representation = dependency_->GetSharedImageManager()->ProduceSkia(
      mailbox, context_state_->memory_type_tracker(), context_state_);
  shared_image_factory_->DestroySharedImage(mailbox);

  return representation;
}

void SkiaOutputSurfaceImplOnGpu::CopyOutputRGBAInMemory(
    SkSurface* surface,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    const SkIRect& src_rect,
    SkSurface::RescaleMode rescale_mode,
    bool is_downscale_or_identity_in_both_dimensions,
    std::unique_ptr<CopyOutputRequest> request) {
  // If we can't convert |color_space| to a SkColorSpace (e.g. PIECEWISE_HDR),
  // request a sRGB destination color space for the copy result instead.
  gfx::ColorSpace dest_color_space = color_space;
  sk_sp<SkColorSpace> sk_color_space = color_space.ToSkColorSpace();
  if (!sk_color_space) {
    dest_color_space = gfx::ColorSpace::CreateSRGB();
  }
  SkImageInfo dst_info = SkImageInfo::Make(
      geometry.result_selection.width(), geometry.result_selection.height(),
      kN32_SkColorType, kPremul_SkAlphaType, sk_color_space);
  std::unique_ptr<ReadPixelsContext> context =
      std::make_unique<ReadPixelsContext>(std::move(request),
                                          geometry.result_selection,
                                          dest_color_space, weak_ptr_);
  // Skia readback could be synchronous. Incremement counter in case
  // ReadbackCompleted is called immediately.
  num_readbacks_pending_++;
  surface->asyncRescaleAndReadPixels(
      dst_info, src_rect, SkSurface::RescaleGamma::kSrc, rescale_mode,
      &CopyOutputResultSkiaRGBA::OnReadbackDone, context.release());
}

void SkiaOutputSurfaceImplOnGpu::CopyOutputRGBA(
    SkSurface* surface,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    const SkIRect& src_rect,
    SkSurface::RescaleMode rescale_mode,
    bool is_downscale_or_identity_in_both_dimensions,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_EQ(request->result_format(), CopyOutputRequest::ResultFormat::RGBA);

  switch (request->result_destination()) {
    case CopyOutputRequest::ResultDestination::kSystemMemory:
      CopyOutputRGBAInMemory(
          surface, geometry, color_space, src_rect, rescale_mode,
          is_downscale_or_identity_in_both_dimensions, std::move(request));
      break;
    case CopyOutputRequest::ResultDestination::kNativeTextures: {
      auto representation = CreateSharedImageRepresentationSkia(
          ResourceFormat::RGBA_8888,
          gfx::Size(geometry.result_bounds.width(),
                    geometry.result_bounds.height()),
          color_space);

      if (!representation) {
        DLOG(ERROR) << "Failed to create shared image.";
        return;
      }

      SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;

      auto scoped_write = representation->BeginScopedWriteAccess(
          /*final_msaa_count=*/1, surface_props, &begin_semaphores,
          &end_semaphores,
          gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

      absl::optional<SkVector> scaling;
      if (request->is_scaled()) {
        scaling =
            SkVector::Make(static_cast<SkScalar>(request->scale_to().x()) /
                               request->scale_from().x(),
                           static_cast<SkScalar>(request->scale_to().y()) /
                               request->scale_from().y());
      }

      scoped_write->surface()->wait(begin_semaphores.size(),
                                    begin_semaphores.data());

      RenderSurface(surface, src_rect, scaling,
                    is_downscale_or_identity_in_both_dimensions,
                    scoped_write->surface());

      bool should_submit = !end_semaphores.empty();

      if (!FlushSurface(scoped_write->surface(), end_semaphores,
                        scoped_write->TakeEndState())) {
        // TODO(penghuang): handle vulkan device lost.
        FailedSkiaFlush("CopyOutputRGBA dest_surface->flush()");
        return;
      }

      if (should_submit && !gr_context()->submit()) {
        DLOG(ERROR) << "CopyOutputRGBA gr_context->submit() failed";
        return;
      }

      representation->SetCleared();

      // Grab the mailbox before we transfer `representation`'s ownership:
      gpu::Mailbox mailbox = representation->mailbox();

      CopyOutputResult::ReleaseCallbacks release_callbacks;
      release_callbacks.push_back(
          CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
              std::move(representation)));

      request->SendResult(std::make_unique<CopyOutputTextureResult>(
          CopyOutputResult::Format::RGBA, geometry.result_bounds,
          CopyOutputResult::TextureResult(mailbox, gpu::SyncToken(),
                                          color_space),
          std::move(release_callbacks)));
      break;
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::RenderSurface(
    SkSurface* surface,
    const SkIRect& source_selection,
    absl::optional<SkVector> scaling,
    bool is_downscale_or_identity_in_both_dimensions,
    SkSurface* dest_surface) {
  SkCanvas* dest_canvas = dest_surface->getCanvas();
  int state_depth = dest_canvas->save();

  if (scaling.has_value()) {
    dest_canvas->scale(scaling->x(), scaling->y());
  }

  dest_canvas->clipRect(SkRect::MakeXYWH(0, 0, source_selection.width(),
                                         source_selection.height()));
  // TODO(b/197353769): Ideally, we should simply use a kSrc blending mode,
  // but for some reason, this triggers some antialiasing code that causes
  // various Vulkan tests to fail. We should investigate this and replace
  // this clear with blend mode.
  if (surface->imageInfo().alphaType() != kOpaque_SkAlphaType) {
    dest_canvas->clear(SK_ColorTRANSPARENT);
  }

  auto sampling =
      is_downscale_or_identity_in_both_dimensions
          ? SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear)
          : SkSamplingOptions({1.0f / 3, 1.0f / 3});
  surface->draw(dest_canvas, -source_selection.x(), -source_selection.y(),
                sampling, /*paint=*/nullptr);

  dest_canvas->restoreToCount(state_depth);
}

bool SkiaOutputSurfaceImplOnGpu::FlushSurface(
    SkSurface* surface,
    std::vector<GrBackendSemaphore>& end_semaphores,
    std::unique_ptr<GrBackendSurfaceMutableState> end_state,
    GrGpuFinishedProc finished_proc,
    GrGpuFinishedContext finished_context) {
  GrFlushInfo flush_info;
  flush_info.fNumSemaphores = end_semaphores.size();
  flush_info.fSignalSemaphores = end_semaphores.data();
  flush_info.fFinishedProc = finished_proc;
  flush_info.fFinishedContext = finished_context;
  gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_, &flush_info);
  GrSemaphoresSubmitted flush_result =
      surface->flush(flush_info, end_state.get());
  return flush_result == GrSemaphoresSubmitted::kYes || end_semaphores.empty();
}

SkiaOutputSurfaceImplOnGpu::PlaneAccessData::PlaneAccessData() = default;
SkiaOutputSurfaceImplOnGpu::PlaneAccessData::~PlaneAccessData() = default;

bool SkiaOutputSurfaceImplOnGpu::CreateSurfacesForNV12Planes(
    const SkYUVAInfo& yuva_info,
    const gfx::ColorSpace& color_space,
    std::array<PlaneAccessData, CopyOutputResult::kNV12MaxPlanes>&
        plane_access_datas) {
  std::array<SkISize, SkYUVAInfo::kMaxPlanes> plane_dimensions;
  int plane_number = yuva_info.planeDimensions(plane_dimensions.data());

  DCHECK_EQ(CopyOutputResult::kNV12MaxPlanes, static_cast<size_t>(plane_number))
      << "We expect SkYUVAInfo to describe an NV12 data, which contains 2 "
         "planes!";

  for (int i = 0; i < plane_number; ++i) {
    PlaneAccessData& plane_data = plane_access_datas[i];
    const SkISize& plane_size = plane_dimensions[i];

    const auto resource_format =
        (i == 0) ? ResourceFormat::RED_8 : ResourceFormat::RG_88;
    auto representation = CreateSharedImageRepresentationSkia(
        resource_format, gfx::SkISizeToSize(plane_size), color_space);
    if (!representation) {
      return false;
    }

    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

    std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedWriteAccess>
        scoped_write = representation->BeginScopedWriteAccess(
            /*final_msaa_count=*/1, surface_props, &plane_data.begin_semaphores,
            &plane_data.end_semaphores,
            gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
    SkSurface* dest_surface = scoped_write->surface();
    dest_surface->wait(plane_data.begin_semaphores.size(),
                       plane_data.begin_semaphores.data());

    // Semaphores have already been populated in `plane_data`.
    // Set the remaining fields.
    plane_data.mailbox = representation->mailbox();
    plane_data.representation = std::move(representation);
    plane_data.scoped_write = std::move(scoped_write);
    plane_data.size = plane_size;
  }

  return true;
}

bool SkiaOutputSurfaceImplOnGpu::ImportSurfacesForNV12Planes(
    const BlitRequest& blit_request,
    std::array<PlaneAccessData, CopyOutputResult::kNV12MaxPlanes>&
        plane_access_datas) {
  for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
    const gpu::MailboxHolder& mailbox_holder = blit_request.mailbox(i);

    // Should never happen, mailboxes are validated when setting blit request on
    // a CopyOutputResult and we only access `kNV12MaxPlanes` mailboxes.
    DCHECK(!mailbox_holder.mailbox.IsZero());

    PlaneAccessData& plane_data = plane_access_datas[i];

    auto representation = dependency_->GetSharedImageManager()->ProduceSkia(
        mailbox_holder.mailbox, context_state_->memory_type_tracker(),
        context_state_);
    if (!representation) {
      return false;
    }

    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

    std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedWriteAccess>
        scoped_write = representation->BeginScopedWriteAccess(
            /*final_msaa_count=*/1, surface_props, &plane_data.begin_semaphores,
            &plane_data.end_semaphores,
            gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
    SkSurface* dest_surface = scoped_write->surface();
    dest_surface->wait(plane_data.begin_semaphores.size(),
                       plane_data.begin_semaphores.data());

    // Semaphores have already been populated in `plane_data`.
    // Set the remaining fields.
    plane_data.size = gfx::SizeToSkISize(representation->size());
    plane_data.mailbox = representation->mailbox();
    plane_data.representation = std::move(representation);
    plane_data.scoped_write = std::move(scoped_write);
  }

  return true;
}

void SkiaOutputSurfaceImplOnGpu::BlendBitmapOverlays(
    SkCanvas* canvas,
    const BlitRequest& blit_request) {
  for (const BlendBitmap& blend_bitmap : blit_request.blend_bitmaps()) {
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrcOver);

    canvas->drawImageRect(blend_bitmap.image(),
                          gfx::RectToSkRect(blend_bitmap.source_region()),
                          gfx::RectToSkRect(blend_bitmap.destination_region()),
                          SkSamplingOptions(SkFilterMode::kLinear), &paint,
                          SkCanvas::kFast_SrcRectConstraint);
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutputNV12(
    SkSurface* surface,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    const SkIRect& src_rect,
    SkSurface::RescaleMode rescale_mode,
    bool is_downscale_or_identity_in_both_dimensions,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK(!request->has_blit_request() ||
         request->result_destination() ==
             CopyOutputRequest::ResultDestination::kNativeTextures)
      << "Only CopyOutputRequests that hand out native textures support blit "
         "requests!";
  DCHECK(!request->has_blit_request() || request->has_result_selection())
      << "Only CopyOutputRequests that specify result selection support blit "
         "requests!";

  // Overview:
  // 1. Try to create surfaces for NV12 planes (we know the needed size in
  // advance). If this fails, send an empty result. For requests that have a
  // blit request appended, the surfaces should be backed by caller-provided
  // textures.
  // 2. Render the desired region into a new SkSurface, taking into account
  // desired scaling and clipping.
  // 3. If blitting, honor the blend bitmap requests set by blending them onto
  // the surface produced in step 2.
  // 4. Grab an SkImage and convert it into multiple SkSurfaces created by
  // step 1, one for each plane.
  // 5. Depending on the result destination of the request, either:
  // - pass ownership of the textures to the caller (native textures result)
  // - schedule a read-back & expose its results to the caller (system memory
  // result)
  //
  // Note: in case the blit request populates the GMBs, the flow stays the same,
  // but we need to ensure that the results are only sent out after the
  // GpuMemoryBuffer is safe to map into system memory.

  // The size of the destination is passed in via `geometry.result_selection` -
  // it already takes into account the rect of the render pass that is being
  // copied, as well as area, scaling & result_selection of the `request`.
  // This represents the size of the intermediate texture that will be then
  // blitted to the destination textures.
  const gfx::Size intermediate_dst_size = geometry.result_selection.size();

  std::array<PlaneAccessData, CopyOutputResult::kNV12MaxPlanes>
      plane_access_datas;

  SkYUVAInfo yuva_info;

  bool destination_surfaces_ready = false;
  if (request->has_blit_request()) {
    if (request->result_selection().size() != intermediate_dst_size) {
      DLOG(WARNING)
          << __func__
          << ": result selection is different than render pass output, "
             "geometry="
          << geometry.ToString() << ", request=" << request->ToString();
      // Send empty result, we have a blit request that asks for a different
      // size than what we have available - the behavior in this case is
      // currently unspecified as we'd have to leave parts of the caller's
      // region unpopulated.
      return;
    }

    destination_surfaces_ready = ImportSurfacesForNV12Planes(
        request->blit_request(), plane_access_datas);

    // The entire destination image size is the same as the size of the luma
    // plane of the image that was just imported:
    yuva_info = SkYUVAInfo(
        plane_access_datas[0].size, SkYUVAInfo::PlaneConfig::kY_UV,
        SkYUVAInfo::Subsampling::k420, kRec709_Limited_SkYUVColorSpace);

    // Check if the destination will fit in the blit target:
    const gfx::Rect blit_destination_rect(
        request->blit_request().destination_region_offset(),
        intermediate_dst_size);
    const gfx::Rect blit_target_image_rect(
        gfx::SkISizeToSize(plane_access_datas[0].size));

    if (!blit_target_image_rect.Contains(blit_destination_rect)) {
      // Send empty result, the blit target image is not large enough to fit the
      // results.
      return;
    }
  } else {
    yuva_info = SkYUVAInfo(gfx::SizeToSkISize(intermediate_dst_size),
                           SkYUVAInfo::PlaneConfig::kY_UV,
                           SkYUVAInfo::Subsampling::k420,
                           kRec709_Limited_SkYUVColorSpace);

    destination_surfaces_ready =
        CreateSurfacesForNV12Planes(yuva_info, color_space, plane_access_datas);
  }

  if (!destination_surfaces_ready) {
    DVLOG(1) << "failed to create / import destination surfaces";
    // Send empty result.
    return;
  }

  // Create a destination for the scaled & clipped result:
  auto intermediate_representation = CreateSharedImageRepresentationSkia(
      ResourceFormat::RGBA_8888, intermediate_dst_size, color_space);
  if (!intermediate_representation) {
    DVLOG(1) << "failed to create shared image representation for the "
                "intermediate surface";
    // Send empty result.
    return;
  }

  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  auto intermediate_scoped_write =
      intermediate_representation->BeginScopedWriteAccess(
          /*final_msaa_count=*/1, surface_props, &begin_semaphores,
          &end_semaphores,
          gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

  absl::optional<SkVector> scaling;
  if (request->is_scaled()) {
    scaling = SkVector::Make(static_cast<SkScalar>(request->scale_to().x()) /
                                 request->scale_from().x(),
                             static_cast<SkScalar>(request->scale_to().y()) /
                                 request->scale_from().y());
  }

  intermediate_scoped_write->surface()->wait(begin_semaphores.size(),
                                             begin_semaphores.data());

  RenderSurface(surface, src_rect, scaling,
                is_downscale_or_identity_in_both_dimensions,
                intermediate_scoped_write->surface());

  if (request->has_blit_request()) {
    BlendBitmapOverlays(intermediate_scoped_write->surface()->getCanvas(),
                        request->blit_request());
  }

  auto intermediate_image =
      intermediate_scoped_write->surface()->makeImageSnapshot();
  if (!intermediate_image) {
    DLOG(ERROR) << "failed to retrieve `intermediate_image`.";
    return;
  }

  // `skia::BlitRGBAToYUVA()` requires a buffer with 4 SkSurface* elements,
  // let's allocate it and populate its first 2 entries with the surfaces
  // obtained from |plane_access_datas|.
  std::array<SkSurface*, SkYUVAInfo::kMaxPlanes> plane_surfaces = {
      plane_access_datas[0].scoped_write->surface(),
      plane_access_datas[1].scoped_write->surface(), nullptr, nullptr};

  // The region to be populated in caller's textures is derived from blit
  // request's |destination_region_offset()|, and from COR's
  // |result_selection()|. If we have a blit request, use it. Otherwise, use an
  // empty rect (which means that entire image will be used as the target of the
  // blit - this will not result in rescaling since w/o blit request present,
  // the destination image size matches the |geometry.result_selection|).
  const SkRect dst_region =
      request->has_blit_request()
          ? gfx::RectToSkRect(
                gfx::Rect(request->blit_request().destination_region_offset(),
                          intermediate_dst_size))
          : SkRect::MakeEmpty();

  // We should clear destination if BlitRequest asked to letterbox everything
  // outside of intended destination region:
  const bool clear_destination =
      request->has_blit_request()
          ? request->blit_request().letterboxing_behavior() ==
                LetterboxingBehavior::kLetterbox
          : false;
  skia::BlitRGBAToYUVA(intermediate_image.get(), plane_surfaces.data(),
                       yuva_info, dst_region, clear_destination);

  // Collect mailbox holders for the destination textures. They will be needed
  // in case the result is kNativeTextures. It happens here in order to simplify
  // the code in case we are populating the GpuMemoryBuffer-backed textures.
  std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes>
      plane_mailbox_holders = {
          gpu::MailboxHolder(plane_access_datas[0].mailbox, gpu::SyncToken(),
                             GL_TEXTURE_2D),
          gpu::MailboxHolder(plane_access_datas[1].mailbox, gpu::SyncToken(),
                             GL_TEXTURE_2D),
          gpu::MailboxHolder(),
      };

  // If we are not the ones allocating the textures, they may come from a GMB,
  // in which case we need to delay sending the results until we receive a
  // callback that the GPU work has completed - otherwise, memory-mapping the
  // GMB may not yield the latest version of the contents.
  const bool should_wait_for_gpu_work =
      request->result_destination() ==
          CopyOutputRequest::ResultDestination::kNativeTextures &&
      request->has_blit_request() &&
      request->blit_request().populates_gpu_memory_buffer();

  scoped_refptr<NV12PlanesReadyContext> nv12_planes_ready = nullptr;
  if (should_wait_for_gpu_work) {
    // Prepare a per-CopyOutputRequest context that will be responsible for
    // sending the CopyOutputResult:
    nv12_planes_ready = base::MakeRefCounted<NV12PlanesReadyContext>(
        weak_ptr_, std::move(request), geometry.result_selection,
        plane_mailbox_holders, color_space);
  }

  bool should_submit = false;
  for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
    plane_access_datas[i].representation->SetCleared();

    should_submit |= !plane_access_datas[i].end_semaphores.empty();

    // Prepare a per-plane context that will notify the per-request context that
    // GPU work that produces the contents of a plane that the GPU-side of the
    // work has completed.
    std::unique_ptr<NV12SinglePlaneReadyContext> nv12_plane_ready =
        should_wait_for_gpu_work
            ? std::make_unique<NV12SinglePlaneReadyContext>(nv12_planes_ready)
            : nullptr;

    if (should_wait_for_gpu_work) {
      // Treat the fact that we're waiting for GPU work to finish the same way
      // as a readback request. This would allow us to nudge Skia to fire the
      // callbacks. See `SkiaOutputSurfaceImplOnGpu::CheckReadbackCompletion()`.
      ++num_readbacks_pending_;
    }

    if (!FlushSurface(
            plane_surfaces[i], plane_access_datas[i].end_semaphores,
            plane_access_datas[i].scoped_write->TakeEndState(),
            should_wait_for_gpu_work
                ? &NV12SinglePlaneReadyContext::OnNV12PlaneReady
                : nullptr,
            should_wait_for_gpu_work ? nv12_plane_ready.release() : nullptr)) {
      // TODO(penghuang): handle vulkan device lost.
      FailedSkiaFlush("CopyOutputNV12 plane_surfaces[i]->flush()");
      return;
    }
  }

  should_submit |= !end_semaphores.empty();

  intermediate_representation->SetCleared();
  if (!FlushSurface(intermediate_scoped_write->surface(), end_semaphores,
                    intermediate_scoped_write->TakeEndState())) {
    // TODO(penghuang): handle vulkan device lost.
    FailedSkiaFlush("CopyOutputNV12 dest_surface->flush()");
    return;
  }

  if (should_submit && !gr_context()->submit()) {
    DLOG(ERROR) << "CopyOutputNV12 gr_context->submit() failed";
    return;
  }

  if (should_wait_for_gpu_work) {
    // Flow will continue after GPU work is done - see
    // `NV12PlanesReadyContext::OnNV12PlaneReady()` that eventually gets called.
    return;
  }

  // We conditionally move from request (if `should_wait_for_gpu_work` is true),
  // DCHECK that we don't accidentally enter this codepath after the request was
  // moved from.
  DCHECK(request);

  switch (request->result_destination()) {
    case CopyOutputRequest::ResultDestination::kNativeTextures: {
      CopyOutputResult::ReleaseCallbacks release_callbacks;

      if (!request->has_blit_request()) {
        // In blit requests, we are not responsible for releasing the textures
        // (the issuer of the request owns them), create the callbacks only if
        // we don't have blit request:
        for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
          release_callbacks.push_back(
              CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
                  std::move(plane_access_datas[i].representation)));
        }
      }

      request->SendResult(std::make_unique<CopyOutputTextureResult>(
          CopyOutputResult::Format::NV12_PLANES, geometry.result_selection,
          CopyOutputResult::TextureResult(plane_mailbox_holders, color_space),
          std::move(release_callbacks)));

      break;
    }
    case CopyOutputRequest::ResultDestination::kSystemMemory: {
      auto nv12_readback = base::MakeRefCounted<NV12PlanesReadbackContext>(
          weak_ptr_, std::move(request), geometry.result_selection);

      // Issue readbacks from the surfaces:
      for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
        SkImageInfo dst_info = SkImageInfo::Make(
            plane_access_datas[i].size,
            (i == 0) ? kAlpha_8_SkColorType : kR8G8_unorm_SkColorType,
            kUnpremul_SkAlphaType);

        auto context =
            std::make_unique<NV12PlanePixelReadContext>(nv12_readback, i);

        num_readbacks_pending_++;
        plane_surfaces[i]->asyncRescaleAndReadPixels(
            dst_info, SkIRect::MakeSize(plane_access_datas[i].size),
            SkSurface::RescaleGamma::kSrc,
            SkSurface::RescaleMode::kRepeatedLinear,
            &CopyOutputResultSkiaNV12::OnNV12PlaneReadbackDone,
            context.release());
      }

      break;
    }
  }
}

ReleaseCallback
SkiaOutputSurfaceImplOnGpu::CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
    std::unique_ptr<gpu::SharedImageRepresentationSkia> representation) {
  auto gpu_callback = std::make_unique<ReleaseCallback>(base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::DestroyCopyOutputResourcesOnGpuThread,
      weak_ptr_factory_.GetWeakPtr(), std::move(representation),
      context_state_));
  release_on_gpu_callbacks_.push_back(std::move(gpu_callback));

  auto run_gpu_callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::RunDestroyCopyOutputResourcesOnGpuThread,
      weak_ptr_factory_.GetWeakPtr(), release_on_gpu_callbacks_.back().get());

  return base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                            std::move(run_gpu_callback));
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    AggregatedRenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    const gpu::Mailbox& mailbox) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::CopyOutput");
  // TODO(https://crbug.com/898595): Do this on the GPU instead of CPU with
  // Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (context_is_lost_)
    return;

  bool from_framebuffer = !id;
  DCHECK(scoped_output_device_paint_ || !from_framebuffer);

  SkSurface* surface;
  std::unique_ptr<gpu::SharedImageRepresentationSkia> backing_representation;
  std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_access;
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<GrBackendSurfaceMutableState> end_state;
  if (from_framebuffer) {
    surface = scoped_output_device_paint_->sk_surface();
  } else {
    backing_representation = shared_image_representation_factory_->ProduceSkia(
        mailbox, context_state_.get());
    DCHECK(backing_representation);

    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
    // TODO(https://crbug.com/1226672): Use BeginScopedReadAccess instead
    scoped_access = backing_representation->BeginScopedWriteAccess(
        /*final_msaa_count=*/1, surface_props, &begin_semaphores,
        &end_semaphores,
        gpu::SharedImageRepresentation::AllowUnclearedAccess::kNo);
    surface = scoped_access->surface();
    end_state = scoped_access->TakeEndState();
    if (!begin_semaphores.empty()) {
      auto result =
          surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                        /*deleteSemaphoresAfterWait=*/false);
      DCHECK(result);
    }
  }

  // Do not support reading back from vulkan secondary command buffer.
  if (!surface)
    return;

  // If a platform doesn't support RGBX_8888 format, we will use RGBA_8888
  // instead. In this case, we need discard alpha channel (modify the alpha
  // value to 0xff, but keep other channel not changed).
  bool need_discard_alpha =
      from_framebuffer && (output_device_->is_emulated_rgbx());
  if (need_discard_alpha) {
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }
    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    paint.setBlendMode(SkBlendMode::kDstATop);
    surface->getCanvas()->drawPaint(paint);
    surface->flush();
  }

  absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
  if (dependency_->GetGrShaderCache()) {
    cache_use.emplace(dependency_->GetGrShaderCache(),
                      gpu::kDisplayCompositorClientId);
  }

  // For downscaling, use the GOOD quality setting (appropriate for
  // thumbnailing); and, for upscaling, use the BEST quality.
  const bool is_downscale_or_identity_in_both_dimensions =
      request->scale_to().x() <= request->scale_from().x() &&
      request->scale_to().y() <= request->scale_from().y();
  const SkSurface::RescaleMode rescale_mode =
      is_downscale_or_identity_in_both_dimensions
          ? SkSurface::RescaleMode::kRepeatedLinear
          : SkSurface::RescaleMode::kRepeatedCubic;

  // Compute |source_selection| as a workaround to support |result_selection|
  // with Skia readback. |result_selection| is a clip rect specified in the
  // destination pixel space. By transforming |result_selection| back to the
  // source pixel space we can compute what rectangle to sample from.
  //
  // This might introduce some rounding error if destination pixel space is
  // scaled up from the source pixel space. When scaling |result_selection| back
  // down it might not be pixel aligned.
  gfx::Rect source_selection = geometry.sampling_bounds;
  if (request->has_result_selection()) {
    gfx::Rect sampling_selection = request->result_selection();
    if (request->is_scaled()) {
      // Invert the scaling.
      sampling_selection = copy_output::ComputeResultRect(
          sampling_selection, request->scale_to(), request->scale_from());
    }
    sampling_selection.Offset(source_selection.OffsetFromOrigin());
    source_selection.Intersect(sampling_selection);
  }

  SkIRect src_rect =
      SkIRect::MakeXYWH(source_selection.x(), source_selection.y(),
                        source_selection.width(), source_selection.height());
  switch (request->result_format()) {
    case CopyOutputRequest::ResultFormat::I420_PLANES: {
      DCHECK_EQ(geometry.result_selection.width() % 2, 0)
          << "SkSurface::asyncRescaleAndReadPixelsYUV420() requires "
             "destination width to be even!";
      DCHECK_EQ(geometry.result_selection.height() % 2, 0)
          << "SkSurface::asyncRescaleAndReadPixelsYUV420() requires "
             "destination height to be even!";

      std::unique_ptr<ReadPixelsContext> context =
          std::make_unique<ReadPixelsContext>(std::move(request),
                                              geometry.result_selection,
                                              color_space, weak_ptr_);
      // Skia readback could be synchronous. Incremement counter in case
      // ReadbackCompleted is called immediately.
      num_readbacks_pending_++;
      surface->asyncRescaleAndReadPixelsYUV420(
          kRec709_SkYUVColorSpace, SkColorSpace::MakeSRGB(), src_rect,
          {geometry.result_selection.width(),
           geometry.result_selection.height()},
          SkSurface::RescaleGamma::kSrc, rescale_mode,
          &CopyOutputResultSkiaYUV::OnReadbackDone, context.release());
      break;
    }
    case CopyOutputRequest::ResultFormat::NV12_PLANES: {
      CopyOutputNV12(surface, geometry, color_space, src_rect, rescale_mode,
                     is_downscale_or_identity_in_both_dimensions,
                     std::move(request));
      break;
    }
    case CopyOutputRequest::ResultFormat::RGBA: {
      CopyOutputRGBA(surface, geometry, color_space, src_rect, rescale_mode,
                     is_downscale_or_identity_in_both_dimensions,
                     std::move(request));
      break;
    }
  }

  if (!FlushSurface(surface, end_semaphores, std::move(end_state))) {
    // TODO(penghuang): handle vulkan device lost.
    FailedSkiaFlush("surface->flush() failed.");
    return;
  }

  ScheduleCheckReadbackCompletion();
}

void SkiaOutputSurfaceImplOnGpu::RunDestroyCopyOutputResourcesOnGpuThread(
    ReleaseCallback* callback,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  for (size_t i = 0; i < release_on_gpu_callbacks_.size(); ++i) {
    if (release_on_gpu_callbacks_[i].get() == callback) {
      std::move(*release_on_gpu_callbacks_[i]).Run(sync_token, is_lost);
      release_on_gpu_callbacks_.erase(release_on_gpu_callbacks_.begin() + i);
      return;
    }
  }
  NOTREACHED() << "The Callback returned by GetDeleteCallback() was called "
               << "more than once.";
}

void SkiaOutputSurfaceImplOnGpu::DestroyCopyOutputResourcesOnGpuThread(
    std::unique_ptr<gpu::SharedImageRepresentationSkia> representation,
    scoped_refptr<gpu::SharedContextState> context_state,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  context_state_->MakeCurrent(nullptr);
  representation.reset();
}

void SkiaOutputSurfaceImplOnGpu::BeginAccessImages(
    const std::vector<ImageContextImpl*>& image_contexts,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::BeginAccessImages");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool is_gl = gpu_preferences_.gr_context_type == gpu::GrContextType::kGL;

  for (auto* context : image_contexts) {
    // Prepare for accessing render pass.
    context->BeginAccessIfNecessary(
        context_state_.get(), shared_image_representation_factory_.get(),
        dependency_->GetMailboxManager(), begin_semaphores, end_semaphores);
    if (auto end_state = context->TakeAccessEndState())
      image_contexts_with_end_access_state_.emplace(context,
                                                    std::move(end_state));

    // Texture parameters can be modified by concurrent reads so reset them
    // before compositing from the texture. See https://crbug.com/1092080.
    if (is_gl && context->maybe_concurrent_reads()) {
      auto* promise_texture = context->promise_image_texture();
      if (promise_texture) {
        GrBackendTexture backend_texture = promise_texture->backendTexture();
        backend_texture.glTextureParametersModified();
      }
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::ResetStateOfImages() {
  for (auto& context : image_contexts_with_end_access_state_) {
    if (!gr_context()->setBackendTextureState(
            context.first->promise_image_texture()->backendTexture(),
            *context.second)) {
      DLOG(ERROR) << "setBackendTextureState() failed.";
    }
  }
  image_contexts_with_end_access_state_.clear();
}

void SkiaOutputSurfaceImplOnGpu::EndAccessImages(
    const base::flat_set<ImageContextImpl*>& image_contexts) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::EndAccessImages");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(image_contexts_with_end_access_state_.empty());
  for (auto* context : image_contexts)
    context->EndAccessIfNecessary();
}

sk_sp<GrContextThreadSafeProxy>
SkiaOutputSurfaceImplOnGpu::GetGrContextThreadSafeProxy() {
  return gr_context() ? gr_context()->threadSafeProxy() : nullptr;
}

void SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts(
    std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
        image_contexts) {
  DCHECK(!image_contexts.empty());
  // The window could be destroyed already, and the MakeCurrent will fail with
  // an destroyed window, so MakeCurrent without requiring the fbo0.
  if (context_is_lost_) {
    for (const auto& context : image_contexts)
      context->OnContextLost();
  }

  image_contexts.clear();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  TRACE_EVENT1("viz", "SkiaOutputSurfaceImplOnGpu::ScheduleOverlays",
               "num_overlays", overlays.size());

  constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
  constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(16);
  constexpr int kHistogramTimeBuckets = 50;
  base::TimeTicks start_time = base::TimeTicks::Now();

  output_device_->ScheduleOverlays(std::move(overlays));

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Gpu.OutputSurface.ScheduleOverlaysUs",
      base::TimeTicks::Now() - start_time, kHistogramMinTime, kHistogramMaxTime,
      kHistogramTimeBuckets);
}

void SkiaOutputSurfaceImplOnGpu::SetEnableDCLayers(bool enable) {
  if (context_is_lost_)
    return;
  output_device_->SetEnableDCLayers(enable);
}

void SkiaOutputSurfaceImplOnGpu::SetGpuVSyncEnabled(bool enabled) {
  output_device_->SetGpuVSyncEnabled(enabled);
}

void SkiaOutputSurfaceImplOnGpu::SetFrameRate(float frame_rate) {
  if (gl_surface_)
    gl_surface_->SetFrameRate(frame_rate);
}

void SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting(
    const OutputSurface::Capabilities& capabilities) {
  // Check that we're using an offscreen surface.
  DCHECK(dependency_->IsOffscreen());
  output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
      context_state_, capabilities.output_surface_origin,
      renderer_settings_.requires_alpha_channel,
      shared_gpu_deps_->memory_tracker(), GetDidSwapBuffersCompleteCallback());
}

bool SkiaOutputSurfaceImplOnGpu::Initialize() {
  TRACE_EVENT1("viz", "SkiaOutputSurfaceImplOnGpu::Initialize",
               "is_using_vulkan", is_using_vulkan());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(USE_OZONE)
    gpu::SurfaceHandle surface_handle = dependency_->GetSurfaceHandle();
    if (surface_handle != gpu::kNullSurfaceHandle) {
      window_surface_ = ui::OzonePlatform::GetInstance()
                            ->GetSurfaceFactoryOzone()
                            ->CreatePlatformWindowSurface(surface_handle);
    }
#endif

  context_state_ = dependency_->GetSharedContextState();
  DCHECK(context_state_);
  if (!context_state_->gr_context()) {
    DLOG(ERROR) << "Failed to create GrContext";
    return false;
  }

  if (is_using_vulkan()) {
    if (!InitializeForVulkan())
      return false;
  } else if (is_using_dawn()) {
    if (!InitializeForDawn())
      return false;
  } else {
    if (!InitializeForGL())
      return false;
  }

  max_resource_cache_bytes_ =
      context_state_->gr_context()->getResourceCacheLimit();
  if (context_state_)
    context_state_->AddContextLostObserver(this);

  return true;
}

bool SkiaOutputSurfaceImplOnGpu::InitializeForGL() {
  gl::GLSurfaceFormat format;
  if (PreferRGB565ResourcesForDisplay() &&
      !renderer_settings_.requires_alpha_channel) {
    format.SetRGB565();
  }

  if (dependency_->IsOffscreen()) {
    gl_surface_ = dependency_->CreateGLSurface(nullptr, format);
    if (!gl_surface_)
      return false;

    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, gfx::SurfaceOrigin::kTopLeft,
        renderer_settings_.requires_alpha_channel,
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
  } else {
    gl_surface_ =
        dependency_->CreateGLSurface(weak_ptr_factory_.GetWeakPtr(), format);

    if (!gl_surface_)
      return false;

    if (MakeCurrent(/*need_framebuffer=*/true)) {
      if (gl_surface_->IsSurfaceless()) {
#if defined(USE_OZONE)
        [[maybe_unused]] bool needs_background_image =
            ui::OzonePlatform::GetInstance()
                ->GetPlatformRuntimeProperties()
                .needs_background_image;
        [[maybe_unused]] bool supports_non_backed_solid_color_images =
            ui::OzonePlatform::GetInstance()
                ->GetPlatformRuntimeProperties()
                .supports_non_backed_solid_color_buffers;
#else   // defined(USE_OZONE)
        [[maybe_unused]] bool needs_background_image = false;
        [[maybe_unused]] bool supports_non_backed_solid_color_images = false;
#endif  // !defined(USE_OZONE)

#if !BUILDFLAG(IS_WIN)
        output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
            std::make_unique<OutputPresenterGL>(
                gl_surface_, dependency_, shared_image_factory_.get(),
                shared_image_representation_factory_.get()),
            dependency_, shared_image_representation_factory_.get(),
            shared_gpu_deps_->memory_tracker(),
            GetDidSwapBuffersCompleteCallback(), needs_background_image,
            supports_non_backed_solid_color_images);
#else   // !BUILDFLAG(IS_WIN)
        NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_WIN)
      } else {
        if (dependency_->NeedsSupportForExternalStencil()) {
          output_device_ = std::make_unique<SkiaOutputDeviceWebView>(
              context_state_.get(), gl_surface_,
              shared_gpu_deps_->memory_tracker(),
              GetDidSwapBuffersCompleteCallback());
        } else {
          output_device_ = std::make_unique<SkiaOutputDeviceGL>(
              dependency_->GetMailboxManager(),
              shared_image_representation_factory_.get(), context_state_.get(),
              gl_surface_, feature_info_, shared_gpu_deps_->memory_tracker(),
              GetDidSwapBuffersCompleteCallback());
        }
      }
    } else {
      gl_surface_ = nullptr;
      context_state_ = nullptr;
      LOG(ERROR) << "Failed to make current during initialization.";
      return false;
    }
  }
  DCHECK_EQ(gl_surface_->IsOffscreen(), dependency_->IsOffscreen());
  return true;
}

#if BUILDFLAG(ENABLE_VULKAN)
bool SkiaOutputSurfaceImplOnGpu::InitializeForVulkan() {
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, gfx::SurfaceOrigin::kBottomLeft,
        renderer_settings_.requires_alpha_channel,
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  if (vulkan_context_provider_->GetGrSecondaryCBDrawContext()) {
    output_device_ = std::make_unique<SkiaOutputDeviceVulkanSecondaryCB>(
        vulkan_context_provider_, shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
    return true;
  }
#endif

#if defined(USE_OZONE)
  [[maybe_unused]] bool needs_background_image =
      ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .needs_background_image;
  [[maybe_unused]] bool supports_non_backed_solid_color_images =
      ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_non_backed_solid_color_buffers;
#else   // defined(USE_OZONE)
  [[maybe_unused]] bool needs_background_image = false;
  [[maybe_unused]] bool supports_non_backed_solid_color_images = false;
#endif  // !defined(USE_OZONE)

#if !BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_FUCHSIA)
  auto output_presenter = OutputPresenterFuchsia::Create(
      window_surface_.get(), dependency_, shared_image_factory_.get(),
      shared_image_representation_factory_.get());
#else
  auto output_presenter =
      OutputPresenterGL::Create(dependency_, shared_image_factory_.get(),
                                shared_image_representation_factory_.get());
  if (output_presenter) {
    // TODO(https://crbug.com/1012401): don't depend on GL.
    gl_surface_ = output_presenter->gl_surface();
  }
#endif
  if (output_presenter) {
    output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
        std::move(output_presenter), dependency_,
        shared_image_representation_factory_.get(),
        shared_gpu_deps_->memory_tracker(), GetDidSwapBuffersCompleteCallback(),
        needs_background_image, supports_non_backed_solid_color_images);
    return true;
  }
#endif  // !BUILDFLAG(IS_WIN)

  std::unique_ptr<SkiaOutputDeviceVulkan> output_device;
  if (!gpu_preferences_.disable_vulkan_surface) {
    output_device = SkiaOutputDeviceVulkan::Create(
        vulkan_context_provider_, dependency_->GetSurfaceHandle(),
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
  }
  if (MayFallBackToSkiaOutputDeviceX11()) {
#if defined(USE_OZONE_PLATFORM_X11)
    if (output_device) {
      output_device_ = std::move(output_device);
    } else {
      output_device_ = SkiaOutputDeviceX11::Create(
          context_state_, dependency_->GetSurfaceHandle(),
          shared_gpu_deps_->memory_tracker(),
          GetDidSwapBuffersCompleteCallback());
    }
    if (output_device_)
      return true;
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
  }
  if (!output_device)
    return false;

#if BUILDFLAG(IS_WIN)
  gpu::SurfaceHandle child_surface = output_device->GetChildSurfaceHandle();
  if (child_surface != gpu::kNullSurfaceHandle) {
    DidCreateAcceleratedSurfaceChildWindow(dependency_->GetSurfaceHandle(),
                                           child_surface);
  }
#endif  // BUILDFLAG(IS_WIN)
  output_device_ = std::move(output_device);
  return true;
}
#else   // BUILDFLAG(ENABLE_VULKAN)
bool SkiaOutputSurfaceImplOnGpu::InitializeForVulkan() {
  return false;
}
#endif  // !BUILDFLAG(ENABLE_VULKAN)

bool SkiaOutputSurfaceImplOnGpu::InitializeForDawn() {
#if BUILDFLAG(SKIA_USE_DAWN)
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, gfx::SurfaceOrigin::kBottomLeft,
        renderer_settings_.requires_alpha_channel,
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
  } else {
#if defined(USE_OZONE_PLATFORM_X11)
    // TODO(rivr): Set up a Vulkan swapchain so that Linux can also use
    // SkiaOutputDeviceDawn.
    if (MayFallBackToSkiaOutputDeviceX11()) {
      output_device_ = SkiaOutputDeviceX11::Create(
          context_state_, dependency_->GetSurfaceHandle(),
          shared_gpu_deps_->memory_tracker(),
          GetDidSwapBuffersCompleteCallback());
    }
#elif BUILDFLAG(IS_WIN)
    std::unique_ptr<SkiaOutputDeviceDawn> output_device =
        std::make_unique<SkiaOutputDeviceDawn>(
            dawn_context_provider_, dependency_->GetSurfaceHandle(),
            gfx::SurfaceOrigin::kTopLeft, shared_gpu_deps_->memory_tracker(),
            GetDidSwapBuffersCompleteCallback());
    const gpu::SurfaceHandle child_surface_handle =
        output_device->GetChildSurfaceHandle();
    DidCreateAcceleratedSurfaceChildWindow(dependency_->GetSurfaceHandle(),
                                           child_surface_handle);
    output_device_ = std::move(output_device);
#else
    NOTREACHED();
    return false;
#endif
  }
#endif
  return !!output_device_;
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent(bool need_framebuffer) {
  // If GL is not being used or GLSurface is not surfaceless, we can ignore
  // making current the GLSurface for better performance.
  bool need_fbo0 = need_framebuffer && context_state_->GrContextIsGL() &&
                   gl_surface_ && !gl_surface_->IsSurfaceless();

  // need_fbo0 implies need_gl too.
  bool need_gl = need_fbo0;

  // Only make current with |gl_surface_|, if following operations will use
  // fbo0.
  auto* gl_surface = need_fbo0 ? gl_surface_.get() : nullptr;
  if (!context_state_->MakeCurrent(gl_surface, need_gl)) {
    LOG(ERROR) << "Failed to make current.";
    dependency_->DidLoseContext(
        *context_state_->context_lost_reason(),
        GURL("chrome://gpu/SkiaOutputSurfaceImplOnGpu::MakeCurrent"));
    MarkContextLost(GetContextLostReason(
        gpu::error::kLostContext, *context_state_->context_lost_reason()));
    return false;
  }

  // Some GLSurface implements OnMakeCurrent() to tracing current GLContext,
  // even if framebuffer is not needed, we still call OnMakeCurrent() so
  // GLSurface implementation will know the current GLContext.
  if (gl_surface_ && !need_fbo0)
    gl_surface_->OnMakeCurrent(context_state_->context());

  context_state_->set_need_context_state_reset(true);
  return true;
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSync(uint64_t sync_fence_release) {
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffersInternal(
    absl::optional<OutputSurfaceFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(output_device_);

  if (context_is_lost_)
    return;

  if (gl_surface_ && frame) {
    gl_surface_->SetChoreographerVsyncIdForNextFrame(
        frame->choreographer_vsync_id);
    if (frame->delegated_ink_metadata) {
      gl_surface_->SetDelegatedInkTrailStartPoint(
          std::move(frame->delegated_ink_metadata));
    }
  }

  bool sync_cpu =
      gpu::ShouldVulkanSyncCpuForSkiaSubmit(vulkan_context_provider_);

  ResetStateOfImages();
  output_device_->Submit(
      sync_cpu, base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PostSubmit,
                               base::Unretained(this), std::move(frame)));
}

void SkiaOutputSurfaceImplOnGpu::PostSubmit(
    absl::optional<OutputSurfaceFrame> frame) {
  promise_image_access_helper_.EndAccess();
  scoped_output_device_paint_.reset();

#if BUILDFLAG(ENABLE_VULKAN)
  while (!pending_release_fence_cbs_.empty()) {
    auto& item = pending_release_fence_cbs_.front();
    auto release_fence = CreateReleaseFenceForVulkan(item.first);
    if (release_fence.is_null())
      LOG(ERROR) << "Unable to create a release fence for Vulkan.";
    PostTaskToClientThread(
        base::BindOnce(std::move(item.second), std::move(release_fence)));
    pending_release_fence_cbs_.pop_front();
  }
#else
  DCHECK(pending_release_fence_cbs_.empty());
#endif

  if (frame) {
    if (waiting_for_full_damage_) {
      // If we're using partial swap, we need to check whether the sub-buffer
      // rect is actually the entire screen, but otherwise, the damage is
      // always the full surface.
      if (frame->sub_buffer_rect &&
          capabilities().supports_post_sub_buffer &&
          frame->sub_buffer_rect->size() != size_) {
        output_device_->SwapBuffersSkipped(buffer_presented_callback_,
                                           std::move(*frame));
        output_surface_plane_.reset();
        destroy_after_swap_.clear();
        return;
      }
      waiting_for_full_damage_ = false;
    }

    if (output_surface_plane_)
      DCHECK(output_device_->IsPrimaryPlaneOverlay());

    if (frame->sub_buffer_rect) {
      if (capabilities().supports_post_sub_buffer) {
        if (capabilities().output_surface_origin ==
            gfx::SurfaceOrigin::kBottomLeft) {
          frame->sub_buffer_rect->set_y(size_.height() -
                                        frame->sub_buffer_rect->y() -
                                        frame->sub_buffer_rect->height());
        }
      }

      if (output_surface_plane_)
        output_surface_plane_->damage_rect = frame->sub_buffer_rect;
    }

    output_device_->SetViewportSize(frame->size);
    output_device_->SchedulePrimaryPlane(output_surface_plane_);

    if (frame->sub_buffer_rect) {
      if (capabilities().supports_post_sub_buffer) {
        output_device_->PostSubBuffer(*frame->sub_buffer_rect,
                                      buffer_presented_callback_,
                                      std::move(*frame));

      } else if (capabilities().supports_commit_overlay_planes) {
        // CommitOverlayPlanes() can only be used for empty swap.
        DCHECK(frame->sub_buffer_rect->IsEmpty());
        output_device_->CommitOverlayPlanes(buffer_presented_callback_,
                                            std::move(*frame));
      } else {
        NOTREACHED();
      }
    } else {
      output_device_->SwapBuffers(buffer_presented_callback_,
                                  std::move(*frame));
    }
  }

  // Reset the primary plane information even on skipped swap.
  output_surface_plane_.reset();

  destroy_after_swap_.clear();
  context_state_->UpdateSkiaOwnedMemorySize();
}

bool SkiaOutputSurfaceImplOnGpu::IsDisplayedAsOverlay() {
  return output_device_->IsPrimaryPlaneOverlay();
}

#if BUILDFLAG(IS_WIN)
void SkiaOutputSurfaceImplOnGpu::DidCreateAcceleratedSurfaceChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  dependency_->DidCreateAcceleratedSurfaceChildWindow(parent_window,
                                                      child_window);
}
#endif

const gpu::gles2::FeatureInfo* SkiaOutputSurfaceImplOnGpu::GetFeatureInfo()
    const {
  return feature_info_.get();
}

const gpu::GpuPreferences& SkiaOutputSurfaceImplOnGpu::GetGpuPreferences()
    const {
  return gpu_preferences_;
}

GpuVSyncCallback SkiaOutputSurfaceImplOnGpu::GetGpuVSyncCallback() {
  return gpu_vsync_callback_;
}

base::TimeDelta SkiaOutputSurfaceImplOnGpu::GetGpuBlockedTimeSinceLastSwap() {
  return dependency_->GetGpuBlockedTimeSinceLastSwap();
}

void SkiaOutputSurfaceImplOnGpu::DidSwapBuffersCompleteInternal(
    gpu::SwapBuffersCompleteParams params,
    const gfx::Size& pixel_size,
    gfx::GpuFenceHandle release_fence) {
  if (params.swap_response.result == gfx::SwapResult::SWAP_FAILED) {
    DLOG(ERROR) << "Context lost on SWAP_FAILED";
    if (!context_state_->IsCurrent(nullptr) ||
        !context_state_->CheckResetStatus(false)) {
      // Mark the context lost if not already lost.
      MarkContextLost(ContextLostReason::CONTEXT_LOST_SWAP_FAILED);
    }
  } else if (params.swap_response.result ==
             gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // We shouldn't present newly reallocated buffers until we have fully
    // initialized their contents. SWAP_NAK_RECREAETE_BUFFERS should trigger a
    // full-screen damage in DirectRenderer, but there is no guarantee that it
    // will happen immediately since the SwapBuffersComplete task gets posted
    // back to the Viz thread and will race with the next invocation of
    // DrawFrame. To ensure we do not display uninitialized memory, we hold
    // off on submitting new frames until we have received a full damage.
    waiting_for_full_damage_ = true;
  }

  PostTaskToClientThread(base::BindOnce(did_swap_buffer_complete_callback_,
                                        params, pixel_size,
                                        std::move(release_fence)));
}

SkiaOutputSurfaceImplOnGpu::DidSwapBufferCompleteCallback
SkiaOutputSurfaceImplOnGpu::GetDidSwapBuffersCompleteCallback() {
  return base::BindRepeating(
      &SkiaOutputSurfaceImplOnGpu::DidSwapBuffersCompleteInternal, weak_ptr_);
}

void SkiaOutputSurfaceImplOnGpu::OnContextLost() {
  MarkContextLost(ContextLostReason::CONTEXT_LOST_UNKNOWN);
}

void SkiaOutputSurfaceImplOnGpu::MarkContextLost(ContextLostReason reason) {
  // This function potentially can be re-entered during from
  // SharedContextState::MarkContextLost(). This guards against it.
  if (context_is_lost_)
    return;
  context_is_lost_ = true;

  UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.DisplayCompositor", reason);

  // Release all ongoing AsyncReadResults.
  ReleaseAsyncReadResultHelpers();

  context_state_->MarkContextLost();
  if (context_lost_callback_) {
    PostTaskToClientThread(std::move(context_lost_callback_));
  }
}

void SkiaOutputSurfaceImplOnGpu::ScheduleCheckReadbackCompletion() {
  if (num_readbacks_pending_ > 0 && !readback_poll_pending_) {
    dependency_->ScheduleDelayedGPUTaskFromGPUThread(
        base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CheckReadbackCompletion,
                       weak_ptr_factory_.GetWeakPtr()));
    readback_poll_pending_ = true;
  }
}

void SkiaOutputSurfaceImplOnGpu::CheckReadbackCompletion() {
  readback_poll_pending_ = false;

  // If there are no pending readback requests or we can't make the context
  // current then exit. There is no thing to do here.
  if (num_readbacks_pending_ == 0 || !MakeCurrent(/*need_framebuffer=*/false))
    return;

  gr_context()->checkAsyncWorkCompletion();
  ScheduleCheckReadbackCompletion();
}

void SkiaOutputSurfaceImplOnGpu::PreserveChildSurfaceControls() {
  if (gl_surface_)
    gl_surface_->PreserveChildSurfaceControls();
}

void SkiaOutputSurfaceImplOnGpu::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  if (gl_surface_) {
    gl_surface_->InitDelegatedInkPointRendererReceiver(
        std::move(pending_receiver));
  }
}

const scoped_refptr<AsyncReadResultLock>
SkiaOutputSurfaceImplOnGpu::GetAsyncReadResultLock() const {
  return async_read_result_lock_;
}

void SkiaOutputSurfaceImplOnGpu::AddAsyncReadResultHelperWithLock(
    AsyncReadResultHelper* helper) {
  async_read_result_lock_->lock().AssertAcquired();
  DCHECK(helper);
  async_read_result_helpers_.insert(helper);
}

void SkiaOutputSurfaceImplOnGpu::RemoveAsyncReadResultHelperWithLock(
    AsyncReadResultHelper* helper) {
  async_read_result_lock_->lock().AssertAcquired();
  DCHECK(helper);
  DCHECK(async_read_result_helpers_.count(helper));
  async_read_result_helpers_.erase(helper);
}

void SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer() {
  // We call GLSurface::SetBackbuffferAllocation in Ensure/Discard backbuffer,
  // so technically need framebuffer. In reality no GLSurface implements it, but
  // until it's removed we should keep true here.
  MakeCurrent(/*need_framebuffer=*/true);
  output_device_->EnsureBackbuffer();
}
void SkiaOutputSurfaceImplOnGpu::DiscardBackbuffer() {
  // We call GLSurface::SetBackbuffferAllocation in Ensure/Discard backbuffer,
  // so technically need framebuffer. In reality no GLSurface implements it, but
  // until it's removed we should keep true here.
  MakeCurrent(/*need_framebuffer=*/true);
  output_device_->DiscardBackbuffer();
}

#if BUILDFLAG(ENABLE_VULKAN)
gfx::GpuFenceHandle SkiaOutputSurfaceImplOnGpu::CreateReleaseFenceForVulkan(
    const GrBackendSemaphore& semaphore) {
  DCHECK(is_using_vulkan());

  if (semaphore.vkSemaphore() == VK_NULL_HANDLE)
    return {};

  auto* implementation = vulkan_context_provider_->GetVulkanImplementation();
  VkDevice device =
      vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice();

  auto handle =
      implementation->GetSemaphoreHandle(device, semaphore.vkSemaphore());
  if (!handle.is_valid()) {
    vkDestroySemaphore(device, semaphore.vkSemaphore(),
                       /*pAllocator=*/nullptr);
    LOG(ERROR) << "Failed to create a release fence for Vulkan.";
    return {};
  }
  return std::move(handle).ToGpuFenceHandle();
}

bool SkiaOutputSurfaceImplOnGpu::CreateAndStoreExternalSemaphoreVulkan(
    std::vector<GrBackendSemaphore>& end_semaphores) {
  DCHECK(is_using_vulkan());

  auto* implementation = vulkan_context_provider_->GetVulkanImplementation();
  VkDevice device =
      vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice();

  VkSemaphore semaphore = implementation->CreateExternalSemaphore(device);
  if (semaphore == VK_NULL_HANDLE) {
    LOG(ERROR)
        << "Creation of an external semaphore for a release fence failed.";
    return false;
  }

  end_semaphores.emplace_back();
  end_semaphores.back().initVulkan(semaphore);
  return true;
}
#endif

gfx::GpuFenceHandle SkiaOutputSurfaceImplOnGpu::CreateReleaseFenceForGL() {
  if (gl::GLFence::IsGpuFenceSupported()) {
    auto fence = gl::GLFence::CreateForGpuFence();
    if (fence)
      return fence->GetGpuFence()->GetGpuFenceHandle().Clone();
  }
  return {};
}

}  // namespace viz
