// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/gl_renderer_copier.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/texture_deleter.h"
#include "components/viz/service/display_embedder/direct_context_provider.h"
#include "components/viz/service/display_embedder/image_context.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/private/SkDeferredDisplayList.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN) && defined(USE_X11)
#include "components/viz/service/display_embedder/skia_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace viz {

class SkiaOutputSurfaceImplOnGpu::ScopedPromiseImageAccess {
 public:
  ScopedPromiseImageAccess(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           std::vector<ImageContext*> image_contexts)
      : impl_on_gpu_(impl_on_gpu), image_contexts_(std::move(image_contexts)) {
    // TODO(penghuang): gather begin read access semaphores from shared images.
    // https://crbug.com/944194
    impl_on_gpu_->BeginAccessImages(image_contexts_);
  }

  ~ScopedPromiseImageAccess() {
    // TODO(penghuang): end shared image access with meaningful semaphores.
    // https://crbug.com/944194
    impl_on_gpu_->EndAccessImages(image_contexts_);
  }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  std::vector<ImageContext*> image_contexts_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPromiseImageAccess);
};

// Skia gr_context() and |context_provider_| share an underlying GLContext.
// Each of them caches some GL state. Interleaving usage could make cached
// state inconsistent with GL state. Using a ScopedUseContextProvider whenever
// |context_provider_| could be accessed (e.g. processing completed queries),
// will keep cached state consistent with driver GL state.
class SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider {
 public:
  ScopedUseContextProvider(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           GLuint texture_client_id)
      : impl_on_gpu_(impl_on_gpu) {
    if (!impl_on_gpu_->MakeCurrent(true /* need_fbo0 */)) {
      valid_ = false;
      return;
    }

    // GLRendererCopier uses context_provider_->ContextGL(), which caches GL
    // state and removes state setting calls that it considers redundant. To get
    // to a known GL state, we first set driver GL state and then make client
    // side consistent with that.
    auto* api = impl_on_gpu_->api_;
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);
    impl_on_gpu_->context_provider_->SetGLRendererCopierRequiredState(
        texture_client_id);
  }

  ~ScopedUseContextProvider() {
    if (valid_)
      impl_on_gpu_->gr_context()->resetContext();
  }

  bool valid() { return valid_; }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  bool valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseContextProvider);
};

namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

scoped_refptr<gpu::gles2::FeatureInfo> CreateFeatureInfo(
    GpuServiceImpl* gpu_service) {
  auto* channel_manager = gpu_service->gpu_channel_manager();
  return base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      channel_manager->gpu_driver_bug_workarounds(),
      channel_manager->gpu_feature_info());
}

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    GpuServiceImpl* gpu_service) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return gpu_service->sync_point_manager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      gpu_service->skia_output_surface_sequence_id());
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(GpuServiceImpl* gpu_service) {
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      gpu_service->shared_image_manager(), nullptr);
}

class ScopedSurfaceToTexture {
 public:
  ScopedSurfaceToTexture(scoped_refptr<DirectContextProvider> context_provider,
                         SkSurface* surface)
      : context_provider_(context_provider) {
    GrBackendTexture skia_texture =
        surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
    GrGLTextureInfo gl_texture_info;
    skia_texture.getGLTextureInfo(&gl_texture_info);
    GLuint client_id = context_provider_->GenClientTextureId();
    auto* texture_manager = context_provider_->texture_manager();
    texture_ref_ =
        texture_manager->CreateTexture(client_id, gl_texture_info.fID);
    texture_manager->SetTarget(texture_ref_.get(), gl_texture_info.fTarget);
    texture_manager->SetLevelInfo(
        texture_ref_.get(), gl_texture_info.fTarget,
        /*level=*/0,
        /*internal_format=*/GL_RGBA, surface->width(), surface->height(),
        /*depth=*/1, /*border=*/0,
        /*format=*/GL_RGBA, /*type=*/GL_UNSIGNED_BYTE,
        /*cleared_rect=*/gfx::Rect(surface->width(), surface->height()));
  }

  ~ScopedSurfaceToTexture() {
    context_provider_->DeleteClientTextureId(client_id());

    // Skia owns the texture. It will delete it when it is done.
    texture_ref_->ForceContextLost();
  }

