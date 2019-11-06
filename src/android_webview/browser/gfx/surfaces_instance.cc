// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/surfaces_instance.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "android_webview/browser/gfx/aw_render_thread_context_provider.h"
#include "android_webview/browser/gfx/aw_vulkan_context_provider.h"
#include "android_webview/browser/gfx/deferred_gpu_command_service.h"
#include "android_webview/browser/gfx/gpu_service_web_view.h"
#include "android_webview/browser/gfx/parent_output_surface.h"
#include "android_webview/browser/gfx/skia_output_surface_dependency_webview.h"
#include "android_webview/browser/gfx/task_queue_web_view.h"
#include "android_webview/common/aw_switches.h"
#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/init/gl_factory.h"

namespace android_webview {

namespace {
// The client_id used here should not conflict with the client_id generated
// from RenderWidgetHostImpl.
constexpr uint32_t kDefaultClientId = 0u;
SurfacesInstance* g_surfaces_instance = nullptr;

void OnContextLost() {
  NOTREACHED() << "Non owned context lost!";
}

}  // namespace

// static
scoped_refptr<SurfacesInstance> SurfacesInstance::GetOrCreateInstance() {
  if (g_surfaces_instance)
    return base::WrapRefCounted(g_surfaces_instance);
  return base::WrapRefCounted(new SurfacesInstance);
}

SurfacesInstance::SurfacesInstance()
    : frame_sink_id_allocator_(kDefaultClientId),
      frame_sink_id_(AllocateFrameSinkId()) {
  viz::RendererSettings settings;

  // Should be kept in sync with compositor_impl_android.cc.
  settings.allow_antialiasing = false;
  settings.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  settings.should_clear_root_render_pass = false;

  settings.use_skia_renderer = features::IsUsingSkiaRenderer();

  // The SharedBitmapManager is null as we do not support or use software
  // compositing on Android.
  frame_sink_manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
      /*shared_bitmap_manager=*/nullptr);
  parent_local_surface_id_allocator_ =
      std::make_unique<viz::ParentLocalSurfaceIdAllocator>();

  constexpr bool is_root = true;
  constexpr bool needs_sync_points = true;
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, frame_sink_manager_.get(), frame_sink_id_, is_root,
      needs_sync_points);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool enable_vulkan =
      command_line->HasSwitch(switches::kWebViewEnableVulkan);
  const bool enable_shared_image =
      command_line->HasSwitch(switches::kWebViewEnableSharedImage);
  LOG_IF(FATAL, enable_vulkan && !enable_shared_image)
      << "--webview-enable-vulkan only works with shared image "
         "(--webview-enable-shared-image).";
  LOG_IF(FATAL, enable_vulkan && !settings.use_skia_renderer)
      << "--webview-enable-vulkan only works with skia renderer "
         "(--enable-features=UseSkiaRenderer).";

  auto vulkan_context_provider =
      enable_vulkan ? AwVulkanContextProvider::GetOrCreateInstance() : nullptr;
  std::unique_ptr<viz::OutputSurface> output_surface;
  gl_surface_ = base::MakeRefCounted<AwGLSurface>();
  if (settings.use_skia_renderer) {
    // TODO(weiliangc): If we could share only one SkiaOutputSurfaceDependency
    // like CommandBufferTaskExecutor, we could move the |shared_context_state|
    // creation to its constructor.
    if (!shared_context_state_) {
      if (!share_group_) {
        // First create a share group.
        share_group_ = base::MakeRefCounted<gl::GLShareGroup>();
      }
      gpu::GpuDriverBugWorkarounds workarounds(
          GpuServiceWebView::GetInstance()
              ->gpu_feature_info()
              .enabled_gpu_driver_bug_workarounds);
      auto gl_context = gl::init::CreateGLContext(
          share_group_.get(), gl_surface_.get(), gl::GLContextAttribs());
      gl_context->MakeCurrent(gl_surface_.get());
      shared_context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
          share_group_, gl_surface_, std::move(gl_context),
          false /* use_virtualized_gl_contexts */,
          base::BindOnce(&OnContextLost), vulkan_context_provider.get());
      shared_context_state_->InitializeGrContext(workarounds,
                                                 nullptr /* gr_shader_cache */);
    }
    auto skia_dependency = std::make_unique<SkiaOutputSurfaceDependencyWebView>(
        TaskQueueWebView::GetInstance(), GpuServiceWebView::GetInstance(),
        shared_context_state_.get(), gl_surface_.get());
    output_surface = viz::SkiaOutputSurfaceImpl::Create(
        std::move(skia_dependency), settings);
    DCHECK(output_surface);
  } else {
    auto context_provider = AwRenderThreadContextProvider::Create(
        gl_surface_, DeferredGpuCommandService::GetInstance());
    output_surface = std::make_unique<ParentOutputSurface>(
        gl_surface_, std::move(context_provider));
  }

  begin_frame_source_ = std::make_unique<viz::StubBeginFrameSource>();
  auto scheduler = std::make_unique<viz::DisplayScheduler>(
      begin_frame_source_.get(), nullptr /* current_task_runner */,
      output_surface->capabilities().max_frames_pending);
  display_ = std::make_unique<viz::Display>(
      nullptr /* shared_bitmap_manager */, settings, frame_sink_id_,
      std::move(output_surface), std::move(scheduler),
      nullptr /* current_task_runner */);
  display_->Initialize(this, frame_sink_manager_->surface_manager(),
                       enable_shared_image);
  frame_sink_manager_->RegisterBeginFrameSource(begin_frame_source_.get(),
                                                frame_sink_id_);

  display_->SetVisible(true);

  DCHECK(!g_surfaces_instance);
  g_surfaces_instance = this;
}

