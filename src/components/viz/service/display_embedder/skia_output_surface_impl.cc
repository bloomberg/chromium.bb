// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/single_task_sequence.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

namespace viz {

namespace {

sk_sp<SkPromiseImageTexture> Fulfill(void* texture_context) {
  DCHECK(texture_context);
  auto* image_context = static_cast<ImageContextImpl*>(texture_context);
  return sk_ref_sp(image_context->promise_image_texture());
}

void DoNothing(void* texture_context) {}

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
    SkSurfaceCharacterization characterization,
    RenderPassId render_pass_id)
    : render_pass_id_(render_pass_id) {
  recorder_storage_.emplace(characterization);
  recorder_ = &recorder_storage_.value();
}

SkiaOutputSurfaceImpl::ScopedPaint::~ScopedPaint() = default;

// static
std::unique_ptr<SkiaOutputSurface> SkiaOutputSurfaceImpl::Create(
    std::unique_ptr<SkiaOutputSurfaceDependency> deps,
    const RendererSettings& renderer_settings) {
  auto output_surface = std::make_unique<SkiaOutputSurfaceImpl>(
      util::PassKey<SkiaOutputSurfaceImpl>(), std::move(deps),
      renderer_settings);
  if (!output_surface->Initialize())
    output_surface = nullptr;
  return output_surface;
}

SkiaOutputSurfaceImpl::SkiaOutputSurfaceImpl(
    util::PassKey<SkiaOutputSurfaceImpl> /* pass_key */,
    std::unique_ptr<SkiaOutputSurfaceDependency> deps,
    const RendererSettings& renderer_settings)
    : SkiaOutputSurface(GetOutputSurfaceType(deps.get())),
      dependency_(std::move(deps)),
      renderer_settings_(renderer_settings) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  current_paint_.reset();
  root_recorder_.reset();

  if (!render_pass_image_cache_.empty()) {
    std::vector<RenderPassId> render_pass_ids;
    render_pass_ids.reserve(render_pass_ids.size());
    for (auto& entry : render_pass_image_cache_)
      render_pass_ids.push_back(entry.first);
    RemoveRenderPassResource(std::move(render_pass_ids));
  }
  DCHECK(render_pass_image_cache_.empty());

  // Post a task to destroy |impl_on_gpu_| on the GPU thread and block until
  // that is finished.
  base::WaitableEvent event;
  auto task = base::BindOnce(
      [](std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
         base::WaitableEvent* event) {
        impl_on_gpu.reset();
        event->Signal();
      },
      std::move(impl_on_gpu_), &event);
  ScheduleGpuTask(std::move(task), {});
  event.Wait();

  gpu_task_scheduler_.reset();
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
  ignore_result(root_recorder_->getCanvas());
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

  // Reshape will damage all buffers.
  current_buffer_ = 0u;
  for (auto& damage : damage_of_buffers_)
    damage = gfx::Rect(size);

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::Reshape,
                             base::Unretained(impl_on_gpu_.get()), size,
                             device_scale_factor, color_space, format,
                             use_stencil, pre_transform_);
  ScheduleGpuTask(std::move(task), {});

  color_space_ = color_space;
  is_hdr_ = color_space_.IsHDR();
  size_ = size;
  characterization_ = CreateSkSurfaceCharacterization(
      size, GetResourceFormat(format), false /* mipmap */,
      color_space_.ToSkColorSpace(), true /* is_root_render_pass */);
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
  if (capabilities_.supports_pre_transform)
    pre_transform_ = transform;
}

gfx::OverlayTransform SkiaOutputSurfaceImpl::GetDisplayTransform() {
  return pre_transform_;
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!current_paint_);
  DCHECK(root_recorder_);

  current_paint_.emplace(&root_recorder_.value());

  if (!renderer_settings_.show_overdraw_feedback)
    return current_paint_->recorder()->getCanvas();

  DCHECK(!overdraw_surface_recorder_);
  DCHECK(renderer_settings_.show_overdraw_feedback);

  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      gfx::Size(characterization_.width(), characterization_.height()),
      BGRA_8888, false /* mipmap */, characterization_.refColorSpace(),
      false /* is_root_render_pass */);
  overdraw_surface_recorder_.emplace(characterization);
  overdraw_canvas_.emplace((overdraw_surface_recorder_->getCanvas()));

  nway_canvas_.emplace(characterization_.width(), characterization_.height());
  nway_canvas_->addCanvas(current_paint_->recorder()->getCanvas());
  nway_canvas_->addCanvas(&overdraw_canvas_.value());
  return &nway_canvas_.value();
}