  GLuint client_id() { return texture_ref_->client_id(); }

 private:
  scoped_refptr<DirectContextProvider> context_provider_;
  scoped_refptr<gpu::gles2::TextureRef> texture_ref_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSurfaceToTexture);
};

// This SingleThreadTaskRunner runs tasks on the GPU main thread, where
// DirectContextProvider can safely service calls. It wraps all posted tasks to
// ensure that |impl_on_gpu_->context_provider_| is made current and in a known
// state when the task is run. If |impl_on_gpu| is destructed, pending tasks are
// no-oped when they are run.
class ContextCurrentTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit ContextCurrentTaskRunner(SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
      : real_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        impl_on_gpu_(impl_on_gpu) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return real_task_runner_->PostDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return real_task_runner_->PostNonNestableDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return real_task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  base::OnceClosure WrapClosure(base::OnceClosure task) {
    return base::BindOnce(
        [](base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
           base::OnceClosure task) {
          if (!impl_on_gpu)
            return;
          SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider scoped_use(
              impl_on_gpu.get(), /*texture_client_id=*/0);
          if (!scoped_use.valid())
            return;

          std::move(task).Run();
        },
        impl_on_gpu_->weak_ptr(), std::move(task));
  }

  ~ContextCurrentTaskRunner() override = default;

  scoped_refptr<base::SingleThreadTaskRunner> real_task_runner_;
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;

  DISALLOW_COPY_AND_ASSIGN(ContextCurrentTaskRunner);
};

}  // namespace

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::~OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface(
    OffscreenSurface&& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface&
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::operator=(
    OffscreenSurface&& offscreen_surface) = default;

SkSurface* SkiaOutputSurfaceImplOnGpu::OffscreenSurface::surface() const {
  return surface_.get();
}

sk_sp<SkPromiseImageTexture>
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::fulfill() {
  DCHECK(surface_);
  if (!promise_texture_) {
    promise_texture_ = SkPromiseImageTexture::Make(
        surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess));
  }
  return promise_texture_;
}

