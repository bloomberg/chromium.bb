// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/single_task_sequence.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

namespace viz {

namespace {

sk_sp<SkPromiseImageTexture> Fulfill(void* texture_context) {
  DCHECK(texture_context);
  auto* image_context = static_cast<ImageContextImpl*>(texture_context);
  return sk_ref_sp(image_context->promise_image_texture());
}

gpu::ContextUrl& GetActiveUrl() {
  static base::NoDestructor<gpu::ContextUrl> active_url(
      GURL("chrome://gpu/SkiaRenderer"));
  return *active_url;
}

OutputSurface::Type GetOutputSurfaceType(SkiaOutputSurfaceDependency* deps) {
  // TODO(penghuang): Support more types.
  return deps->IsUsingVulkan() ? OutputSurface::Type::kVulkan
                               : OutputSurface::Type::kOpenGL;
}

}  // namespace

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    SkDeferredDisplayListRecorder* root_recorder)
    : recorder_(root_recorder), render_pass_id_(0) {}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    SkSurfaceCharacterization characterization)
    : ScopedPaint(characterization, AggregatedRenderPassId(0), gpu::Mailbox()) {
}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    SkSurfaceCharacterization characterization,
    AggregatedRenderPassId render_pass_id,
    gpu::Mailbox mailbox)
    : render_pass_id_(render_pass_id), mailbox_(mailbox) {
  recorder_storage_.emplace(characterization);
  recorder_ = &recorder_storage_.value();
}

SkiaOutputSurfaceImpl::ScopedPaint::~ScopedPaint() = default;

SkiaOutputSurfaceImpl::FrameBufferDamageTracker::FrameBufferDamageTracker(
    size_t number_of_buffers)
    : number_of_buffers_(number_of_buffers) {}

SkiaOutputSurfaceImpl::FrameBufferDamageTracker::~FrameBufferDamageTracker() =
    default;

void SkiaOutputSurfaceImpl::FrameBufferDamageTracker::FrameBuffersChanged(
    const gfx::Size& frame_buffer_size) {
  frame_buffer_size_ = frame_buffer_size;
  damage_between_frames_.clear();
  cached_current_damage_.reset();
}

void SkiaOutputSurfaceImpl::FrameBufferDamageTracker::SwappedWithDamage(
    const gfx::Rect& damage) {
  damage_between_frames_.push_back(damage);
  // Keep at most `number_of_buffers_` frames.
  if (damage_between_frames_.size() > number_of_buffers_) {
    damage_between_frames_.pop_front();
  }
  cached_current_damage_.reset();
}

void SkiaOutputSurfaceImpl::FrameBufferDamageTracker::SkippedSwapWithDamage(
    const gfx::Rect& damage) {
  if (!damage_between_frames_.empty()) {
    damage_between_frames_.back().Union(damage);
    cached_current_damage_.reset();
  } else {
    // First frame after `FrameBuffersChanged already has full damage.
    // So no need to keep track of it with another entry, which would violate
    // the condition the deque size is at most `number_of_buffers_ - 1`.
  }
}

gfx::Rect
SkiaOutputSurfaceImpl::FrameBufferDamageTracker::GetCurrentFrameBufferDamage()
    const {
  if (!cached_current_damage_)
    cached_current_damage_ = ComputeCurrentFrameBufferDamage();
  return *cached_current_damage_;
}

gfx::Rect SkiaOutputSurfaceImpl::FrameBufferDamageTracker::
    ComputeCurrentFrameBufferDamage() const {
  // First `number_of_buffers_` frames has full frame damage.
  if (damage_between_frames_.size() < number_of_buffers_) {
    return gfx::Rect(frame_buffer_size_);
  }

  // Subsequent frames has `number_of_buffers_ - 1` frames of incremental
  // damange unioned. Note index 0 is specifically skipped over its the damage
  // that's last drawn into that's drawn into the current frame buffer.
  gfx::Rect result;
  for (size_t i = 1; i < damage_between_frames_.size(); ++i) {
    result.Union(damage_between_frames_[i]);
  }
  return result;
}

// static
std::unique_ptr<SkiaOutputSurface> SkiaOutputSurfaceImpl::Create(
    DisplayCompositorMemoryAndTaskController* display_controller,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings) {
  DCHECK(display_controller);
  DCHECK(display_controller->skia_dependency());
  DCHECK(display_controller->gpu_task_scheduler());
  auto output_surface = std::make_unique<SkiaOutputSurfaceImpl>(
      base::PassKey<SkiaOutputSurfaceImpl>(), display_controller,
      renderer_settings, debug_settings);
  if (!output_surface->Initialize())
    output_surface = nullptr;
  return output_surface;
}

SkiaOutputSurfaceImpl::SkiaOutputSurfaceImpl(
    base::PassKey<SkiaOutputSurfaceImpl> /* pass_key */,
    DisplayCompositorMemoryAndTaskController* display_controller,
    const RendererSettings& renderer_settings,
    const DebugRendererSettings* debug_settings)
    : SkiaOutputSurface(
          GetOutputSurfaceType(display_controller->skia_dependency())),
      dependency_(display_controller->skia_dependency()),
      renderer_settings_(renderer_settings),
      debug_settings_(debug_settings),
      display_compositor_controller_(display_controller),
      gpu_task_scheduler_(display_compositor_controller_->gpu_task_scheduler()),
      is_using_raw_draw_(features::IsUsingRawDraw()),
      is_raw_draw_using_msaa_(features::IsRawDrawUsingMSAA()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_using_raw_draw_) {
    auto* manager = dependency_->GetSharedImageManager();
    DCHECK(manager->is_thread_safe());
    representation_factory_ =
        std::make_unique<gpu::SharedImageRepresentationFactory>(manager,
                                                                nullptr);
  }
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  current_paint_.reset();
  root_recorder_.reset();

  if (!render_pass_image_cache_.empty()) {
    std::vector<AggregatedRenderPassId> render_pass_ids;
    render_pass_ids.reserve(render_pass_ids.size());
    for (auto& entry : render_pass_image_cache_)
      render_pass_ids.push_back(entry.first);
    RemoveRenderPassResource(std::move(render_pass_ids));
  }
  DCHECK(render_pass_image_cache_.empty());

  // Post a task to destroy |impl_on_gpu_| on the GPU thread.
  auto task = base::BindOnce(
      [](std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu) {},
      std::move(impl_on_gpu_));
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  // Flush GPU tasks and block until all tasks are finished.
  FlushGpuTasks(SyncMode::kWaitForTasksFinished);
}

