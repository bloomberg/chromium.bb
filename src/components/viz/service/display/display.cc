// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <stddef.h>
#include <limits>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/region.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/display/damage_frame_annotator.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/scheduler_sequence.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

#if defined(OS_ANDROID)
#include "ui/gl/android/android_surface_control_compat.h"
#endif
namespace viz {

namespace {

const DrawQuad::Material kNonSplittableMaterials[] = {
    // Exclude debug quads from quad splitting
    DrawQuad::Material::kDebugBorder,
    // Exclude possible overlay candidates from quad splitting
    // See OverlayCandidate::FromDrawQuad
    DrawQuad::Material::kStreamVideoContent,
    DrawQuad::Material::kTextureContent,
    DrawQuad::Material::kVideoHole,
    // See DCLayerOverlayProcessor::ProcessRenderPass
    DrawQuad::Material::kYuvVideoContent,
};

constexpr base::TimeDelta kAllowedDeltaFromFuture =
    base::TimeDelta::FromMilliseconds(16);

// Assign each Display instance a starting value for the the display-trace id,
// so that multiple Displays all don't start at 0, because that makes it
// difficult to associate the trace-events with the particular displays.
int64_t GetStartingTraceId() {
  static int64_t client = 0;
  // https://crbug.com/956695
  return ((++client & 0xffff) << 16);
}

gfx::PresentationFeedback SanitizePresentationFeedback(
    const gfx::PresentationFeedback& feedback,
    base::TimeTicks draw_time) {
  // Temporary to investigate large presentation times.
  // https://crbug.com/894440
  DCHECK(!draw_time.is_null());
  if (feedback.timestamp.is_null())
    return feedback;

  // If the presentation-timestamp is from the future, or from the past (i.e.
  // before swap-time), then invalidate the feedback. Also report how far into
  // the future (or from the past) the timestamps are.
  // https://crbug.com/894440
  const auto now = base::TimeTicks::Now();
  // The timestamp for the presentation feedback may have a different source and
  // therefore the timestamp can be slightly in the future in comparison with
  // base::TimeTicks::Now(). Such presentation feedbacks should not be rejected.
  // See https://crbug.com/1040178
  const auto allowed_delta_from_future =
      ((feedback.flags & gfx::PresentationFeedback::kHWClock) != 0)
          ? kAllowedDeltaFromFuture
          : base::TimeDelta();
  if (feedback.timestamp > now + allowed_delta_from_future) {
    const auto diff = feedback.timestamp - now;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Graphics.PresentationTimestamp.InvalidFromFuture", diff);
    return gfx::PresentationFeedback::Failure();
  }

  if (feedback.timestamp < draw_time) {
    const auto diff = draw_time - feedback.timestamp;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Graphics.PresentationTimestamp.InvalidBeforeSwap", diff);
    return gfx::PresentationFeedback::Failure();
  }

  const auto difference = feedback.timestamp - draw_time;
  if (difference.InMinutes() > 3) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Graphics.PresentationTimestamp.LargePresentationDelta", difference,
        base::TimeDelta::FromMinutes(3), base::TimeDelta::FromHours(1), 50);
  }
  return feedback;
}

// Returns the bounds for the largest rect that can be inscribed in a rounded
// rect.
gfx::RectF GetOccludingRectForRRectF(const gfx::RRectF& bounds) {
  if (bounds.IsEmpty())
    return gfx::RectF();
  if (bounds.GetType() == gfx::RRectF::Type::kRect)
    return bounds.rect();
  gfx::RectF occluding_rect = bounds.rect();

  // Compute the radius for each corner
  float top_left = bounds.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x();
  float top_right = bounds.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x();
  float lower_right =
      bounds.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x();
  float lower_left = bounds.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x();

  // Get a bounding rect that does not intersect with the rounding clip.
  // When a rect has rounded corner with radius r, then the largest rect that
  // can be inscribed inside it has an inset of |((2 - sqrt(2)) / 2) * radius|.
  occluding_rect.Inset(std::max(top_left, lower_left) * 0.3f,
                       std::max(top_left, top_right) * 0.3f,
                       std::max(top_right, lower_right) * 0.3f,
                       std::max(lower_right, lower_left) * 0.3f);
  return occluding_rect;
}

// SkRegion uses INT_MAX as a sentinel. Reduce gfx::Rect values when they are
// equal to INT_MAX to prevent conversion to an empty region.
gfx::Rect SafeConvertRectForRegion(const gfx::Rect& r) {
  gfx::Rect safe_rect(r);
  if (safe_rect.x() == INT_MAX)
    safe_rect.set_x(INT_MAX - 1);
  if (safe_rect.y() == INT_MAX)
    safe_rect.set_y(INT_MAX - 1);
  if (safe_rect.width() == INT_MAX)
    safe_rect.set_width(INT_MAX - 1);
  if (safe_rect.height() == INT_MAX)
    safe_rect.set_height(INT_MAX - 1);
  return safe_rect;
}

// Computes the accumulated area of all the rectangles in the list of |rects|.
int ComputeArea(const std::vector<gfx::Rect>& rects) {
  int area = 0;
  for (const auto& r : rects)
    area += r.size().GetArea();
  return area;
}