void SkiaOutputSurfaceImpl::MakePromiseSkImage(ImageContext* image_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(!image_context->mailbox_holder().mailbox.IsZero());

  images_in_current_paint_.push_back(
      static_cast<ImageContextImpl*>(image_context));

  if (image_context->has_image())
    return;

  SkColorType color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, image_context->resource_format());
  GrBackendFormat backend_format = GetGrBackendFormatForTexture(
      image_context->resource_format(),
      image_context->mailbox_holder().texture_target,
      image_context->ycbcr_info());
  image_context->SetImage(
      current_paint_->recorder()->makePromiseTexture(
          backend_format, image_context->size().width(),
          image_context->size().height(), GrMipMapped::kNo,
          image_context->origin(), color_type, image_context->alpha_type(),
          image_context->color_space(), Fulfill /* fulfillProc */,
          DoNothing /* releaseProc */, DoNothing /* doneProc */,
          image_context /* context */),
      backend_format);

  if (image_context->mailbox_holder().sync_token.HasData()) {
    resource_sync_tokens_.push_back(image_context->mailbox_holder().sync_token);
    image_context->mutable_mailbox_holder()->sync_token.Clear();
  }
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromYUV(
    const std::vector<ImageContext*>& contexts,
    sk_sp<SkColorSpace> image_color_space,
    bool has_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK((has_alpha && (contexts.size() == 3 || contexts.size() == 4)) ||
         (!has_alpha && (contexts.size() == 2 || contexts.size() == 3)));

  SkYUVAIndex indices[4];
  PrepareYUVATextureIndices(contexts, has_alpha, indices);

  GrBackendFormat formats[4] = {};
  SkISize yuva_sizes[4] = {};
  SkDeferredDisplayListRecorder::PromiseImageTextureContext
      texture_contexts[4] = {};
  for (size_t i = 0; i < contexts.size(); ++i) {
    auto* context = static_cast<ImageContextImpl*>(contexts[i]);
    DCHECK(context->origin() == kTopLeft_GrSurfaceOrigin);
    formats[i] = GetGrBackendFormatForTexture(
        context->resource_format(), context->mailbox_holder().texture_target,
        /*ycbcr_info=*/base::nullopt);
    yuva_sizes[i].set(context->size().width(), context->size().height());

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

  // Note: YUV to RGB conversion is handled by a color filter in SkiaRenderer.
  auto image = current_paint_->recorder()->makeYUVAPromiseTexture(
      kIdentity_SkYUVColorSpace, formats, yuva_sizes, indices,
      yuva_sizes[0].width(), yuva_sizes[0].height(), kTopLeft_GrSurfaceOrigin,
      image_color_space, Fulfill, DoNothing, DoNothing, texture_contexts);
  DCHECK(image);
  return image;
}

gpu::SyncToken SkiaOutputSurfaceImpl::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  if (image_contexts.empty())
    return gpu::SyncToken();

  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      impl_on_gpu_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts,
                     base::Unretained(impl_on_gpu_.get()),
                     std::move(image_contexts), sync_fence_release_);
  gpu_task_scheduler_->ScheduleGpuTask(std::move(callback), {});
  return sync_token;
}