gpu::SurfaceHandle SkiaOutputSurfaceImpl::GetSurfaceHandle() const {
  return dependency_->GetSurfaceHandle();
}

void SkiaOutputSurfaceImpl::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SkiaOutputSurfaceImpl::BindFramebuffer() {
  // TODO(penghuang): remove this method when GLRenderer is removed.
}

void SkiaOutputSurfaceImpl::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(capabilities().supports_dc_layers);

  if (has_set_draw_rectangle_for_frame_)
    return;

  // TODO(kylechar): Add a check that |draw_rectangle| is the full size of the
  // framebuffer the next time this is called after Reshape().

  draw_rectangle_.emplace(draw_rectangle);
  has_set_draw_rectangle_for_frame_ = true;
}

void SkiaOutputSurfaceImpl::SetEnableDCLayers(bool enable) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(capabilities().supports_dc_layers);

  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetEnableDCLayers,
                             base::Unretained(impl_on_gpu_.get()), enable);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::DiscardBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::RecreateRootRecorder() {
  DCHECK(characterization_.isValid());
  root_recorder_.emplace(characterization_);

  // This will trigger the lazy initialization of the recorder
  std::ignore = root_recorder_->getCanvas();
}

void SkiaOutputSurfaceImpl::Reshape(const gfx::Size& size,
                                    float device_scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    gfx::BufferFormat format,
                                    bool use_stencil) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!size.IsEmpty());

  // SetDrawRectangle() will need to be called at the new size.
  has_set_draw_rectangle_for_frame_ = false;

  if (use_damage_area_from_skia_output_device_) {
    damage_of_current_buffer_ = gfx::Rect(size);
  } else if (frame_buffer_damage_tracker_) {
    frame_buffer_damage_tracker_->FrameBuffersChanged(size);
  }

  if (is_using_raw_draw_ && is_raw_draw_using_msaa_) {
    if (base::SysInfo::IsLowEndDevice()) {
      // On "low-end" devices use 4 samples per pixel to save memory.
      sample_count_ = 4;
    } else {
      sample_count_ = device_scale_factor >= 2.0f ? 4 : 8;
    }
  } else {
    sample_count_ = 1;
  }

  const auto format_index = static_cast<int>(format);
  const auto& color_type = capabilities_.sk_color_types[format_index];
  DCHECK(color_type != kUnknown_SkColorType)
      << "SkColorType is invalid for buffer format_index: " << format_index;

  auto sk_color_space = color_space.ToSkColorSpace();
  characterization_ = CreateSkSurfaceCharacterization(
      size, color_type, /*mipmap=*/false, std::move(sk_color_space),
      /*is_root_render_pass=*/true, /*is_overlay=*/false);

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::Reshape,
                     base::Unretained(impl_on_gpu_.get()), characterization_,
                     color_space, device_scale_factor, GetDisplayTransform());
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/true,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());
  FlushGpuTasks(SyncMode::kNoWait);

  size_ = size;
  format_ = format;
  RecreateRootRecorder();
}

void SkiaOutputSurfaceImpl::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

void SkiaOutputSurfaceImpl::SetGpuVSyncEnabled(bool enabled) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetGpuVSyncEnabled,
                             base::Unretained(impl_on_gpu_.get()), enabled);
  gpu_task_scheduler_->ScheduleOrRetainGpuTask(std::move(task), {});
}

void SkiaOutputSurfaceImpl::SetGpuVSyncCallback(GpuVSyncCallback callback) {
  gpu_vsync_callback_ = std::move(callback);
}

void SkiaOutputSurfaceImpl::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  display_transform_ = transform;
}

gfx::OverlayTransform SkiaOutputSurfaceImpl::GetDisplayTransform() {
  switch (capabilities_.orientation_mode) {
    case OutputSurface::OrientationMode::kLogic:
      return gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
    case OutputSurface::OrientationMode::kHardware:
      return display_transform_;
  }
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!current_paint_);
  DCHECK(root_recorder_);

  current_paint_.emplace(&root_recorder_.value());

  if (!debug_settings_->show_overdraw_feedback)
    return current_paint_->recorder()->getCanvas();

  DCHECK(!overdraw_surface_recorder_);
  DCHECK(debug_settings_->show_overdraw_feedback);

  nway_canvas_.emplace(characterization_.width(), characterization_.height());
  nway_canvas_->addCanvas(current_paint_->recorder()->getCanvas());

  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      gfx::Size(characterization_.width(), characterization_.height()),
      characterization_.colorType(),
      /*mipmap=*/false, characterization_.refColorSpace(),
      /*is_root_render_pass=*/false, /*is_overlay=*/false);
  if (characterization.isValid()) {
    overdraw_surface_recorder_.emplace(characterization);
    overdraw_canvas_.emplace((overdraw_surface_recorder_->getCanvas()));
    nway_canvas_->addCanvas(&overdraw_canvas_.value());
  }

  return &nway_canvas_.value();
}