// Decides whether or not a DrawQuad should be split into a more complex visible
// region in order to avoid overdraw.
bool CanSplitQuad(const DrawQuad::Material m,
                  const int visible_region_area,
                  const int visible_region_bounding_area,
                  const int minimum_fragments_reduced,
                  const float device_scale_factor) {
  return !base::Contains(kNonSplittableMaterials, m) &&
         (visible_region_bounding_area - visible_region_area) *
                 device_scale_factor * device_scale_factor >
             minimum_fragments_reduced;
}

// Attempts to consolidate rectangles that were only split because of the
// nature of base::Region and transforms the region into a list of visible
// rectangles. Returns true upon successful reduction of the region to under
// |complexity_limit|, false otherwise.
bool ReduceComplexity(const cc::Region& region,
                      size_t complexity_limit,
                      std::vector<gfx::Rect>* reduced_region) {
  DCHECK(reduced_region);

  reduced_region->clear();
  for (const gfx::Rect& r : region) {
    auto it =
        std::find_if(reduced_region->begin(), reduced_region->end(),
                     [&r](const gfx::Rect& a) { return a.SharesEdgeWith(r); });
    if (it != reduced_region->end()) {
      it->Union(r);
      continue;
    }
    reduced_region->push_back(r);

    if (reduced_region->size() >= complexity_limit)
      return false;
  }
  return true;
}

bool SupportsSetFrameRate(const OutputSurface* output_surface) {
#if defined(OS_ANDROID)
  return output_surface->capabilities().supports_surfaceless &&
         gl::SurfaceControl::SupportsSetFrameRate();
#endif
  return false;
}

}  // namespace

constexpr base::TimeDelta Display::kDrawToSwapMin;
constexpr base::TimeDelta Display::kDrawToSwapMax;

Display::PresentationGroupTiming::PresentationGroupTiming() = default;

Display::PresentationGroupTiming::PresentationGroupTiming(
    Display::PresentationGroupTiming&& other) = default;

Display::PresentationGroupTiming::~PresentationGroupTiming() = default;

void Display::PresentationGroupTiming::AddPresentationHelper(
    std::unique_ptr<Surface::PresentationHelper> helper) {
  presentation_helpers_.push_back(std::move(helper));
}

void Display::PresentationGroupTiming::OnDraw(
    base::TimeTicks draw_start_timestamp) {
  draw_start_timestamp_ = draw_start_timestamp;
}

void Display::PresentationGroupTiming::OnSwap(gfx::SwapTimings timings) {
  swap_timings_ = timings;
}

void Display::PresentationGroupTiming::OnPresent(
    const gfx::PresentationFeedback& feedback) {
  for (auto& presentation_helper : presentation_helpers_) {
    presentation_helper->DidPresent(draw_start_timestamp_, swap_timings_,
                                    feedback);
  }
}