std::unique_ptr<ExternalUseClient::ImageContext>
SkiaOutputSurfaceImpl::CreateImageContext(
    const gpu::MailboxHolder& holder,
    const gfx::Size& size,
    ResourceFormat format,
    const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space) {
  return std::make_unique<ImageContextImpl>(holder, size, format, ycbcr_info,
                                            std::move(color_space));
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
  if (!damage_of_buffers_.empty() && current_buffer_modified_) {
    gfx::Rect damage_rect =
        frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(size_);
    // Calculate damage area for every buffer.
    for (size_t i = 0u; i < damage_of_buffers_.size(); ++i) {
      if (i == current_buffer_) {
        damage_of_buffers_[i] = gfx::Rect();
      } else {
        damage_of_buffers_[i].Union(damage_rect);
      }
    }
    // change the current buffer index to the next buffer in the queue.
    if (++current_buffer_ == damage_of_buffers_.size())
      current_buffer_ = 0u;
  }
  current_buffer_modified_ = false;
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffers,
                     base::Unretained(impl_on_gpu_.get()), std::move(frame),
                     std::move(deferred_framebuffer_draw_closure_));
  ScheduleGpuTask(std::move(callback), std::move(resource_sync_tokens_));

  // Recreate |root_recorder_| after SwapBuffers has been scheduled on GPU
  // thread to save some time in BeginPaintCurrentFrame
  // TODO(vasilyt): reuse root recorder
  RecreateRootRecorder();
}

void SkiaOutputSurfaceImpl::SwapBuffersSkipped() {
  if (deferred_framebuffer_draw_closure_) {
    // Run the task to draw the root RenderPass on the GPU thread. If we aren't
    // going to swap buffers and there are no CopyOutputRequests on the root
    // RenderPass we don't strictly need to draw. However, we still need to
    // PostTask to the GPU thread to deal with freeing resources and running
    // callbacks. This is infrequent and all the work is already done in
    // FinishPaintCurrentFrame() so use the same path.
    auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffersSkipped,
                               base::Unretained(impl_on_gpu_.get()),
                               std::move(deferred_framebuffer_draw_closure_));
    ScheduleGpuTask(std::move(task), std::move(resource_sync_tokens_));

    // TODO(vasilyt): reuse root recorder
    RecreateRootRecorder();
  }
}

void SkiaOutputSurfaceImpl::ScheduleOutputSurfaceAsOverlay(
    OverlayProcessorInterface::OutputSurfaceOverlayPlane output_surface_plane) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay,
      base::Unretained(impl_on_gpu_.get()), std::move(output_surface_plane));
  ScheduleGpuTask(std::move(callback), {});
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPass(
    const RenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!current_paint_);
  DCHECK(resource_sync_tokens_.empty());

  SkSurfaceCharacterization c = CreateSkSurfaceCharacterization(
      surface_size, format, mipmap, std::move(color_space),
      false /* is_root_render_pass */);
  current_paint_.emplace(c, id);
  return current_paint_->recorder()->getCanvas();
}

gpu::SyncToken SkiaOutputSurfaceImpl::SubmitPaint(
    base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(!deferred_framebuffer_draw_closure_);
  // If current_render_pass_id_ is not 0, we are painting a render pass.
  // Otherwise we are painting a frame.

  bool painting_render_pass = current_paint_->render_pass_id() != 0;

  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      impl_on_gpu_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();

  auto ddl = current_paint_->recorder()->detach();
  DCHECK(ddl);

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
    DCHECK(!on_finished);
    auto closure = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass,
        base::Unretained(impl_on_gpu_.get()), current_paint_->render_pass_id(),
        std::move(ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, sync_fence_release_);
    ScheduleGpuTask(std::move(closure), std::move(resource_sync_tokens_));
  } else {
    // Draw on the root render pass.
    current_buffer_modified_ = true;
    std::unique_ptr<SkDeferredDisplayList> overdraw_ddl;
    if (renderer_settings_.show_overdraw_feedback) {
      overdraw_ddl = overdraw_surface_recorder_->detach();
      DCHECK(overdraw_ddl);
      overdraw_canvas_.reset();
      nway_canvas_.reset();
      overdraw_surface_recorder_.reset();
    }

    deferred_framebuffer_draw_closure_ = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame,
        base::Unretained(impl_on_gpu_.get()), std::move(ddl),
        std::move(overdraw_ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, sync_fence_release_, std::move(on_finished),
        draw_rectangle_);
    draw_rectangle_.reset();
  }
  images_in_current_paint_.clear();
  current_paint_.reset();
  return sync_token;
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);

  auto& image_context = render_pass_image_cache_[id];
  if (!image_context) {
    image_context = std::make_unique<ImageContextImpl>(id, size, format, mipmap,
                                                       std::move(color_space));
  }
  if (!image_context->has_image()) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    GrBackendFormat backend_format = GetGrBackendFormatForTexture(
        format, GL_TEXTURE_2D, /*ycbcr_info=*/base::nullopt);
    image_context->SetImage(
        current_paint_->recorder()->makePromiseTexture(
            backend_format, image_context->size().width(),
            image_context->size().height(), image_context->mipmap(),
            image_context->origin(), color_type, image_context->alpha_type(),
            image_context->color_space(), Fulfill, DoNothing, DoNothing,
            image_context.get()),
        backend_format);
    DCHECK(image_context->has_image());
  }
  images_in_current_paint_.push_back(image_context.get());
  return image_context->image();
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  std::vector<std::unique_ptr<ImageContextImpl>> image_contexts;
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

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource,
                     base::Unretained(impl_on_gpu_.get()), std::move(ids),
                     std::move(image_contexts));
  ScheduleGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!request->has_result_task_runner())
    request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());

  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CopyOutput,
                                 base::Unretained(impl_on_gpu_.get()), id,
                                 geometry, color_space, std::move(request),
                                 std::move(deferred_framebuffer_draw_closure_));
  ScheduleGpuTask(std::move(callback), std::move(resource_sync_tokens_));
}