void SkiaOutputSurfaceImplOnGpu::OffscreenSurface::set_surface(
    sk_sp<SkSurface> surface) {
  surface_ = std::move(surface);
  promise_texture_ = {};
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    gpu::SurfaceHandle surface_handle,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MailboxManager* mailbox_manager,
    scoped_refptr<gpu::SyncPointClientState> sync_point_client_state,
    std::unique_ptr<gpu::SharedImageRepresentationFactory> sir_factory,
    gpu::raster::GrShaderCache* gr_shader_cache,
    VulkanContextProvider* vulkan_context_provider,
    const RendererSettings& renderer_settings,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : surface_handle_(surface_handle),
      feature_info_(std::move(feature_info)),
      mailbox_manager_(mailbox_manager),
      sync_point_client_state_(std::move(sync_point_client_state)),
      shared_image_representation_factory_(std::move(sir_factory)),
      gr_shader_cache_(gr_shader_cache),
      vulkan_context_provider_(vulkan_context_provider),
      renderer_settings_(renderer_settings),
      did_swap_buffer_complete_callback_(did_swap_buffer_complete_callback),
      buffer_presented_callback_(buffer_presented_callback),
      context_lost_callback_(context_lost_callback),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  fallback_promise_image_texture_.resize(kLastEnum_SkColorType + 1);
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    GpuServiceImpl* gpu_service,
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : SkiaOutputSurfaceImplOnGpu(
          surface_handle,
          CreateFeatureInfo(gpu_service),
          gpu_service->mailbox_manager(),
          CreateSyncPointClientState(gpu_service),
          CreateSharedImageRepresentationFactory(gpu_service),
          gpu_service->gr_shader_cache(),
          gpu_service->vulkan_context_provider(),
          renderer_settings,
          did_swap_buffer_complete_callback,
          buffer_presented_callback,
          context_lost_callback) {
#if defined(USE_OZONE)
  window_surface_ = ui::OzonePlatform::GetInstance()
                        ->GetSurfaceFactoryOzone()
                        ->CreatePlatformWindowSurface(surface_handle);
#endif
  if (gpu_service) {
    gpu_preferences_ = gpu_service->gpu_channel_manager()->gpu_preferences();
  } else {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    gpu_preferences_ = gpu::gles2::ParseGpuPreferences(command_line);
  }

  if (is_using_vulkan())
    InitializeForVulkan(gpu_service);
  else
    InitializeForGLWithGpuService(gpu_service);
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // |context_provider_| and clients want either the context to be lost or made
  // current on destruction.
  if (MakeCurrent(false /* need_fbo0 */)) {
    // This ensures any outstanding callbacks for promise images are performed.
    gr_context()->flush();
  }
  copier_ = nullptr;
  texture_deleter_ = nullptr;
  context_provider_ = nullptr;

  sync_point_client_state_->Destroy();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil,
    SkSurfaceCharacterization* characterization,
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gr_context());

  base::ScopedClosureRunner scoped_runner;
  if (event) {
    scoped_runner.ReplaceClosure(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  if (!MakeCurrent(surface_handle_ /* need_fbo0 */))
    return;

  size_ = size;
  color_space_ = color_space;
  output_device_->Reshape(size_, device_scale_factor, color_space, has_alpha);

  if (characterization) {
    output_sk_surface()->characterize(characterization);
    DCHECK(characterization->isValid());
  }
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
    std::vector<ImageContext*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);
  DCHECK(output_sk_surface());

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  PullTextureUpdates(std::move(sync_tokens));
  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_) {
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    }
    ScopedPromiseImageAccess scoped_promise_image_access(
        this, std::move(image_contexts));
    output_sk_surface()->draw(ddl.get());
    gr_context()->flush();
  }

  // Note that the ScopedCacheUse for GrShaderCache is scoped until the
  // ReleaseFenceSync call here since releasing the fence may schedule a
  // different decoder's stream which also uses the shader cache.
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);

  if (overdraw_ddl) {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_) {
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    }

    sk_sp<SkSurface> overdraw_surface = SkSurface::MakeRenderTarget(
        gr_context(), overdraw_ddl->characterization(), SkBudgeted::kNo);
    overdraw_surface->draw(overdraw_ddl.get());

    SkPaint paint;
    sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

    sk_sp<SkColorFilter> colorFilter = SkiaHelper::MakeOverdrawColorFilter();
    paint.setColorFilter(colorFilter);
    // TODO(xing.xu): move below to the thread where skia record happens.
    output_sk_surface()->getCanvas()->drawImage(overdraw_image.get(), 0, 0,
                                                &paint);
    gr_context()->flush();
  }
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(output_sk_surface());
  if (!MakeCurrent(surface_handle_ /* need_fbo0 */))
    return;

  DCHECK(output_device_);
  gfx::SwapResponse response;
  if (capabilities().supports_post_sub_buffer) {
    DCHECK(frame.sub_buffer_rect);
    DCHECK(!frame.sub_buffer_rect->IsEmpty());
    if (!capabilities().flipped_output_surface)
      frame.sub_buffer_rect->set_y(size_.height() - frame.sub_buffer_rect->y() -
                                   frame.sub_buffer_rect->height());
    response = output_device_->PostSubBuffer(*frame.sub_buffer_rect,
                                             buffer_presented_callback_);
  } else {
    response = output_device_->SwapBuffers(buffer_presented_callback_);
  }

  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, response.swap_start, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, response.swap_end, 1);
  }
  latency_tracker_.OnGpuSwapBuffersCompleted(frame.latency_info);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    RenderPassId id,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<ImageContext*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  PullTextureUpdates(std::move(sync_tokens));

  auto& offscreen = offscreen_surfaces_[id];
  SkSurfaceCharacterization characterization;
  // TODO(penghuang): Using characterization != ddl->characterization(), when
  // the SkSurfaceCharacterization::operator!= is implemented in Skia.
  if (!offscreen.surface() ||
      !offscreen.surface()->characterize(&characterization) ||
      characterization != ddl->characterization()) {
    offscreen.set_surface(SkSurface::MakeRenderTarget(
        gr_context(), ddl->characterization(), SkBudgeted::kNo));
    DCHECK(offscreen.surface());
  }
  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_)
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    ScopedPromiseImageAccess scoped_promise_image_access(
        this, std::move(image_contexts));
    offscreen.surface()->draw(ddl.get());
    gr_context()->flush();
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!image_contexts.empty());
  for (auto& image_context : image_contexts) {
    auto it = offscreen_surfaces_.find(image_context->render_pass_id);
    DCHECK(it != offscreen_surfaces_.end());
    offscreen_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(crbug.com/898595): Do this on the GPU instead of CPU with Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool from_fbo0 = !id;
  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  DCHECK(from_fbo0 ||
         offscreen_surfaces_.find(id) != offscreen_surfaces_.end());
  auto* surface =
      from_fbo0 ? output_sk_surface() : offscreen_surfaces_[id].surface();

  if (!is_using_vulkan()) {
    // Lazy initialize GLRendererCopier.
    if (!copier_) {
      context_provider_ = base::MakeRefCounted<DirectContextProvider>(
          context_state_->context(), gl_surface_, supports_alpha_,
          gpu_preferences_, feature_info_.get());
      auto result = context_provider_->BindToCurrentThread();
      if (result != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Couldn't initialize GLRendererCopier";
        context_provider_ = nullptr;
        return;
      }
      context_current_task_runner_ =
          base::MakeRefCounted<ContextCurrentTaskRunner>(this);
      texture_deleter_ =
          std::make_unique<TextureDeleter>(context_current_task_runner_);
      copier_ = std::make_unique<GLRendererCopier>(context_provider_,
                                                   texture_deleter_.get());
      copier_->set_async_gl_task_runner(context_current_task_runner_);
    }
    surface->flush();

    GLuint gl_id = 0;
    GLenum internal_format = supports_alpha_ ? GL_RGBA : GL_RGB;
    bool flipped = from_fbo0 ? !capabilities().flipped_output_surface : false;

    base::Optional<ScopedSurfaceToTexture> texture_mapper;
    if (!from_fbo0 || surface_handle_ == gpu::kNullSurfaceHandle) {
      texture_mapper.emplace(context_provider_.get(), surface);
      gl_id = texture_mapper.value().client_id();
      internal_format = GL_RGBA;
    }
    gfx::Size surface_size(surface->width(), surface->height());
    ScopedUseContextProvider use_context_provider(this, gl_id);
    copier_->CopyFromTextureOrFramebuffer(std::move(request), geometry,
                                          internal_format, gl_id, surface_size,
                                          flipped, color_space);

    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries())
      ScheduleDelayedWork();

    return;
  }

  SkBitmap bitmap;
  if (request->is_scaled()) {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.sampling_bounds.x(),
                        geometry.sampling_bounds.y());

    // Execute the scaling: For downscaling, use the RESIZE_BETTER strategy
    // (appropriate for thumbnailing); and, for upscaling, use the RESIZE_BEST
    // strategy. Note that processing is only done on the subset of the
    // RenderPass output that contributes to the result.
    using skia::ImageOperations;
    const bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    const ImageOperations::ResizeMethod method =
        is_downscale_in_both_dimensions ? ImageOperations::RESIZE_BETTER
                                        : ImageOperations::RESIZE_BEST;
    bitmap = ImageOperations::Resize(
        bitmap, method, geometry.result_bounds.width(),
        geometry.result_bounds.height(),
        SkIRect{geometry.result_selection.x(), geometry.result_selection.y(),
                geometry.result_selection.right(),
                geometry.result_selection.bottom()});
  } else {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.result_selection.width(), geometry.result_selection.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.readback_offset.x(),
                        geometry.readback_offset.y());
  }

  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to the
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());

  // Deliver the result. SkiaRenderer supports RGBA_BITMAP and I420_PLANES
  // only. For legacy reasons, if a RGBA_TEXTURE request is being made, clients
  // are prepared to accept RGBA_BITMAP results.
  //
  // TODO(crbug/754872): Get rid of the legacy behavior and send empty results
  // for RGBA_TEXTURE requests once tab capture is moved into VIZ.
  const CopyOutputResult::Format result_format =
      (request->result_format() == CopyOutputResult::Format::RGBA_TEXTURE)
          ? CopyOutputResult::Format::RGBA_BITMAP
          : request->result_format();
  // Note: The CopyOutputSkBitmapResult automatically provides I420 format
  // conversion, if needed.
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      result_format, geometry.result_selection, bitmap));
}