Display::Display(
    SharedBitmapManager* bitmap_manager,
    const RendererSettings& settings,
    const FrameSinkId& frame_sink_id,
    std::unique_ptr<OutputSurface> output_surface,
    std::unique_ptr<OverlayProcessorInterface> overlay_processor,
    std::unique_ptr<DisplaySchedulerBase> scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
    : bitmap_manager_(bitmap_manager),
      settings_(settings),
      frame_sink_id_(frame_sink_id),
      output_surface_(std::move(output_surface)),
      skia_output_surface_(output_surface_->AsSkiaOutputSurface()),
      scheduler_(std::move(scheduler)),
      current_task_runner_(std::move(current_task_runner)),
      overlay_processor_(std::move(overlay_processor)),
      swapped_trace_id_(GetStartingTraceId()),
      last_swap_ack_trace_id_(swapped_trace_id_),
      last_presented_trace_id_(swapped_trace_id_) {
  DCHECK(output_surface_);
  DCHECK(frame_sink_id_.is_valid());
  if (scheduler_)
    scheduler_->SetClient(this);
  enable_quad_splitting_ = features::ShouldSplitPartiallyOccludedQuads() &&
                           !overlay_processor_->DisableSplittingQuads();
}

Display::~Display() {
#if DCHECK_IS_ON()
  allow_schedule_gpu_task_during_destruction_.reset(
      new gpu::ScopedAllowScheduleGpuTask);
#endif
  if (resource_provider_) {
    resource_provider_->SetAllowAccessToGPUThread(true);
  }
#if defined(OS_ANDROID)
  // In certain cases, drivers hang when tearing down the display. Finishing
  // before teardown appears to address this. As we're during display teardown,
  // an additional finish should have minimal impact.
  // TODO(ericrk): Add a more robust workaround. crbug.com/899705
  if (auto* context = output_surface_->context_provider()) {
    context->ContextGL()->Finish();
  }
#endif

  if (no_pending_swaps_callback_)
    std::move(no_pending_swaps_callback_).Run();

  for (auto& observer : observers_)
    observer.OnDisplayDestroyed();
  observers_.Clear();

  // Send gfx::PresentationFeedback::Failure() to any surfaces expecting
  // feedback.
  pending_presentation_group_timings_.clear();

  // Only do this if Initialize() happened.
  if (client_) {
    if (auto* context = output_surface_->context_provider())
      context->RemoveObserver(this);
    if (skia_output_surface_)
      skia_output_surface_->RemoveContextLostObserver(this);
  }

  // Un-register as DisplaySchedulerClient to prevent us from being called in a
  // partially destructed state.
  if (scheduler_)
    scheduler_->SetClient(nullptr);

  if (damage_tracker_)
    damage_tracker_->RunDrawCallbacks();
}

void Display::Initialize(DisplayClient* client,
                         SurfaceManager* surface_manager,
                         bool enable_shared_images,
                         bool hw_support_for_multiple_refresh_rates,
                         size_t num_of_frames_to_toggle_interval) {
  DCHECK(client);
  DCHECK(surface_manager);
  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
  client_ = client;
  surface_manager_ = surface_manager;

  output_surface_->BindToClient(this);
  if (output_surface_->software_device())
    output_surface_->software_device()->BindToClient(this);

  frame_rate_decider_ = std::make_unique<FrameRateDecider>(
      surface_manager_, this, hw_support_for_multiple_refresh_rates,
      SupportsSetFrameRate(output_surface_.get()),
      num_of_frames_to_toggle_interval);

  InitializeRenderer(enable_shared_images);

  damage_tracker_ = std::make_unique<DisplayDamageTracker>(surface_manager_,
                                                           aggregator_.get());
  if (scheduler_)
    scheduler_->SetDamageTracker(damage_tracker_.get());

  // This depends on assumptions that Display::Initialize will happen on the
  // same callstack as the ContextProvider being created/initialized or else
  // it could miss a callback before setting this.
  if (auto* context = output_surface_->context_provider())
    context->AddObserver(this);

  if (skia_output_surface_)
    skia_output_surface_->AddContextLostObserver(this);
}

void Display::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void Display::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Display::SetLocalSurfaceId(const LocalSurfaceId& id,
                                float device_scale_factor) {
  if (current_surface_id_.local_surface_id() == id &&
      device_scale_factor_ == device_scale_factor) {
    return;
  }

  TRACE_EVENT0("viz", "Display::SetSurfaceId");
  current_surface_id_ = SurfaceId(frame_sink_id_, id);
  device_scale_factor_ = device_scale_factor;

  damage_tracker_->SetNewRootSurface(current_surface_id_);
}

void Display::SetVisible(bool visible) {
  TRACE_EVENT1("viz", "Display::SetVisible", "visible", visible);
  if (renderer_)
    renderer_->SetVisible(visible);
  if (scheduler_)
    scheduler_->SetVisible(visible);
  visible_ = visible;

  if (!visible) {
    // Damage tracker needs a full reset as renderer resources are dropped when
    // not visible.
    if (aggregator_ && current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::Resize(const gfx::Size& size) {
  disable_swap_until_resize_ = false;

  if (size == current_surface_size_)
    return;

  // This DCHECK should probably go at the top of the function, but mac
  // sometimes calls Resize() with 0x0 before it sets a real size. This will
  // early out before the DCHECK fails.
  DCHECK(!size.IsEmpty());
  TRACE_EVENT0("viz", "Display::Resize");

  swapped_since_resize_ = false;
  current_surface_size_ = size;

  damage_tracker_->DisplayResized();
}

void Display::DisableSwapUntilResize(
    base::OnceClosure no_pending_swaps_callback) {
  TRACE_EVENT0("viz", "Display::DisableSwapUntilResize");
  DCHECK(no_pending_swaps_callback_.is_null());

  if (!disable_swap_until_resize_) {
    DCHECK(scheduler_);

    if (!swapped_since_resize_)
      scheduler_->ForceImmediateSwapIfPossible();

    if (no_pending_swaps_callback && pending_swaps_ > 0 &&
        (output_surface_->context_provider() ||
         output_surface_->AsSkiaOutputSurface())) {
      no_pending_swaps_callback_ = std::move(no_pending_swaps_callback);
    }

    disable_swap_until_resize_ = true;
  }

  // There are no pending swaps for current size so immediately run callback.
  if (no_pending_swaps_callback)
    std::move(no_pending_swaps_callback).Run();
}

void Display::SetColorMatrix(const SkMatrix44& matrix) {
  if (output_surface_)
    output_surface_->set_color_matrix(matrix);

  // Force a redraw.
  if (aggregator_) {
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }

  damage_tracker_->SetRootSurfaceDamaged();
}

void Display::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_color_spaces_ = display_color_spaces;
  if (aggregator_)
    aggregator_->SetDisplayColorSpaces(display_color_spaces_);
}

void Display::SetOutputIsSecure(bool secure) {
  if (secure == output_is_secure_)
    return;
  output_is_secure_ = secure;

  if (aggregator_) {
    aggregator_->set_output_is_secure(secure);
    // Force a redraw.
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::InitializeRenderer(bool enable_shared_images) {
  auto mode = output_surface_->context_provider() || skia_output_surface_
                  ? DisplayResourceProvider::kGpu
                  : DisplayResourceProvider::kSoftware;
  resource_provider_ = std::make_unique<DisplayResourceProvider>(
      mode, output_surface_->context_provider(), bitmap_manager_,
      enable_shared_images);
  if (settings_.use_skia_renderer && mode == DisplayResourceProvider::kGpu) {
    // Default to use DDL if skia_output_surface is not null.
    if (skia_output_surface_) {
      renderer_ = std::make_unique<SkiaRenderer>(
          &settings_, output_surface_.get(), resource_provider_.get(),
          overlay_processor_.get(), skia_output_surface_,
          SkiaRenderer::DrawMode::DDL);
    } else {
      // GPU compositing with GL to an SKP.
      DCHECK(output_surface_);
      DCHECK(output_surface_->context_provider());
      DCHECK(settings_.record_sk_picture);
      DCHECK(!overlay_processor_->IsOverlaySupported());
      renderer_ = std::make_unique<SkiaRenderer>(
          &settings_, output_surface_.get(), resource_provider_.get(),
          overlay_processor_.get(), nullptr /* skia_output_surface */,
          SkiaRenderer::DrawMode::SKPRECORD);
    }
  } else if (output_surface_->context_provider()) {
    renderer_ = std::make_unique<GLRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get(),
        overlay_processor_.get(), current_task_runner_);
  } else {
    DCHECK(!overlay_processor_->IsOverlaySupported());
    auto renderer = std::make_unique<SoftwareRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get(),
        overlay_processor_.get());
    software_renderer_ = renderer.get();
    renderer_ = std::move(renderer);
  }

  renderer_->Initialize();
  renderer_->SetVisible(visible_);

  // Outputting a partial list of quads might not work in cases where contents
  // outside the damage rect might be needed by the renderer.
  bool output_partial_list =
      output_surface_->capabilities().only_invalidates_damage_rect &&
      renderer_->use_partial_swap() &&
      !overlay_processor_->IsOverlaySupported();

  aggregator_ = std::make_unique<SurfaceAggregator>(
      surface_manager_, resource_provider_.get(), output_partial_list,
      overlay_processor_->NeedsSurfaceOccludingDamageRect());
  if (settings_.show_aggregated_damage)
    aggregator_->SetFrameAnnotator(std::make_unique<DamageFrameAnnotator>());

  aggregator_->set_output_is_secure(output_is_secure_);
  aggregator_->SetDisplayColorSpaces(display_color_spaces_);
  // Consider adding a softare limit as well.
  aggregator_->SetMaximumTextureSize(
      (output_surface_ && output_surface_->context_provider())
          ? output_surface_->context_provider()
                ->ContextCapabilities()
                .max_texture_size
          : 0);
}

bool Display::IsRootFrameMissing() const {
  return damage_tracker_->root_frame_missing();
}

bool Display::HasPendingSurfaces(const BeginFrameArgs& args) const {
  return damage_tracker_->HasPendingSurfaces(args);
}

void Display::OnContextLost() {
  if (scheduler_)
    scheduler_->OutputSurfaceLost();
  // WARNING: The client may delete the Display in this method call. Do not
  // make any additional references to members after this call.
  client_->DisplayOutputSurfaceLost();
}

bool Display::DrawAndSwap(base::TimeTicks expected_display_time) {
  TRACE_EVENT0("viz", "Display::DrawAndSwap");
  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;

  if (!current_surface_id_.is_valid()) {
    TRACE_EVENT_INSTANT0("viz", "No root surface.", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (!output_surface_) {
    TRACE_EVENT_INSTANT0("viz", "No output surface", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (output_surface_->capabilities().skips_draw) {
    TRACE_EVENT_INSTANT0("viz", "Skip draw", TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  gfx::OverlayTransform current_display_transform = gfx::OVERLAY_TRANSFORM_NONE;
  Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id_);
  if (surface->HasActiveFrame()) {
    current_display_transform =
        surface->GetActiveFrame().metadata.display_transform_hint;
    if (current_display_transform != output_surface_->GetDisplayTransform()) {
      output_surface_->SetDisplayTransformHint(current_display_transform);

      // Gets the transform from |output_surface_| back so that if it ignores
      // the hint, the rest of the code ignores the hint too.
      current_display_transform = output_surface_->GetDisplayTransform();
    }
  }

  // During aggregation, SurfaceAggregator marks all resources used for a draw
  // in the resource provider.  This has the side effect of deleting unused
  // resources and their textures, generating sync tokens, and returning the
  // resources to the client.  This involves GL work which is issued before
  // drawing commands, and gets prioritized by GPU scheduler because sync token
  // dependencies aren't issued until the draw.
  //
  // Batch and defer returning resources in resource provider.  This defers the
  // GL commands for deleting resources to after the draw, and prevents context
  // switching because the scheduler knows sync token dependencies at that time.
  DisplayResourceProvider::ScopedBatchReturnResources returner(
      resource_provider_.get(), /*allow_access_to_gpu_thread=*/true);

  base::ElapsedTimer aggregate_timer;
  aggregate_timer.Begin();
  CompositorFrame frame;
  {
    FrameRateDecider::ScopedAggregate scoped_aggregate(
        frame_rate_decider_.get());
    frame =
        aggregator_->Aggregate(current_surface_id_, expected_display_time,
                               current_display_transform, ++swapped_trace_id_);
  }

#if defined(OS_ANDROID)
  bool wide_color_enabled = display_color_spaces_.GetOutputColorSpace(
                                frame.metadata.content_color_usage, true) !=
                            gfx::ColorSpace::CreateSRGB();
  if (wide_color_enabled != last_wide_color_enabled_) {
    client_->SetWideColorEnabled(wide_color_enabled);
    last_wide_color_enabled_ = wide_color_enabled;
  }
#endif

  UMA_HISTOGRAM_COUNTS_1M("Compositing.SurfaceAggregator.AggregateUs",
                          aggregate_timer.Elapsed().InMicroseconds());

  if (frame.render_pass_list.empty()) {
    TRACE_EVENT_INSTANT0("viz", "Empty aggregated frame.",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  TRACE_EVENT_ASYNC_BEGIN0("viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
                           swapped_trace_id_);

  // Run callbacks early to allow pipelining and collect presented callbacks.
  damage_tracker_->RunDrawCallbacks();

  frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                     stored_latency_info_.begin(),
                                     stored_latency_info_.end());
  stored_latency_info_.clear();
  bool have_copy_requests = false;
  for (const auto& pass : frame.render_pass_list)
    have_copy_requests |= !pass->copy_requests.empty();

  gfx::Size surface_size;
  bool have_damage = false;
  auto& last_render_pass = *frame.render_pass_list.back();

  // The CompositorFrame provided by the SurfaceAggregator includes the display
  // transform while |current_surface_size_| is the pre-transform size received
  // from the client.
  const gfx::Transform display_transform = gfx::OverlayTransformToTransform(
      current_display_transform, gfx::SizeF(current_surface_size_));
  const gfx::Size current_surface_size =
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          display_transform, gfx::Rect(current_surface_size_))
          .size();
  if (settings_.auto_resize_output_surface &&
      last_render_pass.output_rect.size() != current_surface_size &&
      last_render_pass.damage_rect == last_render_pass.output_rect &&
      !current_surface_size.IsEmpty()) {
    // Resize the output rect to the current surface size so that we won't
    // skip the draw and so that the GL swap won't stretch the output.
    last_render_pass.output_rect.set_size(current_surface_size);
    last_render_pass.damage_rect = last_render_pass.output_rect;
  }
  surface_size = last_render_pass.output_rect.size();
  have_damage = !last_render_pass.damage_rect.size().IsEmpty();

  bool size_matches = surface_size == current_surface_size;
  if (!size_matches)
    TRACE_EVENT_INSTANT0("viz", "Size mismatch.", TRACE_EVENT_SCOPE_THREAD);

  bool should_draw = have_copy_requests || (have_damage && size_matches);
  client_->DisplayWillDrawAndSwap(should_draw, &frame.render_pass_list);

  base::Optional<base::ElapsedTimer> draw_timer;
  if (should_draw) {
    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "Draw");
    base::ElapsedTimer draw_occlusion_timer;
    RemoveOverdrawQuads(&frame);
    UMA_HISTOGRAM_COUNTS_1000(
        "Compositing.Display.Draw.Occlusion.Calculation.Time",
        draw_occlusion_timer.Elapsed().InMicroseconds());

    bool disable_image_filtering =
        frame.metadata.is_resourceless_software_draw_with_scroll_or_animation;
    if (software_renderer_) {
      software_renderer_->SetDisablePictureQuadImageFiltering(
          disable_image_filtering);
    } else {
      // This should only be set for software draws in synchronous compositor.
      DCHECK(!disable_image_filtering);
    }

    draw_timer.emplace();
    renderer_->DecideRenderPassAllocationsForFrame(frame.render_pass_list);
    renderer_->DrawFrame(&frame.render_pass_list, device_scale_factor_,
                         current_surface_size, display_color_spaces_);
    switch (output_surface_->type()) {
      case OutputSurface::Type::kSoftware:
        UMA_HISTOGRAM_COUNTS_1M(
            "Compositing.DirectRenderer.Software.DrawFrameUs",
            draw_timer->Elapsed().InMicroseconds());
        break;
      case OutputSurface::Type::kOpenGL:
        UMA_HISTOGRAM_COUNTS_1M("Compositing.DirectRenderer.GL.DrawFrameUs",
                                draw_timer->Elapsed().InMicroseconds());
        break;
      case OutputSurface::Type::kVulkan:
        UMA_HISTOGRAM_COUNTS_1M("Compositing.DirectRenderer.VK.DrawFrameUs",
                                draw_timer->Elapsed().InMicroseconds());
        break;
    }
  } else {
    TRACE_EVENT_INSTANT0("viz", "Draw skipped.", TRACE_EVENT_SCOPE_THREAD);
  }

  bool should_swap = !disable_swap_until_resize_ && should_draw && size_matches;
  if (should_swap) {
    PresentationGroupTiming presentation_group_timing;
    presentation_group_timing.OnDraw(draw_timer->Begin());

    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
      if (surface) {
        std::unique_ptr<Surface::PresentationHelper> helper =
            surface->TakePresentationHelperForPresentNotification();
        if (helper) {
          presentation_group_timing.AddPresentationHelper(std::move(helper));
        }
      }
    }
    pending_presentation_group_timings_.emplace_back(
        std::move(presentation_group_timing));

    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "WaitForSwap");
    swapped_since_resize_ = true;

    ui::LatencyInfo::TraceIntermediateFlowEvents(
        frame.metadata.latency_info,
        perfetto::protos::pbzero::ChromeLatencyInfo::STEP_DRAW_AND_SWAP);

    cc::benchmark_instrumentation::IssueDisplayRenderingStatsEvent();
    DirectRenderer::SwapFrameData swap_frame_data;
    swap_frame_data.latency_info = std::move(frame.metadata.latency_info);
    if (frame.metadata.top_controls_visible_height.has_value()) {
      swap_frame_data.top_controls_visible_height_changed =
          last_top_controls_visible_height_ !=
          *frame.metadata.top_controls_visible_height;
      last_top_controls_visible_height_ =
          *frame.metadata.top_controls_visible_height;
    }

    // We must notify scheduler and increase |pending_swaps_| before calling
    // SwapBuffers() as it can call DidReceiveSwapBuffersAck synchronously.
    if (scheduler_)
      scheduler_->DidSwapBuffers();
    pending_swaps_++;

    renderer_->SwapBuffers(std::move(swap_frame_data));
  } else {
    TRACE_EVENT_INSTANT0("viz", "Swap skipped.", TRACE_EVENT_SCOPE_THREAD);

    if (have_damage && !size_matches)
      aggregator_->SetFullDamageForSurface(current_surface_id_);

    if (have_damage) {
      // Do not store more than the allowed size.
      if (ui::LatencyInfo::Verify(frame.metadata.latency_info,
                                  "Display::DrawAndSwap")) {
        stored_latency_info_.swap(frame.metadata.latency_info);
      }
    } else {
      // There was no damage. Terminate the latency info objects.
      while (!frame.metadata.latency_info.empty()) {
        auto& latency = frame.metadata.latency_info.back();
        latency.Terminate();
        frame.metadata.latency_info.pop_back();
      }
    }

    renderer_->SwapBuffersSkipped();

    TRACE_EVENT_ASYNC_END1("viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
                           swapped_trace_id_, "status", "canceled");
    --swapped_trace_id_;
    if (scheduler_) {
      scheduler_->DidSwapBuffers();
      scheduler_->DidReceiveSwapBuffersAck();
    }
  }

  client_->DisplayDidDrawAndSwap();

  // Garbage collection can lead to sync IPCs to the GPU service to verify sync
  // tokens. We defer garbage collection until the end of DrawAndSwap to avoid
  // stalling the critical path for compositing.
  surface_manager_->GarbageCollectSurfaces();

  return true;
}

void Display::DidReceiveSwapBuffersAck(const gfx::SwapTimings& timings) {
  // Adding to |pending_presentation_group_timings_| must
  // have been done in DrawAndSwap(), and should not be popped until
  // DidReceiveSwapBuffersAck.
  DCHECK(!pending_presentation_group_timings_.empty());

  ++last_swap_ack_trace_id_;
  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "viz,benchmark", "Graphics.Pipeline.DrawAndSwap", last_swap_ack_trace_id_,
      "Swap", timings.swap_start);
  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "viz,benchmark", "Graphics.Pipeline.DrawAndSwap", last_swap_ack_trace_id_,
      "WaitForPresentation", timings.swap_end);

  DCHECK_GT(pending_swaps_, 0);
  pending_swaps_--;
  if (scheduler_) {
    scheduler_->DidReceiveSwapBuffersAck();
  }

  if (no_pending_swaps_callback_ && pending_swaps_ == 0)
    std::move(no_pending_swaps_callback_).Run();

  if (overlay_processor_)
    overlay_processor_->OverlayPresentationComplete();
  if (renderer_)
    renderer_->SwapBuffersComplete();

  // It's possible to receive multiple calls to DidReceiveSwapBuffersAck()
  // before DidReceivePresentationFeedback(). Ensure that we're not setting
  // |swap_timings_| for the same PresentationGroupTiming multiple times.
  base::TimeTicks draw_start_timestamp;
  for (auto& group_timing : pending_presentation_group_timings_) {
    if (!group_timing.HasSwapped()) {
      group_timing.OnSwap(timings);
      draw_start_timestamp = group_timing.draw_start_timestamp();
      break;
    }
  }

  // We should have at least one group that hasn't received a SwapBuffersAck
  DCHECK(!draw_start_timestamp.is_null());

  // Check that the swap timings correspond with the timestamp from when
  // the swap was triggered. Note that not all output surfaces provide timing
  // information, hence the check for a valid swap_start.
  if (!timings.swap_start.is_null()) {
    DCHECK_LE(draw_start_timestamp, timings.swap_start);
    base::TimeDelta delta = timings.swap_start - draw_start_timestamp;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Compositing.Display.DrawToSwapUs", delta, kDrawToSwapMin,
        kDrawToSwapMax, kDrawToSwapUsBuckets);
  }
}

void Display::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  if (renderer_)
    renderer_->DidReceiveTextureInUseResponses(responses);
}

void Display::DidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  if (client_)
    client_->DisplayDidReceiveCALayerParams(ca_layer_params);
}