void SkiaOutputSurfaceImpl::ScheduleOverlays(
    OverlayList overlays,
    std::vector<gpu::SyncToken> sync_tokens) {
  auto task =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::ScheduleOverlays,
                     base::Unretained(impl_on_gpu_.get()), std::move(overlays));
  ScheduleGpuTask(std::move(task), std::move(sync_tokens));
}

#if defined(OS_WIN)
void SkiaOutputSurfaceImpl::SetEnableDCLayers(bool enable) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetEnableDCLayers,
                             base::Unretained(impl_on_gpu_.get()), enable);
  ScheduleGpuTask(std::move(task), {});
}
#endif

gpu::MemoryTracker* SkiaOutputSurfaceImpl::GetMemoryTracker() {
  // Should only be called after initialization.
  DCHECK(impl_on_gpu_);
  return impl_on_gpu_->GetMemoryTracker();
}

void SkiaOutputSurfaceImpl::SetFrameRate(float frame_rate) {
  auto task = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetFrameRate,
                             base::Unretained(impl_on_gpu_.get()), frame_rate);
  ScheduleGpuTask(std::move(task), {});
}

void SkiaOutputSurfaceImpl::SetCapabilitiesForTesting(
    gfx::SurfaceOrigin output_surface_origin) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(impl_on_gpu_);
  capabilities_.output_surface_origin = output_surface_origin;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting,
                     base::Unretained(impl_on_gpu_.get()), capabilities_);
  ScheduleGpuTask(std::move(callback), {});
}

bool SkiaOutputSurfaceImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Before starting to schedule GPU task, set up |gpu_task_scheduler_| that
  // holds a task sequence.
  gpu_task_scheduler_ = base::MakeRefCounted<gpu::GpuTaskSchedulerHelper>(
      dependency_->CreateSequence());

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();

  // This runner could be called from vsync or GPU thread after |this| is
  // destroyed. We post directly to display compositor thread to check
  // |weak_ptr_| as |dependency_| may have been destroyed.
  GpuVSyncCallback vsync_callback_runner =
#if defined(OS_ANDROID)
      // Callback is never used on Android. Doesn't work with WebView because
      // calling it bypasses SkiaOutputSurfaceDependency.
      base::DoNothing();
#else
      base::BindRepeating(
          [](scoped_refptr<base::SingleThreadTaskRunner> runner,
             base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr,
             base::TimeTicks timebase, base::TimeDelta interval) {
            runner->PostTask(FROM_HERE,
                             base::BindOnce(&SkiaOutputSurfaceImpl::OnGpuVSync,
                                            weak_ptr, timebase, interval));
          },
          base::ThreadTaskRunnerHandle::Get(), weak_ptr_);