gpu::DecoderContext* SkiaOutputSurfaceImplOnGpu::decoder() {
  return context_provider_->decoder();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PerformDelayedWork,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(2));
}

void SkiaOutputSurfaceImplOnGpu::PerformDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedUseContextProvider use_context_provider(this, /*texture_client_id=*/0);

  delayed_work_pending_ = false;
  if (MakeCurrent(true /* need_fbo0 */)) {
    decoder()->PerformIdleWork();
    decoder()->ProcessPendingQueries(false);
    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries()) {
      ScheduleDelayedWork();
    }
  }
}

sk_sp<SkPromiseImageTexture> SkiaOutputSurfaceImplOnGpu::FallbackPromiseImage(
    ResourceFormat format) {
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
  auto& promise_image_texture = fallback_promise_image_texture_[color_type];
  if (!promise_image_texture) {
    auto image_info = SkImageInfo::Make(1 /* width */, 1 /* height */,
                                        color_type, kOpaque_SkAlphaType);
    auto surface = SkSurface::MakeRenderTarget(
        gr_context(), SkBudgeted::kNo, image_info, 0 /* sampleCount */,
        kTopLeft_GrSurfaceOrigin, nullptr /* surfaceProps */);
    // We don't have driver support for that particular color_type. Try again
    // with well supported SkColorType. Reads from this may have unusual colors
    // due to interpreting RGBA as |color_type|, but that's better than
    // completely undefined.
    if (!surface) {
      image_info =
          SkImageInfo::Make(1 /* width */, 1 /* height */,
                            kRGBA_8888_SkColorType, kOpaque_SkAlphaType);
      surface = SkSurface::MakeRenderTarget(
          gr_context(), SkBudgeted::kNo, image_info, 0 /* sampleCount */,
          kTopLeft_GrSurfaceOrigin, nullptr /* surfaceProps */);

      if (!surface)
        return nullptr;
    }
    auto* canvas = surface->getCanvas();
#if DCHECK_IS_ON()
    canvas->clear(SK_ColorRED);
#else
    canvas->clear(SK_ColorWHITE);
#endif
    fallback_promise_images_.push_back(surface->makeImageSnapshot());
    auto gr_texture = fallback_promise_images_.back()->getBackendTexture(
        false /* flushPendingGrContextIO */);
    promise_image_texture = SkPromiseImageTexture::Make(gr_texture);
  }
  return promise_image_texture;
}