void Display::DidSwapWithSize(const gfx::Size& pixel_size) {
  if (client_)
    client_->DisplayDidCompleteSwapWithSize(pixel_size);
}

void Display::DidReceivePresentationFeedback(
    const gfx::PresentationFeedback& feedback) {
  if (pending_presentation_group_timings_.empty()) {
    DLOG(ERROR) << "Received unexpected PresentationFeedback";
    return;
  }
  ++last_presented_trace_id_;
  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
      "viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
      last_presented_trace_id_, feedback.timestamp);
  auto& presentation_group_timing = pending_presentation_group_timings_.front();
  auto copy_feedback = SanitizePresentationFeedback(
      feedback, presentation_group_timing.draw_start_timestamp());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "benchmark,viz", "Display::FrameDisplayed", TRACE_EVENT_SCOPE_THREAD,
      copy_feedback.timestamp);
  presentation_group_timing.OnPresent(copy_feedback);
  pending_presentation_group_timings_.pop_front();
}

void Display::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  aggregator_->SetFullDamageForSurface(current_surface_id_);
  damage_tracker_->SetRootSurfaceDamaged();
}

void Display::DidFinishFrame(const BeginFrameAck& ack) {
  for (auto& observer : observers_)
    observer.OnDisplayDidFinishFrame(ack);

  // Only used with experimental de-jelly effect. Forces us to produce a new
  // un-skewed frame if the last one had a de-jelly skew applied. This prevents
  // de-jelly skew from staying on screen for more than one frame.
  if (aggregator_->last_frame_had_jelly()) {
    scheduler_->SetNeedsOneBeginFrame(true);
  }
}