#endif

  base::WaitableEvent event;
  bool result = false;
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                                 base::Unretained(this), vsync_callback_runner,
                                 &event, &result);
  ScheduleGpuTask(std::move(callback), {});
  event.Wait();

  if (capabilities_.preserve_buffer_content &&
      capabilities_.supports_post_sub_buffer) {
    capabilities_.only_invalidates_damage_rect = false;
    damage_of_buffers_.resize(capabilities_.max_frames_pending + 1);
  }

  return result;
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(
    GpuVSyncCallback vsync_callback_runner,
    base::WaitableEvent* event,
    bool* result) {
  base::Optional<base::ScopedClosureRunner> scoped_runner;
  if (event) {
    scoped_runner.emplace(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  auto did_swap_buffer_complete_callback = base::BindRepeating(
      &SkiaOutputSurfaceImpl::DidSwapBuffersComplete, weak_ptr_);
  auto buffer_presented_callback =
      base::BindRepeating(&SkiaOutputSurfaceImpl::BufferPresented, weak_ptr_);
  auto context_lost_callback =
      base::BindOnce(&SkiaOutputSurfaceImpl::ContextLost, weak_ptr_);

  impl_on_gpu_ = SkiaOutputSurfaceImplOnGpu::Create(
      dependency_.get(), renderer_settings_,
      gpu_task_scheduler_->GetSequenceId(),
      std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(vsync_callback_runner));
  if (!impl_on_gpu_) {
    *result = false;
  } else {
    capabilities_ = impl_on_gpu_->capabilities();
    is_displayed_as_overlay_ = impl_on_gpu_->IsDisplayedAsOverlay();
    *result = true;
  }
}

SkSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateSkSurfaceCharacterization(
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    bool is_root_render_pass) {
  auto gr_context_thread_safe = impl_on_gpu_->GetGrContextThreadSafeProxy();
  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(0 /*flags */,
                               SkSurfaceProps::kLegacyFontHost_InitType);
  if (is_root_render_pass) {
    auto color_type =
        is_hdr_ && capabilities_.sk_color_type_for_hdr != kUnknown_SkColorType
            ? capabilities_.sk_color_type_for_hdr
            : capabilities_.sk_color_type;

    const auto& backend_format =
        is_hdr_ && capabilities_.gr_backend_format_for_hdr.isValid()
            ? capabilities_.gr_backend_format_for_hdr
            : capabilities_.gr_backend_format;
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
    auto characterization = gr_context_thread_safe->createCharacterization(
        cache_max_resource_bytes, image_info, backend_format,
        0 /* sampleCount */, surface_origin, surface_props, mipmap,
        capabilities_.uses_default_gl_framebuffer, false /* isTextureable */,
        impl_on_gpu_->GetGpuPreferences().enforce_vulkan_protected_memory
            ? GrProtected::kYes
            : GrProtected::kNo);
    DCHECK(characterization.isValid());
    return characterization;
  }

  auto color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
  auto backend_format = gr_context_thread_safe->defaultBackendFormat(
      color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid());
  auto image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        kPremul_SkAlphaType, std::move(color_space));

  auto characterization = gr_context_thread_safe->createCharacterization(
      cache_max_resource_bytes, image_info, backend_format, 0 /* sampleCount */,
      kTopLeft_GrSurfaceOrigin, surface_props, mipmap,
      false /* willUseGLFBO0 */, true /* isTextureable */,
      impl_on_gpu_->GetGpuPreferences().enforce_vulkan_protected_memory
          ? GrProtected::kYes
          : GrProtected::kNo);
  DCHECK(characterization.isValid());
  return characterization;
}

void SkiaOutputSurfaceImpl::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params,
    const gfx::Size& pixel_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);

  // Reset |damage_of_buffers_|, if buffers are new created.
  if (params.swap_response.result ==
      gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    for (auto& damage : damage_of_buffers_)
      damage = gfx::Rect(size_);
  }

  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  client_->DidReceiveSwapBuffersAck(params.swap_response.timings);
  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void SkiaOutputSurfaceImpl::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  client_->DidReceivePresentationFeedback(feedback);
  if (update_vsync_parameters_callback_ &&
      feedback.flags & gfx::PresentationFeedback::kVSync) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    update_vsync_parameters_callback_.Run(
        feedback.timestamp, feedback.interval.is_zero()
                                ? BeginFrameArgs::DefaultInterval()
                                : feedback.interval);
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
  ScheduleGpuTask(std::move(callback), std::move(sync_tokens));
}