void SkiaOutputSurfaceImpl::MakePromiseSkImage(ImageContext* image_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(!image_context->mailbox_holder().mailbox.IsZero());
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImpl::MakePromiseSkImage");

  images_in_current_paint_.push_back(
      static_cast<ImageContextImpl*>(image_context));

  const auto& mailbox_holder = image_context->mailbox_holder();

  if (representation_factory_) {
    auto* sync_point_manager = dependency_->GetSyncPointManager();
    auto const& sync_token = mailbox_holder.sync_token;
    if (sync_token.HasData() &&
        !sync_point_manager->IsSyncTokenReleased(sync_token)) {
      gpu_task_sync_tokens_.push_back(sync_token);
      FlushGpuTasks(SyncMode::kWaitForTasksStarted);
      image_context->mutable_mailbox_holder()->sync_token.Clear();
    }

    auto* impl = static_cast<ImageContextImpl*>(image_context);
    if (impl->BeginRasterAccess(representation_factory_.get()))
      return;
  }

  if (image_context->has_image())
    return;

  SkColorType color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, image_context->resource_format());
  GrBackendFormat backend_format = GetGrBackendFormatForTexture(
      image_context->resource_format(),
      image_context->mailbox_holder().texture_target,
      image_context->ycbcr_info());
  auto image = SkImage::MakePromiseTexture(
      gr_context_thread_safe_, backend_format,
      {image_context->size().width(), image_context->size().height()},
      GrMipMapped::kNo, image_context->origin(), color_type,
      image_context->alpha_type(), image_context->color_space(), Fulfill,
      /*textureReleaseProc=*/nullptr, image_context);
  image_context->SetImage(std::move(image), backend_format);

  if (mailbox_holder.sync_token.HasData()) {
    resource_sync_tokens_.push_back(mailbox_holder.sync_token);
    image_context->mutable_mailbox_holder()->sync_token.Clear();
  }
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromYUV(
    const std::vector<ImageContext*>& contexts,
    sk_sp<SkColorSpace> image_color_space,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config)) ==
         contexts.size());

  auto* y_context = static_cast<ImageContextImpl*>(contexts[0]);
  // Note: YUV to RGB conversion is handled by a color filter in SkiaRenderer.
  SkYUVAInfo yuva_info({y_context->size().width(), y_context->size().height()},
                       plane_config, subsampling, kIdentity_SkYUVColorSpace);

  GrBackendFormat formats[4] = {};
  SkDeferredDisplayListRecorder::PromiseImageTextureContext
      texture_contexts[4] = {};
  for (size_t i = 0; i < contexts.size(); ++i) {
    auto* context = static_cast<ImageContextImpl*>(contexts[i]);
    DCHECK(context->origin() == kTopLeft_GrSurfaceOrigin);
    formats[i] = GetGrBackendFormatForTexture(
        context->resource_format(), context->mailbox_holder().texture_target,
        /*ycbcr_info=*/absl::nullopt);

    // NOTE: We don't have promises for individual planes, but still need format
    // for fallback
    context->SetImage(nullptr, formats[i]);

    if (context->mailbox_holder().sync_token.HasData()) {
      resource_sync_tokens_.push_back(context->mailbox_holder().sync_token);
      context->mutable_mailbox_holder()->sync_token.Clear();
    }
    images_in_current_paint_.push_back(context);
    texture_contexts[i] = context;
  }

  GrYUVABackendTextureInfo yuva_backend_info(
      yuva_info, formats, GrMipmapped::kNo, kTopLeft_GrSurfaceOrigin);
  auto image = SkImage::MakePromiseYUVATexture(
      gr_context_thread_safe_, yuva_backend_info, std::move(image_color_space),
      Fulfill,
      /*textureReleaseProc=*/nullptr, texture_contexts);
  DCHECK(image);
  return image;
}

gpu::SyncToken SkiaOutputSurfaceImpl::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  if (image_contexts.empty())
    return gpu::SyncToken();

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts,
      base::Unretained(impl_on_gpu_.get()), std::move(image_contexts));
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
  return Flush();
}

std::unique_ptr<ExternalUseClient::ImageContext>
SkiaOutputSurfaceImpl::CreateImageContext(
    const gpu::MailboxHolder& holder,
    const gfx::Size& size,
    ResourceFormat format,
    bool maybe_concurrent_reads,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool raw_draw_if_possible) {
  return std::make_unique<ImageContextImpl>(
      holder, size, format, maybe_concurrent_reads, ycbcr_info,
      std::move(color_space),
      /*allow_keeping_read_access=*/true, raw_draw_if_possible);
}

void SkiaOutputSurfaceImpl::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!current_paint_);
  DCHECK_EQ(!frame.sub_buffer_rect || !frame.sub_buffer_rect->IsEmpty(),
            current_buffer_modified_);

  has_set_draw_rectangle_for_frame_ = false;

  // If current_buffer_modified_ is false, it means SkiaRenderer doesn't draw
  // anything for current frame. So this SwapBuffer() must be a empty swap, so
  // the previous buffer will be used for this frame.
  if (frame_buffer_damage_tracker_ && current_buffer_modified_) {
    gfx::Rect damage_rect =
        frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(size_);
    frame_buffer_damage_tracker_->SwappedWithDamage(damage_rect);
  }
  current_buffer_modified_ = false;

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffers,
                     base::Unretained(impl_on_gpu_.get()), std::move(frame));
  EnqueueGpuTask(std::move(callback), std::move(resource_sync_tokens_),
                 /*make_current=*/true,
                 /*need_framebuffer=*/!dependency_->IsOffscreen());

  // Recreate |root_recorder_| after SwapBuffers has been scheduled on GPU
  // thread to save some time in BeginPaintCurrentFrame
  // TODO(vasilyt): reuse root recorder
  RecreateRootRecorder();
}