const SurfaceId& Display::CurrentSurfaceId() {
  return current_surface_id_;
}

LocalSurfaceId Display::GetSurfaceAtAggregation(
    const FrameSinkId& frame_sink_id) const {
  if (!aggregator_)
    return LocalSurfaceId();
  auto it = aggregator_->previous_contained_frame_sinks().find(frame_sink_id);
  if (it == aggregator_->previous_contained_frame_sinks().end())
    return LocalSurfaceId();
  return it->second;
}

void Display::SoftwareDeviceUpdatedCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  if (client_)
    client_->DisplayDidReceiveCALayerParams(ca_layer_params);
}

void Display::ForceImmediateDrawAndSwapIfPossible() {
  if (scheduler_)
    scheduler_->ForceImmediateSwapIfPossible();
}

void Display::SetNeedsOneBeginFrame() {
  if (scheduler_)
    scheduler_->SetNeedsOneBeginFrame(false);
}

void Display::RemoveOverdrawQuads(CompositorFrame* frame) {
  if (frame->render_pass_list.empty())
    return;

  base::flat_map<RenderPassId, gfx::Rect> backdrop_filter_rects;
  for (const auto& pass : frame->render_pass_list) {
    if (!pass->backdrop_filters.IsEmpty() &&
        pass->backdrop_filters.HasFilterThatMovesPixels()) {
      backdrop_filter_rects[pass->id] = cc::MathUtil::MapEnclosingClippedRect(
          pass->transform_to_root_target, pass->output_rect);
    }
  }

  for (const auto& pass : frame->render_pass_list) {
    const SharedQuadState* last_sqs = nullptr;
    cc::Region occlusion_in_target_space;
    cc::Region backdrop_filters_in_target_space;
    bool current_sqs_intersects_occlusion = false;

    // TODO(yiyix): Add filter effects to draw occlusion calculation
    if (!pass->filters.IsEmpty() || !pass->backdrop_filters.IsEmpty())
      continue;

    // When there is only one quad in the render pass, occlusion is not
    // possible.
    if (pass->quad_list.size() == 1)
      continue;

    auto quad_list_end = pass->quad_list.end();
    cc::Region occlusion_in_quad_content_space;
    gfx::Rect render_pass_quads_in_content_space;
    for (auto quad = pass->quad_list.begin(); quad != quad_list_end;) {
      // Skip quad if it is a RenderPassDrawQuad because RenderPassDrawQuad is a
      // special type of DrawQuad where the visible_rect of shared quad state is
      // not entirely covered by draw quads in it.
      if (quad->material == ContentDrawQuadBase::Material::kRenderPass) {
        // A RenderPass with backdrop filters may apply to a quad underlying
        // RenderPassQuad. These regions should be tracked so that correctly
        // handle splitting and occlusion of the underlying quad.
        auto it = backdrop_filter_rects.find(
            RenderPassDrawQuad::MaterialCast(*quad)->render_pass_id);
        if (it != backdrop_filter_rects.end()) {
          backdrop_filters_in_target_space.Union(it->second);
        }
        ++quad;
        continue;
      }
      // Also skip quad if the DrawQuad is inside a 3d object.
      if (quad->shared_quad_state->sorting_context_id != 0) {
        ++quad;
        continue;
      }

      if (!last_sqs)
        last_sqs = quad->shared_quad_state;

      gfx::Transform transform =
          quad->shared_quad_state->quad_to_target_transform;

      // TODO(yiyix): Find a rect interior to each transformed quad.
      if (last_sqs != quad->shared_quad_state) {
        if (last_sqs->opacity == 1 && last_sqs->are_contents_opaque &&
            (last_sqs->blend_mode == SkBlendMode::kSrcOver ||
             last_sqs->blend_mode == SkBlendMode::kSrc) &&
            last_sqs->quad_to_target_transform.Preserves2dAxisAlignment()) {
          gfx::Rect sqs_rect_in_target =
              cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                  last_sqs->quad_to_target_transform,
                  last_sqs->visible_quad_layer_rect);

          // If a rounded corner is being applied then the visible rect for the
          // sqs is actually even smaller. Reduce the rect size to get a
          // rounded corner adjusted occluding region.
          if (!last_sqs->rounded_corner_bounds.IsEmpty()) {
            sqs_rect_in_target.Intersect(gfx::ToEnclosedRect(
                GetOccludingRectForRRectF(last_sqs->rounded_corner_bounds)));
          }

          if (last_sqs->is_clipped)
            sqs_rect_in_target.Intersect(last_sqs->clip_rect);

          // If region complexity is above our threshold, remove the smallest
          // rects from occlusion region.
          occlusion_in_target_space.Union(sqs_rect_in_target);
          while (occlusion_in_target_space.GetRegionComplexity() >
                 settings_.kMaximumOccluderComplexity) {
            gfx::Rect smallest_rect = *occlusion_in_target_space.begin();
            for (const auto& occluding_rect : occlusion_in_target_space) {
              if (occluding_rect.size().GetArea() <
                  smallest_rect.size().GetArea())
                smallest_rect = occluding_rect;
            }
            occlusion_in_target_space.Subtract(smallest_rect);
          }
        }
        // If the visible_rect of the current shared quad state does not
        // intersect with the occlusion rect, we can skip draw occlusion checks
        // for quads in the current SharedQuadState.
        last_sqs = quad->shared_quad_state;
        occlusion_in_quad_content_space.Clear();
        render_pass_quads_in_content_space = gfx::Rect();
        const auto current_sqs_in_target_space =
            cc::MathUtil::MapEnclosingClippedRect(
                transform, last_sqs->visible_quad_layer_rect);
        current_sqs_intersects_occlusion =
            occlusion_in_target_space.Intersects(current_sqs_in_target_space);

        // Compute the occlusion region in the quad content space for scale and
        // translation transforms. Note that 0 scale transform will fail the
        // positive scale check.
        if (current_sqs_intersects_occlusion &&
            transform.IsPositiveScaleOrTranslation()) {
          gfx::Transform reverse_transform;
          bool is_invertible = transform.GetInverse(&reverse_transform);
          // Scale transform can be inverted by multiplying 1/scale (given
          // scale > 0) and translation transform can be inverted by applying
          // the reversed directional translation. Therefore, |transform| is
          // always invertible.
          DCHECK(is_invertible);
          DCHECK_LE(occlusion_in_target_space.GetRegionComplexity(),
                    settings_.kMaximumOccluderComplexity);

          // Since transform can only be a scale or a translation matrix, it is
          // safe to use function MapEnclosedRectWith2dAxisAlignedTransform to
          // define occluded region in the quad content space with inverted
          // transform.
          for (const gfx::Rect& rect_in_target_space :
               occlusion_in_target_space) {
            if (current_sqs_in_target_space.Intersects(rect_in_target_space)) {
              auto rect_in_content =
                  cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                      reverse_transform, rect_in_target_space);
              occlusion_in_quad_content_space.Union(
                  SafeConvertRectForRegion(rect_in_content));
            }
          }

          // A render pass quad may apply some filter or transform to an
          // underlying quad. Do not split quads when they intersect with a
          // render pass quad.
          if (current_sqs_in_target_space.Intersects(
                  backdrop_filters_in_target_space.bounds())) {
            for (const auto& rect_in_target_space :
                 backdrop_filters_in_target_space) {
              auto rect_in_content =
                  cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                      reverse_transform, rect_in_target_space);
              render_pass_quads_in_content_space.Union(rect_in_content);
            }
          }
        }
      }

      if (!current_sqs_intersects_occlusion) {
        ++quad;
        continue;
      }

      if (occlusion_in_quad_content_space.Contains(quad->visible_rect)) {
        // Case 1: for simple transforms (scale or translation), define the
        // occlusion region in the quad content space. If |quad| is not
        // shown on the screen, then set its rect and visible_rect to be empty.
        quad->visible_rect.set_size(gfx::Size());
      } else if (occlusion_in_quad_content_space.Intersects(
                     quad->visible_rect)) {
        // Case 2: for simple transforms, if the quad is partially shown on
        // screen and the region formed by (occlusion region - visible_rect) is
        // a rect, then update visible_rect to the resulting rect.
        cc::Region visible_region = quad->visible_rect;
        visible_region.Subtract(occlusion_in_quad_content_space);
        quad->visible_rect = visible_region.bounds();

        // Split quad into multiple draw quads when area can be reduce by
        // more than X fragments.
        const bool should_split_quads =
            enable_quad_splitting_ &&
            !visible_region.Intersects(render_pass_quads_in_content_space) &&
            ReduceComplexity(visible_region, settings_.quad_split_limit,
                             &cached_visible_region_) &&
            CanSplitQuad(quad->material, ComputeArea(cached_visible_region_),
                         visible_region.bounds().size().GetArea(),
                         settings_.minimum_fragments_reduced,
                         device_scale_factor_);
        if (should_split_quads) {
          auto new_quad = pass->quad_list.InsertCopyBeforeDrawQuad(
              quad, cached_visible_region_.size() - 1);
          for (const auto& visible_rect : cached_visible_region_) {
            new_quad->visible_rect = visible_rect;
            ++new_quad;
          }
          quad = new_quad;
          continue;
        }
      } else if (occlusion_in_quad_content_space.IsEmpty() &&
                 occlusion_in_target_space.Contains(
                     cc::MathUtil::MapEnclosingClippedRect(
                         transform, quad->visible_rect))) {
        // Case 3: for non simple transforms, define the occlusion region in
        // target space. If |quad| is not shown on the screen, then set its
        // rect and visible_rect to be empty.
        quad->visible_rect.set_size(gfx::Size());
      }
      ++quad;
    }
  }
}

void Display::SetPreferredFrameInterval(base::TimeDelta interval) {
  if (frame_rate_decider_->supports_set_frame_rate()) {
    float frame_rate =
        interval.InSecondsF() == 0 ? 0 : (1 / interval.InSecondsF());
    output_surface_->SetFrameRate(frame_rate);
    return;
  }

  client_->SetPreferredFrameInterval(interval);
}

base::TimeDelta Display::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id,
    mojom::CompositorFrameSinkType* type) {
  return client_->GetPreferredFrameIntervalForFrameSinkId(id, type);
}

void Display::SetSupportedFrameIntervals(
    std::vector<base::TimeDelta> intervals) {
  frame_rate_decider_->SetSupportedFrameIntervals(std::move(intervals));
}

base::ScopedClosureRunner Display::GetCacheBackBufferCb() {
  return output_surface_->GetCacheBackBufferCb();
}

void Display::DisableGPUAccessByDefault() {
  DCHECK(resource_provider_);
  resource_provider_->SetAllowAccessToGPUThread(false);
}

}  // namespace viz