SurfacesInstance::~SurfacesInstance() {
  DCHECK_EQ(g_surfaces_instance, this);
  frame_sink_manager_->UnregisterBeginFrameSource(begin_frame_source_.get());
  g_surfaces_instance = nullptr;
  display_ = nullptr;
  DCHECK(!shared_context_state_ || shared_context_state_->HasOneRef());
  DCHECK(child_ids_.empty());
}

void SurfacesInstance::DisplayOutputSurfaceLost() {
  // Android WebView does not handle context loss.
  LOG(FATAL) << "Render thread context loss";
}

viz::FrameSinkId SurfacesInstance::AllocateFrameSinkId() {
  return frame_sink_id_allocator_.NextFrameSinkId();
}

viz::FrameSinkManagerImpl* SurfacesInstance::GetFrameSinkManager() {
  return frame_sink_manager_.get();
}

void SurfacesInstance::DrawAndSwap(const gfx::Size& viewport,
                                   const gfx::Rect& clip,
                                   const gfx::Transform& transform,
                                   const gfx::Size& frame_size,
                                   const viz::SurfaceId& child_id,
                                   float device_scale_factor,
                                   const gfx::ColorSpace& color_space) {
  DCHECK(base::Contains(child_ids_, child_id));

  gfx::ColorSpace display_color_space =
      color_space.IsValid() ? color_space : gfx::ColorSpace::CreateSRGB();
  display_->SetColorSpace(display_color_space);

  // Create a frame with a single SurfaceDrawQuad referencing the child
  // Surface and transformed using the given transform.
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(viewport), clip, gfx::Transform());
  render_pass->has_transparent_background = false;

  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->quad_to_target_transform = transform;
  quad_state->quad_layer_rect = gfx::Rect(frame_size);
  quad_state->visible_quad_layer_rect = gfx::Rect(frame_size);
  quad_state->clip_rect = clip;
  quad_state->is_clipped = true;
  quad_state->opacity = 1.f;

  viz::SurfaceDrawQuad* surface_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
  surface_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_rect),
                       gfx::Rect(quad_state->quad_layer_rect),
                       viz::SurfaceRange(base::nullopt, child_id),
                       SK_ColorWHITE, /*stretch_content_to_fill_bounds=*/false,
                       /*ignores_input_event=*/false);
  surface_quad->allow_merge = !BackdropFiltersPreventMerge(child_id);

  viz::CompositorFrame frame;
  // We draw synchronously, so acknowledge a manual BeginFrame.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.render_pass_list.push_back(std::move(render_pass));
  frame.metadata.device_scale_factor = device_scale_factor;
  frame.metadata.referenced_surfaces = GetChildIdsRanges();
  frame.metadata.frame_token = ++next_frame_token_;

  if (!root_id_allocation_.IsValid() || viewport != surface_size_ ||
      device_scale_factor != device_scale_factor_) {
    parent_local_surface_id_allocator_->GenerateId();
    root_id_allocation_ = parent_local_surface_id_allocator_
                              ->GetCurrentLocalSurfaceIdAllocation();
    surface_size_ = viewport;
    device_scale_factor_ = device_scale_factor;
    display_->SetLocalSurfaceId(root_id_allocation_.local_surface_id(),
                                device_scale_factor);
  }
  support_->SubmitCompositorFrame(root_id_allocation_.local_surface_id(),
                                  std::move(frame));

  if (shared_context_state_) {
    // GL state could be changed across frames, so we need reset GrContext.
    shared_context_state_->PessimisticallyResetGrContext();
  }
  gl_surface_->SetSize(viewport);
  display_->Resize(viewport);
  display_->DrawAndSwap();
  // SkiaRenderer generates DidReceiveSwapBuffersAck calls.
  if (!features::IsUsingSkiaRenderer()) {
    // TODO(dlibby): Consider sending real swap timings here for webview (or
    // prove why it is not needed).
    display_->DidReceiveSwapBuffersAck(gfx::SwapTimings());
  }
  gl_surface_->MaybeDidPresent(gfx::PresentationFeedback(
      base::TimeTicks::Now(), base::TimeDelta(), 0 /* flags */));
}