void SkiaOutputSurfaceImpl::SwapBuffersSkipped(
    const gfx::Rect root_pass_damage_rect) {
  if (current_buffer_modified_ && frame_buffer_damage_tracker_) {
    // If |current_buffer_modified_| is true but we skipped swap there is still
    // damage to the current framebuffer to account for. Unlike SwapBuffers()
    // don't reset current buffers rect, since that damage still need to be
    // taken into account when the buffer is swapped later.
    frame_buffer_damage_tracker_->SkippedSwapWithDamage(root_pass_damage_rect);
  }
  current_buffer_modified_ = false;

  // PostTask to the GPU thread to deal with freeing resources and running
  // callbacks.
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffersSkipped,
                             base::Unretained(impl_on_gpu_.get()));
  // SwapBuffersSkipped currently does mostly the same as SwapBuffers and needs
  // MakeCurrent.
  EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                 /*make_current=*/true, /*need_framebuffer=*/false);

  // TODO(vasilyt): reuse root recorder
  RecreateRootRecorder();
}

void SkiaOutputSurfaceImpl::ScheduleOutputSurfaceAsOverlay(
    OverlayProcessorInterface::OutputSurfaceOverlayPlane output_surface_plane) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay,
      base::Unretained(impl_on_gpu_.get()), std::move(output_surface_plane));
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!current_paint_);
  DCHECK(resource_sync_tokens_.empty());

  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      surface_size, color_type, mipmap, std::move(color_space),
      /*is_root_render_pass=*/false, /*is_overlay=*/false);
  if (!characterization.isValid())
    return nullptr;

  current_paint_.emplace(characterization, id, mailbox);
  return current_paint_->recorder()->getCanvas();
}

#if BUILDFLAG(IS_APPLE) || defined(USE_OZONE)
SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPassOverlay(
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!current_paint_);
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      size, color_type, mipmap, std::move(color_space),
      /*is_root_render_pass=*/false, /*is_overlay=*/true);
  if (!characterization.isValid())
    return nullptr;

  current_paint_.emplace(characterization);
  return current_paint_->recorder()->getCanvas();
}

sk_sp<SkDeferredDisplayList>
SkiaOutputSurfaceImpl::EndPaintRenderPassOverlay() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);

  auto ddl = current_paint_->recorder()->detach();
  current_paint_.reset();
  return ddl;
}
#endif  // BUILDFLAG(IS_APPLE) || defined(USE_OZONE)

void SkiaOutputSurfaceImpl::EndPaint(base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  // If current_render_pass_id_ is not null, we are painting a render pass.
  // Otherwise we are painting a frame.

  bool painting_render_pass = !current_paint_->render_pass_id().is_null();

  auto ddl = current_paint_->recorder()->detach();

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  if (painting_render_pass) {
    auto it = render_pass_image_cache_.find(current_paint_->render_pass_id());
    if (it != render_pass_image_cache_.end()) {
      // We are going to overwrite the render pass, so we need reset the
      // image_context, so a new promise image will be created when the
      // MakePromiseSkImageFromRenderPass() is called.
      it->second->clear_image();
    }

    auto task = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass,
        base::Unretained(impl_on_gpu_.get()), current_paint_->mailbox(),
        std::move(ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, std::move(on_finished));
    EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                   /*make_current=*/true, /*need_framebuffer=*/false);
  } else {
    // Draw on the root render pass.
    current_buffer_modified_ = true;
    sk_sp<SkDeferredDisplayList> overdraw_ddl;
    if (overdraw_surface_recorder_) {
      overdraw_ddl = overdraw_surface_recorder_->detach();
      DCHECK(overdraw_ddl);
      overdraw_canvas_.reset();
      overdraw_surface_recorder_.reset();
    }
    nway_canvas_.reset();

    auto task = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame,
        base::Unretained(impl_on_gpu_.get()), std::move(ddl),
        std::move(overdraw_ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, std::move(on_finished), draw_rectangle_);
    EnqueueGpuTask(std::move(task), std::move(resource_sync_tokens_),
                   /*make_current=*/true, /*need_framebuffer=*/true);
    draw_rectangle_.reset();
  }
  images_in_current_paint_.clear();
  current_paint_.reset();
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);

  auto& image_context = render_pass_image_cache_[id];
  if (!image_context) {
    gpu::MailboxHolder mailbox_holder(mailbox, gpu::SyncToken(), 0);
    image_context = std::make_unique<ImageContextImpl>(
        mailbox_holder, size, format, /*maybe_concurrent_reads=*/false,
        /*ycbcr_info=*/absl::nullopt, std::move(color_space),
        /*allow_keeping_read_access=*/false);
  }
  if (!image_context->has_image()) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    GrBackendFormat backend_format = GetGrBackendFormatForTexture(
        format, GL_TEXTURE_2D, /*ycbcr_info=*/absl::nullopt);
    auto image = SkImage::MakePromiseTexture(
        gr_context_thread_safe_, backend_format,
        {image_context->size().width(), image_context->size().height()},
        mipmap ? GrMipMapped::kYes : GrMipMapped::kNo, image_context->origin(),
        color_type, image_context->alpha_type(), image_context->color_space(),
        Fulfill,
        /*textureReleaseProc=*/nullptr, image_context.get());
    image_context->SetImage(std::move(image), backend_format);
    if (!image_context->has_image()) {
      return nullptr;
    }
  }
  images_in_current_paint_.push_back(image_context.get());
  return image_context->image();
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<AggregatedRenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  std::vector<std::unique_ptr<ExternalUseClient::ImageContext>> image_contexts;
  image_contexts.reserve(ids.size());
  for (const auto id : ids) {
    auto it = render_pass_image_cache_.find(id);
    // If the render pass was only used for a copy request, there won't be a
    // matching entry in |render_pass_image_cache_|.
    if (it != render_pass_image_cache_.end()) {
      it->second->clear_image();
      image_contexts.push_back(std::move(it->second));
      render_pass_image_cache_.erase(it);
    }
  }

  if (!image_contexts.empty()) {
    // impl_on_gpu_ is released on the GPU thread by a posted task from
    // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
    auto callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts,
        base::Unretained(impl_on_gpu_.get()), std::move(image_contexts));
    // ReleaseImageContexts will delete gpu resources and needs MakeCurrent.
    EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                   /*need_framebuffer=*/false);
  }
}