void SkiaOutputSurfaceImplOnGpu::BeginAccessImages(
    const std::vector<ImageContext*>& image_contexts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto* context : image_contexts) {
    // Skip the context if it has been processed.
    if (context->representation_is_being_accessed)
      continue;
    // Prepare for accessing render pass.
    if (context->render_pass_id) {
      // We don't cache promise image for render pass, so the it should always
      // be nullptr.
      auto it = offscreen_surfaces_.find(context->render_pass_id);
      DCHECK(it != offscreen_surfaces_.end());
      context->promise_image_texture = it->second.fulfill();
      if (!context->promise_image_texture) {
        DLOG(ERROR) << "Failed to fulfill the promise texture created from "
                       "RenderPassId:"
                    << context->render_pass_id;
        context->promise_image_texture =
            FallbackPromiseImage(context->resource_format);
      }
      continue;
    }

    // Prepare for accessing shared image.
    if (context->mailbox.IsSharedImage()) {
      if (context->promise_image_texture) {
        // The promise image has been fulfilled early, so we just need begin
        // access the shared image representation. If representation is nullptr,
        // it means the promise image was not fulfilled successfully last time
        // and a fallback content has been used, in that case, we cannot change
        // the content, so we do nothing.
        if (context->representation) {
          // TODO(penghuang): create gather read access semaphores and call
          // skia flush() with them. https://crbug.com/944194
          auto promise_image_texture =
              context->representation->BeginReadAccess();
          // The image has been fulfilled and cached. It is too late to tell
          // skia the backing of the cached image is not accessible right now,
          // so crash for now.
          // TODO(penghuang): find a way to notify skia.
          CHECK(promise_image_texture);
          context->representation_is_being_accessed = true;
        }
        continue;
      }

      auto representation = shared_image_representation_factory_->ProduceSkia(
          context->mailbox, context_state_.get());
      if (!representation) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "mailbox not found in SharedImageManager.";
        context->promise_image_texture =
            FallbackPromiseImage(context->resource_format);
        continue;
      }

      if (!(representation->usage() & gpu::SHARED_IMAGE_USAGE_DISPLAY)) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "was not created with display usage.";
        context->promise_image_texture =
            FallbackPromiseImage(context->resource_format);
        continue;
      }

      context->representation = std::move(representation);
      // TODO(penghuang): create gather read access semaphores and call
      // skia flush() with them. https://crbug.com/944194
      context->promise_image_texture =
          context->representation->BeginReadAccess();
      if (!context->promise_image_texture) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "begin read access failed..";
        context->promise_image_texture =
            FallbackPromiseImage(context->resource_format);
        continue;
      }
      context->representation_is_being_accessed = true;
      continue;
    }

    // Prepare for accessing legacy mailbox.
    // The promise image has been fulfilled once, so we do need do anything.
    if (context->promise_image_texture)
      continue;

    if (is_using_vulkan()) {
      // Probably this texture is created with wrong interface
      // (GLES2Interface).
      DLOG(ERROR)
          << "Failed to fulfill the promise texture whose backend is not "
             "compitable with vulkan.";
      context->promise_image_texture =
          FallbackPromiseImage(context->resource_format);
      continue;
    }

    auto* texture_base = mailbox_manager_->ConsumeTexture(context->mailbox);
    if (!texture_base) {
      DLOG(ERROR) << "Failed to fulfill the promise texture.";
      context->promise_image_texture =
          FallbackPromiseImage(context->resource_format);
      continue;
    }
    BindOrCopyTextureIfNecessary(texture_base);
    GrBackendTexture backend_texture;
    gpu::GetGrBackendTexture(gl_version_info_, texture_base->target(),
                             context->size, texture_base->service_id(),
                             context->resource_format, &backend_texture);
    if (!backend_texture.isValid()) {
      DLOG(ERROR) << "Failed to fulfill the promise texture.";
      context->promise_image_texture =
          FallbackPromiseImage(context->resource_format);
      continue;
    }
    context->promise_image_texture =
        SkPromiseImageTexture::Make(backend_texture);
  }
}