void SkiaOutputSurfaceImpl::ScheduleGpuTask(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  auto wrapped_closure = base::BindOnce(
      [](base::OnceClosure callback) {
        gpu::ContextUrl::SetActiveUrl(GetActiveUrl());
        std::move(callback).Run();
      },
      std::move(callback));
  gpu_task_scheduler_->ScheduleGpuTask(std::move(wrapped_closure),
                                       std::move(sync_tokens));
}

GrBackendFormat SkiaOutputSurfaceImpl::GetGrBackendFormatForTexture(
    ResourceFormat resource_format,
    uint32_t gl_texture_target,
    const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info) {
  if (dependency_->IsUsingVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    if (!ycbcr_info) {
      // YCbCr info is required for YUV images.
      DCHECK(resource_format != YVU_420 && resource_format != YUV_420_BIPLANAR);
      return GrBackendFormat::MakeVk(ToVkFormat(resource_format));
    }

    // Assume optimal tiling.
    GrVkYcbcrConversionInfo gr_ycbcr_info =
        CreateGrVkYcbcrConversionInfo(dependency_->GetVulkanContextProvider()
                                          ->GetDeviceQueue()
                                          ->GetVulkanPhysicalDevice(),
                                      VK_IMAGE_TILING_OPTIMAL, ycbcr_info);
    return GrBackendFormat::MakeVk(gr_ycbcr_info);
#endif
  } else if (dependency_->IsUsingDawn()) {
#if BUILDFLAG(SKIA_USE_DAWN)
    wgpu::TextureFormat format = ToDawnFormat(resource_format);
    return GrBackendFormat::MakeDawn(format);
#endif
  } else {
    DCHECK(!ycbcr_info);
    // Convert internal format from GLES2 to platform GL.
    unsigned int texture_storage_format = gpu::GetGrGLBackendTextureFormat(
        impl_on_gpu_->GetFeatureInfo(), resource_format);

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

void SkiaOutputSurfaceImpl::AddContextLostObserver(
    ContextLostObserver* observer) {
  observers_.AddObserver(observer);
}

void SkiaOutputSurfaceImpl::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SkiaOutputSurfaceImpl::PrepareYUVATextureIndices(
    const std::vector<ImageContext*>& contexts,
    bool has_alpha,
    SkYUVAIndex indices[4]) {
  DCHECK((has_alpha && (contexts.size() == 3 || contexts.size() == 4)) ||
         (!has_alpha && (contexts.size() == 2 || contexts.size() == 3)));

  bool uv_interleaved = has_alpha ? contexts.size() == 3 : contexts.size() == 2;

  indices[SkYUVAIndex::kY_Index].fIndex = 0;
  indices[SkYUVAIndex::kY_Index].fChannel = SkColorChannel::kR;

  if (uv_interleaved) {
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 1;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kG;

    indices[SkYUVAIndex::kA_Index].fIndex = has_alpha ? 2 : -1;
    indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
  } else {
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 2;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kA_Index].fIndex = has_alpha ? 3 : -1;
    indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
  }
}

void SkiaOutputSurfaceImpl::ContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_)
    observer.OnContextLost();
}

scoped_refptr<gpu::GpuTaskSchedulerHelper>
SkiaOutputSurfaceImpl::GetGpuTaskSchedulerHelper() {
  return gpu_task_scheduler_;
}

gfx::Rect SkiaOutputSurfaceImpl::GetCurrentFramebufferDamage() const {
  if (damage_of_buffers_.empty())
    return gfx::Rect();

  DCHECK_LT(current_buffer_, damage_of_buffers_.size());
  return damage_of_buffers_[current_buffer_];
}

}  // namespace viz