void SkiaOutputSurfaceImpl::CopyOutput(
    AggregatedRenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (request->has_blit_request()) {
    for (const auto& mailbox_holder : request->blit_request().mailboxes()) {
      if (mailbox_holder.sync_token.HasData()) {
        resource_sync_tokens_.push_back(mailbox_holder.sync_token);
      }
    }
  }

  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CopyOutput,
                     base::Unretained(impl_on_gpu_.get()), id, geometry,
                     color_space, std::move(request), mailbox);
  EnqueueGpuTask(std::move(callback), std::move(resource_sync_tokens_),
                 /*make_current=*/true, /*need_framebuffer=*/!id);
}

void SkiaOutputSurfaceImpl::ScheduleOverlays(
    OverlayList overlays,
    std::vector<gpu::SyncToken> sync_tokens,
    base::OnceClosure on_finished) {
#if BUILDFLAG(IS_APPLE) || defined(USE_OZONE)
  // If there are render pass overlays, then a gl context is needed for drawing
  // the overlay render passes to a backing for being scanned out.
  bool make_current =
      std::find_if(overlays.begin(), overlays.end(), [](const auto& overlay) {
        return !!overlay.ddl;
      }) != overlays.end();
  // Append |resource_sync_tokens_| which are depended by drawing render passes
  // to overlay backings.
  std::move(resource_sync_tokens_.begin(), resource_sync_tokens_.end(),
            std::back_inserter(sync_tokens));
  resource_sync_tokens_.clear();
#else
  bool make_current = false;
#endif
  auto task = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ScheduleOverlays,
      base::Unretained(impl_on_gpu_.get()), std::move(overlays),
      std::move(images_in_current_paint_), std::move(on_finished));
  EnqueueGpuTask(std::move(task), std::move(sync_tokens), make_current,
                 /*need_framebuffer=*/false);
  images_in_current_paint_.clear();
}

void SkiaOutputSurfaceImpl::SetFrameRate(float frame_rate) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetFrameRate,
                             base::Unretained(impl_on_gpu_.get()), frame_rate);
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::SetCapabilitiesForTesting(
    gfx::SurfaceOrigin output_surface_origin) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(impl_on_gpu_);
  capabilities_.output_surface_origin = output_surface_origin;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting,
                     base::Unretained(impl_on_gpu_.get()), capabilities_);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/true,
                 /*need_framebuffer=*/false);
}

bool SkiaOutputSurfaceImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();

  // This runner could be called from vsync or GPU thread after |this| is
  // destroyed. We post directly to display compositor thread to check
  // |weak_ptr_| as |dependency_| may have been destroyed.
#if BUILDFLAG(IS_ANDROID)
  // Callback is never used on Android. Doesn't work with WebView because
  // calling it bypasses SkiaOutputSurfaceDependency.
  GpuVSyncCallback vsync_callback_runner = base::DoNothing();
#else
  GpuVSyncCallback vsync_callback_runner = base::BindRepeating(
      [](scoped_refptr<base::SingleThreadTaskRunner> runner,
         base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr,
         base::TimeTicks timebase, base::TimeDelta interval) {
        runner->PostTask(FROM_HERE,
                         base::BindOnce(&SkiaOutputSurfaceImpl::OnGpuVSync,
                                        weak_ptr, timebase, interval));
      },
      base::ThreadTaskRunnerHandle::Get(), weak_ptr_);
#endif

  bool result = false;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                     base::Unretained(this), vsync_callback_runner, &result);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  // |capabilities_| will be initialized in InitializeOnGpuThread(), so have to
  // wait.
  FlushGpuTasks(SyncMode::kWaitForTasksFinished);

  if (capabilities_.preserve_buffer_content &&
      capabilities_.supports_post_sub_buffer) {
    capabilities_.only_invalidates_damage_rect = false;
    // If there is only one pending frame, then we can use damage area hint from
    // SkiaOutputDevice, otherwise we have to track damage area with
    // FrameBufferDamageTracker.
    if (capabilities_.pending_swap_params.max_pending_swaps == 1 &&
        capabilities_.damage_area_from_skia_output_device) {
      use_damage_area_from_skia_output_device_ = true;
      damage_of_current_buffer_ = gfx::Rect();
    } else {
      frame_buffer_damage_tracker_.emplace(capabilities_.number_of_buffers);
    }
  }
  return result;
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(
    GpuVSyncCallback vsync_callback_runner,
    bool* result) {
  auto did_swap_buffer_complete_callback = base::BindRepeating(
      &SkiaOutputSurfaceImpl::DidSwapBuffersComplete, weak_ptr_);
  auto buffer_presented_callback =
      base::BindRepeating(&SkiaOutputSurfaceImpl::BufferPresented, weak_ptr_);
  auto context_lost_callback =
      base::BindOnce(&SkiaOutputSurfaceImpl::ContextLost, weak_ptr_);

  impl_on_gpu_ = SkiaOutputSurfaceImplOnGpu::Create(
      dependency_, renderer_settings_, gpu_task_scheduler_->GetSequenceId(),
      display_compositor_controller_->controller_on_gpu(),
      std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(vsync_callback_runner));
  if (!impl_on_gpu_) {
    *result = false;
  } else {
    capabilities_ = impl_on_gpu_->capabilities();
    is_displayed_as_overlay_ = impl_on_gpu_->IsDisplayedAsOverlay();
    gr_context_thread_safe_ = impl_on_gpu_->GetGrContextThreadSafeProxy();
    *result = true;
  }
}

SkSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateSkSurfaceCharacterization(
    const gfx::Size& surface_size,
    SkColorType color_type,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    bool is_root_render_pass,
    bool is_overlay) {
  if (!gr_context_thread_safe_) {
    DLOG(ERROR) << "gr_context_thread_safe_ is null.";
    return SkSurfaceCharacterization();
  }

  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
  if (is_root_render_pass) {
    DCHECK(!is_overlay);
    int sample_count = std::min(
        sample_count_,
        gr_context_thread_safe_->maxSurfaceSampleCountForColorType(color_type));
    auto backend_format = gr_context_thread_safe_->defaultBackendFormat(
        color_type, GrRenderable::kYes);
#if BUILDFLAG(IS_MAC)
    DCHECK_EQ(dependency_->gr_context_type(), gpu::GrContextType::kGL);
    // For root rander pass, IOSurface will be used, and we may need using
    // GL_TEXTURE_RECTANGLE_ARB as texture target.
    backend_format =
        GrBackendFormat::MakeGL(backend_format.asGLFormatEnum(),
                                gpu::GetPlatformSpecificTextureTarget());
#endif
    DCHECK(backend_format.isValid())
        << "GrBackendFormat is invalid for color_type: " << color_type;
    auto surface_origin =
        capabilities_.output_surface_origin == gfx::SurfaceOrigin::kBottomLeft
            ? kBottomLeft_GrSurfaceOrigin
            : kTopLeft_GrSurfaceOrigin;
    auto image_info = SkImageInfo::Make(
        surface_size.width(), surface_size.height(), color_type,
        kPremul_SkAlphaType, std::move(color_space));
    DCHECK((capabilities_.uses_default_gl_framebuffer &&
            dependency_->gr_context_type() == gpu::GrContextType::kGL) ||
           !capabilities_.uses_default_gl_framebuffer);
    // Skia doesn't support set desired MSAA count for default gl framebuffer.
    if (capabilities_.uses_default_gl_framebuffer)
      sample_count = 1;
    bool is_textureable =
        !capabilities_.uses_default_gl_framebuffer &&
        !capabilities_.root_is_vulkan_secondary_command_buffer;
    auto characterization = gr_context_thread_safe_->createCharacterization(
        cache_max_resource_bytes, image_info, backend_format, sample_count,
        surface_origin, surface_props, mipmap,
        capabilities_.uses_default_gl_framebuffer, is_textureable,
        GrProtected::kNo, /*vkRTSupportsInputAttachment=*/false,
        capabilities_.root_is_vulkan_secondary_command_buffer);
#if BUILDFLAG(ENABLE_VULKAN)
    VkFormat vk_format = VK_FORMAT_UNDEFINED;
#endif
    LOG_IF(DFATAL, !characterization.isValid())
        << "\n  surface_size=" << surface_size.ToString()
        << "\n  format=" << static_cast<int>(color_type)
        << "\n  color_type=" << static_cast<int>(color_type)
        << "\n  backend_format.isValid()=" << backend_format.isValid()
        << "\n  backend_format.backend()="
        << static_cast<int>(backend_format.backend())
        << "\n  backend_format.asGLFormat()="
        << static_cast<int>(backend_format.asGLFormat())
#if BUILDFLAG(ENABLE_VULKAN)
        << "\n  backend_format.asVkFormat()="
        << static_cast<int>(backend_format.asVkFormat(&vk_format))
        << "\n  backend_format.asVkFormat() vk_format="
        << static_cast<int>(vk_format)
#endif
        << "\n  sample_count=" << sample_count
        << "\n  surface_origin=" << static_cast<int>(surface_origin)
        << "\n  willGlFBO0=" << capabilities_.uses_default_gl_framebuffer;
    return characterization;
  }

  const int sample_count = std::min(
      sample_count_,
      gr_context_thread_safe_->maxSurfaceSampleCountForColorType(color_type));
  auto backend_format = gr_context_thread_safe_->defaultBackendFormat(
      color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid());
#if BUILDFLAG(IS_MAC)
  if (is_overlay) {
    DCHECK_EQ(dependency_->gr_context_type(), gpu::GrContextType::kGL);
    // For overlay, IOSurface will be used, and we may need using
    // GL_TEXTURE_RECTANGLE_ARB as texture target.
    backend_format =
        GrBackendFormat::MakeGL(backend_format.asGLFormatEnum(),
                                gpu::GetPlatformSpecificTextureTarget());
  }
#endif
  auto image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        kPremul_SkAlphaType, std::move(color_space));

  auto characterization = gr_context_thread_safe_->createCharacterization(
      cache_max_resource_bytes, image_info, backend_format, sample_count,
      kTopLeft_GrSurfaceOrigin, surface_props, mipmap,
      /*willUseGLFBO0=*/false, /*isTextureable=*/true, GrProtected::kNo);
  DCHECK(characterization.isValid());
  return characterization;
}