void SkiaOutputSurfaceImplOnGpu::EndAccessImages(
    const std::vector<ImageContext*>& image_contexts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto* context : image_contexts) {
    if (!context->representation_is_being_accessed)
      continue;
    // TODO(penghuang): create end read access semaphores and call
    // EndReadAccess() with them. https://crbug.com/944194
    DCHECK(context->representation);
    context->representation->EndReadAccess();
    context->representation_is_being_accessed = false;
  }
}

sk_sp<GrContextThreadSafeProxy>
SkiaOutputSurfaceImplOnGpu::GetGrContextThreadSafeProxy() {
  return gr_context()->threadSafeProxy();
}

void SkiaOutputSurfaceImplOnGpu::ReleaseSkImages(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  DCHECK(!image_contexts.empty());
  // The window could be destroyed already, and the MakeCurrent will fail with
  // an destroyed window, so MakeCurrent without requiring the fbo0.
  MakeCurrent(false /* need_fbo0 */);
}

void SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting(
    const OutputSurface::Capabilities& capabilities) {
  MakeCurrent(false /* need_fbo0 */);
  // Check that we're using an offscreen surface.
  DCHECK(!surface_handle_);
  output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
      gr_context(), capabilities.flipped_output_surface,
      renderer_settings_.requires_alpha_channel,
      did_swap_buffer_complete_callback_);
}