void SurfacesInstance::AddChildId(const viz::SurfaceId& child_id) {
  DCHECK(!base::Contains(child_ids_, child_id));
  child_ids_.push_back(child_id);
  if (root_id_allocation_.IsValid())
    SetSolidColorRootFrame();
}

void SurfacesInstance::RemoveChildId(const viz::SurfaceId& child_id) {
  auto itr = std::find(child_ids_.begin(), child_ids_.end(), child_id);
  DCHECK(itr != child_ids_.end());
  child_ids_.erase(itr);
  if (root_id_allocation_.IsValid())
    SetSolidColorRootFrame();
}

void SurfacesInstance::SetSolidColorRootFrame() {
  DCHECK(!surface_size_.IsEmpty());
  gfx::Rect rect(surface_size_);
  bool is_clipped = false;
  bool are_contents_opaque = true;
  std::unique_ptr<viz::RenderPass> render_pass = viz::RenderPass::Create();
  render_pass->SetNew(1, rect, rect, gfx::Transform());
  viz::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(gfx::Transform(), rect, rect, gfx::RRectF(), rect,
                     is_clipped, are_contents_opaque, 1.f,
                     SkBlendMode::kSrcOver, 0);
  viz::SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  solid_quad->SetNew(quad_state, rect, rect, SK_ColorBLACK, false);
  viz::CompositorFrame frame;
  frame.render_pass_list.push_back(std::move(render_pass));
  // We draw synchronously, so acknowledge a manual BeginFrame.
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.referenced_surfaces = GetChildIdsRanges();
  frame.metadata.device_scale_factor = device_scale_factor_;
  frame.metadata.frame_token = ++next_frame_token_;
  support_->SubmitCompositorFrame(root_id_allocation_.local_surface_id(),
                                  std::move(frame));
}

void SurfacesInstance::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  ReclaimResources(resources);
}

std::vector<viz::SurfaceRange> SurfacesInstance::GetChildIdsRanges() {
  std::vector<viz::SurfaceRange> child_ranges;
  for (const viz::SurfaceId& surface_id : child_ids_)
    child_ranges.emplace_back(surface_id);
  return child_ranges;
}

void SurfacesInstance::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {}

void SurfacesInstance::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  // Root surface should have no resources to return.
  CHECK(resources.empty());
}

void SurfacesInstance::OnBeginFramePausedChanged(bool paused) {}

base::TimeDelta SurfacesInstance::GetPreferredFrameIntervalForFrameSinkId(
    const viz::FrameSinkId& id) {
  return frame_sink_manager_->GetPreferredFrameIntervalForFrameSinkId(id);
}

bool SurfacesInstance::BackdropFiltersPreventMerge(
    const viz::SurfaceId& surface_id) {
  // TODO(ericrk): This function makes the pessemistic assumption that any
  // backdrop filter prevents merging this surface. This is not true in a
  // number of cases:
  //  - SkiaRenderer may handle framebuffer readback in some cases.
  //  - This is not needed if framebuffer format is not floating point.
  //
  //  In the future we should optimize this more and avoid the intermediate
  //  in the cases listed above. crbug.com/996434
  const viz::Surface* surface =
      frame_sink_manager_->surface_manager()->GetSurfaceForId(surface_id);
  const auto& frame = surface->GetActiveFrame();
  base::flat_set<viz::RenderPassId> backdrop_filter_passes;
  for (const auto& render_pass : frame.render_pass_list) {
    if (!render_pass->backdrop_filters.IsEmpty())
      backdrop_filter_passes.insert(render_pass->id);
  }

  if (backdrop_filter_passes.empty())
    return false;

  const auto* root_pass = frame.render_pass_list.back().get();
  for (const auto* quad : root_pass->quad_list) {
    if (quad->material != viz::DrawQuad::Material::kRenderPass)
      continue;
    const auto* pass_quad = viz::RenderPassDrawQuad::MaterialCast(quad);
    if (backdrop_filter_passes.find(pass_quad->render_pass_id) !=
        backdrop_filter_passes.end()) {
      return true;
    }
  }

  return false;
}

}  // namespace android_webview