void SkiaOutputSurfaceImpl::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params,
    const gfx::Size& pixel_size,
    gfx::GpuFenceHandle release_fence) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  last_swapped_mailbox_ = params.primary_plane_mailbox;

  if (params.swap_response.result ==
      gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    client_->SetNeedsRedrawRect(gfx::Rect(size_));
    if (frame_buffer_damage_tracker_)
      frame_buffer_damage_tracker_->FrameBuffersChanged(size_);
  }

  if (use_damage_area_from_skia_output_device_) {
    damage_of_current_buffer_ = params.frame_buffer_damage_area;
    DCHECK(damage_of_current_buffer_);
  }

  // texture_in_use_responses is used for GLRenderer only.
  DCHECK(params.texture_in_use_responses.empty());

  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  client_->DidReceiveSwapBuffersAck(params.swap_response.timings,
                                    std::move(release_fence));
  if (!params.released_overlays.empty())
    client_->DidReceiveReleasedOverlays(params.released_overlays);
  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void SkiaOutputSurfaceImpl::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  client_->DidReceivePresentationFeedback(feedback);
  if (update_vsync_parameters_callback_ &&
      (feedback.flags & gfx::PresentationFeedback::kVSync ||
       refresh_interval_ != feedback.interval)) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    update_vsync_parameters_callback_.Run(
        feedback.timestamp, feedback.interval.is_zero()
                                ? BeginFrameArgs::DefaultInterval()
                                : feedback.interval);
    // Update |refresh_interval_|, so we only update when interval is changed.
    refresh_interval_ = feedback.interval;
  }
}

void SkiaOutputSurfaceImpl::OnGpuVSync(base::TimeTicks timebase,
                                       base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (gpu_vsync_callback_)
    gpu_vsync_callback_.Run(timebase, interval);
}

void SkiaOutputSurfaceImpl::ScheduleGpuTaskForTesting(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  EnqueueGpuTask(std::move(callback), std::move(sync_tokens),
                 /*make_current=*/false, /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
}

void SkiaOutputSurfaceImpl::EnqueueGpuTask(
    GpuTask task,
    std::vector<gpu::SyncToken> sync_tokens,
    bool make_current,
    bool need_framebuffer) {
  gpu_tasks_.push_back(std::move(task));
  std::move(sync_tokens.begin(), sync_tokens.end(),
            std::back_inserter(gpu_task_sync_tokens_));

  // Set |make_current_|, so MakeCurrent() will be called before executing all
  // enqueued GPU tasks.
  make_current_ |= make_current;
  need_framebuffer_ |= need_framebuffer;
}

void SkiaOutputSurfaceImpl::FlushGpuTasks(SyncMode sync_mode) {
  TRACE_EVENT1("viz", "SkiaOutputSurfaceImpl::FlushGpuTasks", "sync_mode",
               sync_mode);
  // If |wait_for_finish| is true, a GPU task will be always scheduled to make
  // sure all pending tasks are finished on the GPU thread.
  if (gpu_tasks_.empty() && sync_mode == SyncMode::kNoWait)
    return;

  auto event = sync_mode != SyncMode::kNoWait
                   ? std::make_unique<base::WaitableEvent>()
                   : nullptr;

  base::TimeTicks post_task_timestamp;
  if (should_measure_next_post_task_) {
    post_task_timestamp = base::TimeTicks::Now();
  }

  auto callback = base::BindOnce(
      [](std::vector<GpuTask> tasks, SyncMode sync_mode,
         base::WaitableEvent* event, SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
         bool make_current, bool need_framebuffer,
         base::TimeTicks post_task_timestamp) {
        if (sync_mode == SyncMode::kWaitForTasksStarted)
          event->Signal();
        gpu::ContextUrl::SetActiveUrl(GetActiveUrl());
        // impl_on_gpu can be null during destruction.
        if (impl_on_gpu) {
          if (!post_task_timestamp.is_null())
            impl_on_gpu->SetDrawTimings(post_task_timestamp);
          // MakeCurrent() will mark context lost in SkiaOutputSurfaceImplOnGpu,
          // if it fails.
          if (make_current)
            impl_on_gpu->MakeCurrent(need_framebuffer);
        }
        // Each task can check SkiaOutputSurfaceImplOnGpu::contest_is_lost_
        // to detect errors.
        for (auto& task : tasks) {
          std::move(task).Run();
        }

        if (sync_mode == SyncMode::kWaitForTasksFinished)
          event->Signal();
      },
      std::move(gpu_tasks_), sync_mode, event.get(), impl_on_gpu_.get(),
      make_current_, need_framebuffer_, post_task_timestamp);

  gpu::GpuTaskSchedulerHelper::ReportingCallback reporting_callback;
  if (should_measure_next_post_task_) {
    // Note that the usage of base::Unretained() with the impl_on_gpu_ is
    // considered safe as it is also owned by |callback| and share the same
    // lifetime.
    reporting_callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::SetDependenciesResolvedTimings,
        base::Unretained(impl_on_gpu_.get()));
  }

  gpu_task_scheduler_->ScheduleGpuTask(std::move(callback),
                                       std::move(gpu_task_sync_tokens_),
                                       std::move(reporting_callback));

  make_current_ = false;
  need_framebuffer_ = false;
  should_measure_next_post_task_ = false;
  gpu_task_sync_tokens_.clear();
  gpu_tasks_.clear();

  if (event)
    event->Wait();
}