void SkiaOutputSurfaceImplOnGpu::InitializeForGLWithGpuService(
    GpuServiceImpl* gpu_service) {
  std::unique_ptr<SkiaOutputDeviceGL> onscreen_device;
  if (surface_handle_) {
    onscreen_device = std::make_unique<SkiaOutputDeviceGL>(
        surface_handle_, feature_info_, did_swap_buffer_complete_callback_);
    gl_surface_ = onscreen_device->gl_surface();
  } else {
    gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  }
  DCHECK(gl_surface_);

  context_state_ = gpu_service->GetContextState();
  if (!context_state_) {
    LOG(FATAL) << "Failed to create GrContext";
    // TODO(penghuang): handle the failure.
  }

  auto* context = context_state_->real_context();
  auto* current_gl = context->GetCurrentGL();
  api_ = current_gl->Api;
  gl_version_info_ = context->GetVersionInfo();

  if (onscreen_device) {
    if (!MakeCurrent(true /* need_fbo0 */)) {
      LOG(FATAL) << "Failed to make current during initialization.";
      return;
    }
    onscreen_device->Initialize(gr_context(), context);
    supports_alpha_ = onscreen_device->supports_alpha();
    output_device_ = std::move(onscreen_device);
  } else {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        gr_context(), true /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  }
}

void SkiaOutputSurfaceImplOnGpu::InitializeForVulkan(
    GpuServiceImpl* gpu_service) {
  context_state_ = gpu_service->GetContextState();
  DCHECK(context_state_);
#if BUILDFLAG(ENABLE_VULKAN)
  if (surface_handle_ == gpu::kNullSurfaceHandle) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        gr_context(), false /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  } else {
#if defined(USE_X11)
    supports_alpha_ = true;
    if (gpu_preferences_.disable_vulkan_surface) {
      output_device_ = std::make_unique<SkiaOutputDeviceX11>(
          gr_context(), surface_handle_, did_swap_buffer_complete_callback_);
    } else {
      output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
          vulkan_context_provider_, surface_handle_,
          did_swap_buffer_complete_callback_);
    }
#else
    output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
        vulkan_context_provider_, surface_handle_,
        did_swap_buffer_complete_callback_);
#endif
  }
#endif
}

void SkiaOutputSurfaceImplOnGpu::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (texture_base->GetType() != gpu::TextureBase::Type::kValidated)
    return;
  // If a texture is validated and bound to an image, we may defer copying the
  // image to the texture until the texture is used. It is for implementing low
  // latency drawing (e.g. fast ink) and avoiding unnecessary texture copy. So
  // we need check the texture image state, and bind or copy the image to the
  // texture if necessary.
  auto* texture = gpu::gles2::Texture::CheckedCast(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
      if (!image->BindTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to bind a gl image to texture.";
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to copy a gl image to texture.";
    }
  }
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent(bool need_fbo0) {
  if (!is_using_vulkan()) {
    // Only make current with |gl_surface_|, if following operations will use
    // fbo0.
    if (!context_state_->MakeCurrent(need_fbo0 ? gl_surface_.get() : nullptr)) {
      LOG(ERROR) << "Failed to make current.";
      context_lost_callback_.Run();
      if (context_provider_)
        context_provider_->MarkContextLost();
      return false;
    }
    context_state_->set_need_context_state_reset(true);
  }
  return true;
}

void SkiaOutputSurfaceImplOnGpu::PullTextureUpdates(
    std::vector<gpu::SyncToken> sync_tokens) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (mailbox_manager_->UsesSync()) {
    for (auto& sync_token : sync_tokens)
      mailbox_manager_->PullTextureUpdates(sync_token);
  }
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSyncAndPushTextureUpdates(
    uint64_t sync_fence_release) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (mailbox_manager_->UsesSync()) {
    // If MailboxManagerSync is used, we are sharing textures between threads.
    // In this case, sync point can only guarantee GL commands are issued in
    // correct order across threads and GL contexts. However GPU driver may
    // execute GL commands out of the issuing order across GL contexts. So we
    // have to use PushTextureUpdates() and PullTextureUpdates() to ensure the
    // correct GL commands executing order.
    // PushTextureUpdates(token) will insert a GL fence into the current GL
    // context, PullTextureUpdates(token) will wait the GL fence associated with
    // the give token on the current GL context.
    // Reconstruct sync_token from sync_fence_release.
    gpu::SyncToken sync_token(
        gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
        command_buffer_id(), sync_fence_release);
    mailbox_manager_->PushTextureUpdates(sync_token);
  }
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

}  // namespace viz