GrBackendFormat SkiaOutputSurfaceImpl::GetGrBackendFormatForTexture(
    ResourceFormat resource_format,
    uint32_t gl_texture_target,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info) {
  if (dependency_->IsUsingVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    if (!ycbcr_info) {
      // YCbCr info is required for YUV images.
      DCHECK(resource_format != YVU_420 &&
             resource_format != YUV_420_BIPLANAR && resource_format != P010);
      return GrBackendFormat::MakeVk(ToVkFormat(resource_format));
    }

    // Assume optimal tiling.
    GrVkYcbcrConversionInfo gr_ycbcr_info =
        CreateGrVkYcbcrConversionInfo(dependency_->GetVulkanContextProvider()
                                          ->GetDeviceQueue()
                                          ->GetVulkanPhysicalDevice(),
                                      VK_IMAGE_TILING_OPTIMAL, ycbcr_info);
#if BUILDFLAG(IS_LINUX)
    // Textures that were allocated _on linux_ with ycbcr info came from
    // VaapiVideoDecoder, which exports using DRM format modifiers.
    return GrBackendFormat::MakeVk(gr_ycbcr_info,
                                   /*willUseDRMFormatModifiers=*/true);
#else
    return GrBackendFormat::MakeVk(gr_ycbcr_info);
#endif  // BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(ENABLE_VULKAN)
  } else if (dependency_->IsUsingDawn()) {
#if BUILDFLAG(SKIA_USE_DAWN)
    wgpu::TextureFormat format = ToDawnFormat(resource_format);
    return GrBackendFormat::MakeDawn(format);
#endif
  } else if (dependency_->IsUsingMetal()) {
#if BUILDFLAG(IS_APPLE)
    return GrBackendFormat::MakeMtl(ToMTLPixelFormat(resource_format));
#endif
  } else {
    DCHECK(!ycbcr_info);
    // Convert internal format from GLES2 to platform GL.
    unsigned int texture_storage_format = gpu::GetGrGLBackendTextureFormat(
        impl_on_gpu_->GetFeatureInfo(), resource_format,
        gr_context_thread_safe_);

    return GrBackendFormat::MakeGL(texture_storage_format, gl_texture_target);
  }
  NOTREACHED();
  return GrBackendFormat();
}

uint32_t SkiaOutputSurfaceImpl::GetFramebufferCopyTextureFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return GL_RGB;
}

bool SkiaOutputSurfaceImpl::IsDisplayedAsOverlayPlane() const {
  return is_displayed_as_overlay_;
}

unsigned SkiaOutputSurfaceImpl::GetOverlayTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return 0;
}

gpu::Mailbox SkiaOutputSurfaceImpl::GetOverlayMailbox() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return last_swapped_mailbox_;
}

bool SkiaOutputSurfaceImpl::HasExternalStencilTest() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return false;
}

void SkiaOutputSurfaceImpl::ApplyExternalStencil() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

unsigned SkiaOutputSurfaceImpl::UpdateGpuFence() {
  return 0;
}

void SkiaOutputSurfaceImpl::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}

base::ScopedClosureRunner SkiaOutputSurfaceImpl::GetCacheBackBufferCb() {
  if (!impl_on_gpu_->gl_surface())
    return base::ScopedClosureRunner();
  return dependency_->CacheGLSurface(impl_on_gpu_->gl_surface());
}

gpu::SharedImageInterface* SkiaOutputSurfaceImpl::GetSharedImageInterface() {
  return display_compositor_controller_->shared_image_interface();
}

void SkiaOutputSurfaceImpl::AddContextLostObserver(
    ContextLostObserver* observer) {
  observers_.AddObserver(observer);
}

void SkiaOutputSurfaceImpl::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  observers_.RemoveObserver(observer);
}

gpu::SyncToken SkiaOutputSurfaceImpl::Flush() {
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      impl_on_gpu_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::ReleaseFenceSync,
                     base::Unretained(impl_on_gpu_.get()), sync_fence_release_);
  EnqueueGpuTask(std::move(callback), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
  return sync_token;
}

bool SkiaOutputSurfaceImpl::EnsureMinNumberOfBuffers(int n) {
  DCHECK(capabilities_.supports_dynamic_frame_buffer_allocation);
  DCHECK_GT(n, 0);
  DCHECK_LE(n, capabilities_.number_of_buffers);

  if (cached_number_of_buffers_ >= n)
    return false;

  cached_number_of_buffers_ = n;
  if (frame_buffer_damage_tracker_) {
    frame_buffer_damage_tracker_->FrameBuffersChanged(size_);
  }

  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureMinNumberOfBuffers,
                     base::Unretained(impl_on_gpu_.get()), n);
  EnqueueGpuTask(std::move(task), std::vector<gpu::SyncToken>(),
                 /*make_current=*/true,
                 /*need_framebuffer=*/false);
  FlushGpuTasks(SyncMode::kNoWait);
  return true;
}

void SkiaOutputSurfaceImpl::ContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DLOG(ERROR) << "SkiaOutputSurfaceImpl::ContextLost()";
  gr_context_thread_safe_.reset();
  for (auto& observer : observers_)
    observer.OnContextLost();
}

gfx::Rect SkiaOutputSurfaceImpl::GetCurrentFramebufferDamage() const {
  if (use_damage_area_from_skia_output_device_) {
    DCHECK(damage_of_current_buffer_);
    return *damage_of_current_buffer_;
  }

  if (!frame_buffer_damage_tracker_) {
    return gfx::Rect();
  }

  return frame_buffer_damage_tracker_->GetCurrentFrameBufferDamage();
}

void SkiaOutputSurfaceImpl::SetNeedsMeasureNextDrawLatency() {
  should_measure_next_post_task_ = true;
}

void SkiaOutputSurfaceImpl::PreserveChildSurfaceControls() {
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PreserveChildSurfaceControls,
                     base::Unretained(impl_on_gpu_.get()));
  EnqueueGpuTask(std::move(task), std::vector<gpu::SyncToken>(),
                 /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

void SkiaOutputSurfaceImpl::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  auto task = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::InitDelegatedInkPointRendererReceiver,
      base::Unretained(impl_on_gpu_.get()), std::move(pending_receiver));
  EnqueueGpuTask(std::move(task), {}, /*make_current=*/false,
                 /*need_framebuffer=*/false);
}

}  // namespace viz
