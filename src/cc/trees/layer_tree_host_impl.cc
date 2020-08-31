// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <list>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/base/switches.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/input/scroll_state.h"
#include "cc/input/scrollbar.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/scroller_size_metrics.h"
#include "cc/input/snap_selection_strategy.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/effect_tree_layer_list_iterator.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/viewport.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "cc/metrics/lcd_text_metrics_reporter.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_worklet_job.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/tiles/eviction_tile_priority_queue.h"
#include "cc/tiles/frame_viewer_instrumentation.h"
#include "cc/tiles/gpu_image_decode_cache.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/debug_rect_history.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/frame_rate_counter.h"
#include "cc/trees/image_animation_controller.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/presentation_time_callback_buffer.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_and_scale_set.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/tree_synchronizer.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/traced_value.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

// Used to accommodate finite precision when comparing scaled viewport and
// content widths. While this value may seem large, width=device-width on an N7
// V1 saw errors of ~0.065 between computed window and content widths.
const float kMobileViewportWidthEpsilon = 0.15f;

// In BuildHitTestData we iterate all layers to find all layers that overlap
// OOPIFs, but when the number of layers is greater than
// |kAssumeOverlapThreshold|, it can be inefficient to accumulate layer bounds
// for overlap checking. As a result, we are conservative and make OOPIFs
// kHitTestAsk after the threshold is reached.
const size_t kAssumeOverlapThreshold = 100;

FrameSequenceTrackerType GetTrackerTypeForScroll(
    ui::ScrollInputType input_type) {
  switch (input_type) {
    case ui::ScrollInputType::kWheel:
      return FrameSequenceTrackerType::kWheelScroll;
    case ui::ScrollInputType::kTouchscreen:
      return FrameSequenceTrackerType::kTouchScroll;
    case ui::ScrollInputType::kScrollbar:
      return FrameSequenceTrackerType::kScrollbarScroll;
    case ui::ScrollInputType::kAutoscroll:
      return FrameSequenceTrackerType::kMaxType;
  }
}

bool HasFixedPageScale(LayerTreeImpl* active_tree) {
  return active_tree->min_page_scale_factor() ==
         active_tree->max_page_scale_factor();
}

bool HasMobileViewport(LayerTreeImpl* active_tree) {
  float window_width_dip = active_tree->current_page_scale_factor() *
                           active_tree->ScrollableViewportSize().width();
  float content_width_css = active_tree->ScrollableSize().width();
  return content_width_css <= window_width_dip + kMobileViewportWidthEpsilon;
}

bool IsMobileOptimized(LayerTreeImpl* active_tree) {
  bool has_mobile_viewport = HasMobileViewport(active_tree);
  bool has_fixed_page_scale = HasFixedPageScale(active_tree);
  return has_fixed_page_scale || has_mobile_viewport;
}

// This helper returns an adjusted version of |delta| where the scroll delta is
// cleared in any axis in which user scrolling is disabled (e.g. by
// |overflow-x: hidden|).
gfx::Vector2dF UserScrollableDelta(const ScrollNode& node,
                                   const gfx::Vector2dF& delta) {
  gfx::Vector2dF adjusted_delta = delta;
  if (!node.user_scrollable_horizontal)
    adjusted_delta.set_x(0);
  if (!node.user_scrollable_vertical)
    adjusted_delta.set_y(0);

  return adjusted_delta;
}

viz::ResourceFormat TileRasterBufferFormat(
    const LayerTreeSettings& settings,
    viz::ContextProvider* context_provider,
    bool use_gpu_rasterization) {
  // Software compositing always uses the native skia RGBA N32 format, but we
  // just call it RGBA_8888 everywhere even though it can be BGRA ordering,
  // because we don't need to communicate the actual ordering as the code all
  // assumes the native skia format.
  if (!context_provider)
    return viz::RGBA_8888;

  // RGBA4444 overrides the defaults if specified, but only for gpu compositing.
  // It is always supported on platforms where it is specified.
  if (settings.use_rgba_4444)
    return viz::RGBA_4444;
  // Otherwise we use BGRA textures if we can but it depends on the context
  // capabilities, and we have different preferences when rastering to textures
  // vs uploading textures.
  const gpu::Capabilities& caps = context_provider->ContextCapabilities();
  if (use_gpu_rasterization)
    return viz::PlatformColor::BestSupportedRenderBufferFormat(caps);
  return viz::PlatformColor::BestSupportedTextureFormat(caps);
}

void DidVisibilityChange(LayerTreeHostImpl* id, bool visible) {
  if (visible) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("cc", "LayerTreeHostImpl::SetVisible",
                                      TRACE_ID_LOCAL(id), "LayerTreeHostImpl",
                                      id);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("cc", "LayerTreeHostImpl::SetVisible",
                                  TRACE_ID_LOCAL(id));
}

enum ScrollThread { MAIN_THREAD, CC_THREAD };

void RecordCompositorSlowScrollMetric(ui::ScrollInputType type,
                                      ScrollThread scroll_thread) {
  bool scroll_on_main_thread = (scroll_thread == MAIN_THREAD);
  if (type == ui::ScrollInputType::kWheel) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CompositorWheelScrollUpdateThread",
                          scroll_on_main_thread);
  } else if (type == ui::ScrollInputType::kTouchscreen) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CompositorTouchScrollUpdateThread",
                          scroll_on_main_thread);
  }
}

void PopulateMetadataContentColorUsage(
    const LayerTreeHostImpl::FrameData* frame,
    viz::CompositorFrameMetadata* metadata) {
  metadata->content_color_usage = gfx::ContentColorUsage::kSRGB;
  for (const LayerImpl* layer : frame->will_draw_layers) {
    metadata->content_color_usage =
        std::max(metadata->content_color_usage, layer->GetContentColorUsage());
  }
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SourceIdConsistency : int {
  kConsistent = 0,
  kContainsInvalid = 1,
  kNonUnique = 2,
  kInvalidAndNonUnique = 3,
  kMaxValue = kInvalidAndNonUnique,
};

void RecordSourceIdConsistency(bool all_valid, bool all_unique) {
  SourceIdConsistency consistency =
      all_unique ? (all_valid ? SourceIdConsistency::kConsistent
                              : SourceIdConsistency::kContainsInvalid)
                 : (all_valid ? SourceIdConsistency::kNonUnique
                              : SourceIdConsistency::kInvalidAndNonUnique);

  // TODO(crbug.com/1062764): we're sometimes seeing unexpected values for the
  // ukm::SourceId. We'll use this histogram to track how often it happens so
  // we can properly (de-)prioritize a fix.
  UMA_HISTOGRAM_ENUMERATION("Event.LatencyInfo.Debug.SourceIdConsistency",
                            consistency);
}

}  // namespace

DEFINE_SCOPED_UMA_HISTOGRAM_TIMER(PendingTreeRasterDurationHistogramTimer,
                                  "Scheduling.%s.PendingTreeRasterDuration")

LayerTreeHostImpl::FrameData::FrameData() = default;
LayerTreeHostImpl::FrameData::~FrameData() = default;
LayerTreeHostImpl::UIResourceData::UIResourceData() = default;
LayerTreeHostImpl::UIResourceData::~UIResourceData() = default;
LayerTreeHostImpl::UIResourceData::UIResourceData(UIResourceData&&) noexcept =
    default;
LayerTreeHostImpl::UIResourceData& LayerTreeHostImpl::UIResourceData::operator=(
    UIResourceData&&) = default;

std::unique_ptr<LayerTreeHostImpl> LayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingClient* scheduling_client) {
  return base::WrapUnique(new LayerTreeHostImpl(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host), id,
      std::move(image_worker_task_runner), scheduling_client));
}

LayerTreeHostImpl::LayerTreeHostImpl(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingClient* scheduling_client)
    : client_(client),
      scheduling_client_(scheduling_client),
      task_runner_provider_(task_runner_provider),
      current_begin_frame_tracker_(FROM_HERE),
      compositor_frame_reporting_controller_(
          std::make_unique<CompositorFrameReportingController>(
              /*should_report_metrics=*/!settings
                  .single_thread_proxy_scheduler)),
      settings_(settings),
      is_synchronous_single_threaded_(!task_runner_provider->HasImplThread() &&
                                      !settings_.single_thread_proxy_scheduler),
      cached_managed_memory_policy_(settings.memory_policy),
      // Must be initialized after is_synchronous_single_threaded_ and
      // task_runner_provider_.
      tile_manager_(this,
                    GetTaskRunner(),
                    std::move(image_worker_task_runner),
                    is_synchronous_single_threaded_
                        ? std::numeric_limits<size_t>::max()
                        : settings.scheduled_raster_task_limit,
                    settings.ToTileManagerSettings()),
      fps_counter_(
          FrameRateCounter::Create(task_runner_provider_->HasImplThread())),
      memory_history_(MemoryHistory::Create()),
      debug_rect_history_(DebugRectHistory::Create()),
      mutator_host_(std::move(mutator_host)),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      micro_benchmark_controller_(this),
      task_graph_runner_(task_graph_runner),
      id_(id),
      consecutive_frame_with_damage_count_(settings.damaged_frame_limit),
      // It is safe to use base::Unretained here since we will outlive the
      // ImageAnimationController.
      image_animation_controller_(GetTaskRunner(),
                                  this,
                                  settings_.enable_image_animation_resync),
      paint_image_generator_client_id_(PaintImage::GetNextGeneratorClientId()),
      scrollbar_controller_(std::make_unique<ScrollbarController>(this)),
      frame_trackers_(settings.single_thread_proxy_scheduler,
                      compositor_frame_reporting_controller_.get()),
      scroll_gesture_did_end_(false),
      lcd_text_metrics_reporter_(LCDTextMetricsReporter::CreateIfNeeded(this)),
      frame_rate_estimator_(GetTaskRunner()) {
  DCHECK(mutator_host_);
  mutator_host_->SetMutatorHostClient(this);
  mutator_events_ = mutator_host_->CreateEvents();

  DCHECK(task_runner_provider_->IsImplThread());
  DidVisibilityChange(this, visible_);

  // LTHI always has an active tree.
  active_tree_ = std::make_unique<LayerTreeImpl>(
      this, new SyncedProperty<ScaleGroup>, new SyncedBrowserControls,
      new SyncedBrowserControls, new SyncedElasticOverscroll);
  active_tree_->property_trees()->is_active = true;

  viewport_ = Viewport::Create(this);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                                     "cc::LayerTreeHostImpl", id_);

  browser_controls_offset_manager_ = BrowserControlsOffsetManager::Create(
      this, settings.top_controls_show_threshold,
      settings.top_controls_hide_threshold);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLayerTreeHostMemoryPressure)) {
    memory_pressure_listener_.reset(
        new base::MemoryPressureListener(base::BindRepeating(
            &LayerTreeHostImpl::OnMemoryPressure, base::Unretained(this))));
  }

  SetDebugState(settings.initial_debug_state);
}

LayerTreeHostImpl::~LayerTreeHostImpl() {
  DCHECK(task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHostImpl::~LayerTreeHostImpl()");
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                                     "cc::LayerTreeHostImpl", id_);

  // The frame sink is released before shutdown, which takes down
  // all the resource and raster structures.
  DCHECK(!layer_tree_frame_sink_);
  DCHECK(!resource_pool_);
  DCHECK(!image_decode_cache_);
  DCHECK(!single_thread_synchronous_task_graph_runner_);

  if (input_handler_client_) {
    input_handler_client_->WillShutdown();
    input_handler_client_ = nullptr;
  }
  if (scroll_elasticity_helper_)
    scroll_elasticity_helper_.reset();

  // The layer trees must be destroyed before the LayerTreeHost. Also, if they
  // are holding onto any resources, destroying them will release them, before
  // we mark any leftover resources as lost.
  if (recycle_tree_)
    recycle_tree_->Shutdown();
  if (pending_tree_)
    pending_tree_->Shutdown();
  active_tree_->Shutdown();
  recycle_tree_ = nullptr;
  pending_tree_ = nullptr;
  active_tree_ = nullptr;

  // All resources should already be removed, so lose anything still exported.
  resource_provider_.ShutdownAndReleaseAllResources();

  mutator_host_->ClearMutators();
  mutator_host_->SetMutatorHostClient(nullptr);

  // Clear the UKM Manager so that we do not try to report when the
  // UKM System has shut down.
  compositor_frame_reporting_controller_->SetUkmManager(nullptr);
  frame_trackers_.SetUkmManager(nullptr);
}

void LayerTreeHostImpl::WillSendBeginMainFrame() {
  if (scheduling_client_)
    scheduling_client_->DidScheduleBeginMainFrame();
}

void LayerTreeHostImpl::DidSendBeginMainFrame(const viz::BeginFrameArgs& args) {
  frame_trackers_.NotifyBeginMainFrame(args);
}

void LayerTreeHostImpl::BeginMainFrameAborted(
    CommitEarlyOutReason reason,
    std::vector<std::unique_ptr<SwapPromise>> swap_promises,
    const viz::BeginFrameArgs& args) {
  if (reason == CommitEarlyOutReason::ABORTED_NOT_VISIBLE ||
      reason == CommitEarlyOutReason::FINISHED_NO_UPDATES) {
    frame_trackers_.NotifyMainFrameCausedNoDamage(args);
  } else {
    frame_trackers_.NotifyMainFrameProcessed(args);
  }

  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  if (CommitEarlyOutHandledCommit(reason)) {
    active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit();
    if (pending_tree_) {
      pending_tree_->AppendSwapPromises(std::move(swap_promises));
    } else {
      for (const auto& swap_promise : swap_promises)
        swap_promise->DidNotSwap(SwapPromise::COMMIT_NO_UPDATE);
    }
  }
}

void LayerTreeHostImpl::ReadyToCommit(const viz::BeginFrameArgs& commit_args) {
  frame_trackers_.NotifyMainFrameProcessed(commit_args);
}

void LayerTreeHostImpl::BeginCommit() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BeginCommit");

  if (!CommitToActiveTree())
    CreatePendingTree();
}

void LayerTreeHostImpl::CommitComplete() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::CommitComplete");

  // In high latency mode commit cannot finish within the same frame. We need to
  // flush input here to make sure they got picked up by |PrepareTiles()|.
  if (input_handler_client_ && impl_thread_phase_ == ImplThreadPhase::IDLE)
    input_handler_client_->DeliverInputForHighLatencyMode();

  if (CommitToActiveTree()) {
    active_tree_->HandleScrollbarShowRequestsFromMain();

    // Property tree nodes have been updated by the commit. Update elements
    // available on active tree to start/stop ticking animations.
    UpdateElements(ElementListType::ACTIVE);

    // We have to activate animations here or "IsActive()" is true on the layers
    // but the animations aren't activated yet so they get ignored by
    // UpdateDrawProperties.
    ActivateAnimations();
  }

  // We clear the entries that were never mutated by CC animations from the last
  // commit until now. Moreover, we reset the values of input properties and
  // relies on the fact that CC animation will mutate those values when pending
  // tree is animated below.
  // With that, when CC finishes animating an input property, the value of that
  // property stays at finish state until a commit kicks in, which is consistent
  // with current composited animations.
  paint_worklet_tracker_.ClearUnusedInputProperties();

  // Start animations before UpdateDrawProperties and PrepareTiles, as they can
  // change the results. When doing commit to the active tree, this must happen
  // after ActivateAnimations() in order for this ticking to be propogated
  // to layers on the active tree.
  if (CommitToActiveTree())
    Animate();
  else
    AnimatePendingTreeAfterCommit();

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
  micro_benchmark_controller_.DidCompleteCommit();

  if (mutator_host_->CurrentFrameHadRAF())
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kRAF);

  if (mutator_host_->MainThreadAnimationsCount() > 0) {
    frame_trackers_.StartSequence(
        FrameSequenceTrackerType::kMainThreadAnimation);
  }

  for (const auto& info : mutator_host_->TakePendingThroughputTrackerInfos()) {
    const MutatorHost::TrackedAnimationSequenceId sequence_id = info.id;
    const bool start = info.start;
    if (start)
      frame_trackers_.StartCustomSequence(sequence_id);
    else
      frame_trackers_.StopCustomSequence(sequence_id);
  }
}

void LayerTreeHostImpl::UpdateSyncTreeAfterCommitOrImplSideInvalidation() {
  // LayerTreeHost may have changed the GPU rasterization flags state, which
  // may require an update of the tree resources.
  UpdateTreeResourcesForGpuRasterizationIfNeeded();
  sync_tree()->set_needs_update_draw_properties();

  // We need an update immediately post-commit to have the opportunity to create
  // tilings.
  // We can avoid updating the ImageAnimationController during this
  // DrawProperties update since it will be done when we animate the controller
  // below.
  bool update_image_animation_controller = false;
  sync_tree()->UpdateDrawProperties(update_image_animation_controller);
  // Because invalidations may be coming from the main thread, it's
  // safe to do an update for lcd text at this point and see if lcd text needs
  // to be disabled on any layers.
  // It'd be ideal if this could be done earlier, but when the raster source
  // is updated from the main thread during push properties, update draw
  // properties has not occurred yet and so it's not clear whether or not the
  // layer can or cannot use lcd text.  So, this is the cleanup pass to
  // determine if lcd state needs to switch due to draw properties.
  sync_tree()->UpdateCanUseLCDText();

  // Defer invalidating images until UpdateDrawProperties is performed since
  // that updates whether an image should be animated based on its visibility
  // and the updated data for the image from the main frame.
  PaintImageIdFlatSet images_to_invalidate =
      tile_manager_.TakeImagesToInvalidateOnSyncTree();
  if (ukm_manager_)
    ukm_manager_->AddCheckerboardedImages(images_to_invalidate.size());

  const auto& animated_images =
      image_animation_controller_.AnimateForSyncTree(CurrentBeginFrameArgs());
  images_to_invalidate.insert(animated_images.begin(), animated_images.end());

  // Invalidate cached PaintRecords for worklets whose input properties were
  // mutated since the last pending tree. We keep requesting invalidations until
  // the animation is ticking on impl thread. Note that this works since the
  // animation starts ticking on the pending tree
  // (AnimatePendingTreeAfterCommit) which committed this animation timeline.
  // After this the animation may only tick on the active tree for impl-side
  // invalidations (since AnimatePendingTreeAfterCommit is not done for pending
  // trees created by impl-side invalidations). But we ensure here that we
  // request another invalidation if an input property was mutated on the active
  // tree.
  if (paint_worklet_tracker_.InvalidatePaintWorkletsOnPendingTree()) {
    client_->NeedsImplSideInvalidation(
        true /* needs_first_draw_on_activation */);
  }
  PaintImageIdFlatSet dirty_paint_worklet_ids;
  PaintWorkletJobMap dirty_paint_worklets =
      GatherDirtyPaintWorklets(&dirty_paint_worklet_ids);
  images_to_invalidate.insert(dirty_paint_worklet_ids.begin(),
                              dirty_paint_worklet_ids.end());

  sync_tree()->InvalidateRegionForImages(images_to_invalidate);

  // Note that it is important to push the state for checkerboarded and animated
  // images prior to PrepareTiles here when committing to the active tree. This
  // is because new tiles on the active tree depend on tree specific state
  // cached in these components, which must be pushed to active before preparing
  // tiles for the updated active tree.
  if (CommitToActiveTree())
    ActivateStateForImages();

  if (!paint_worklet_painter_) {
    // Blink should not send us any PaintWorklet inputs until we have a painter
    // registered.
    DCHECK(sync_tree()->picture_layers_with_paint_worklets().empty());
    pending_tree_fully_painted_ = true;
    NotifyPendingTreeFullyPainted();
    return;
  }

  if (!dirty_paint_worklets.size()) {
    pending_tree_fully_painted_ = true;
    NotifyPendingTreeFullyPainted();
    return;
  }

  client_->NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState::PROCESSING);
  auto done_callback = base::BindOnce(
      &LayerTreeHostImpl::OnPaintWorkletResultsReady, base::Unretained(this));
  paint_worklet_painter_->DispatchWorklets(std::move(dirty_paint_worklets),
                                           std::move(done_callback));
}

PaintWorkletJobMap LayerTreeHostImpl::GatherDirtyPaintWorklets(
    PaintImageIdFlatSet* dirty_paint_worklet_ids) const {
  PaintWorkletJobMap dirty_paint_worklets;
  for (PictureLayerImpl* layer :
       sync_tree()->picture_layers_with_paint_worklets()) {
    for (const auto& entry : layer->GetPaintWorkletRecordMap()) {
      const scoped_refptr<const PaintWorkletInput>& input = entry.first;
      const PaintImage::Id& paint_image_id = entry.second.first;
      const sk_sp<PaintRecord>& record = entry.second.second;
      // If we already have a record we can reuse it and so the
      // PaintWorkletInput isn't dirty.
      if (record)
        continue;

      // Mark this PaintWorklet as needing invalidation.
      dirty_paint_worklet_ids->insert(paint_image_id);

      // Create an entry in the appropriate PaintWorkletJobVector for this dirty
      // PaintWorklet.
      int worklet_id = input->WorkletId();
      auto& job_vector = dirty_paint_worklets[worklet_id];
      if (!job_vector)
        job_vector = base::MakeRefCounted<PaintWorkletJobVector>();

      PaintWorkletJob::AnimatedPropertyValues animated_property_values;
      for (const auto& element : input->GetPropertyKeys()) {
        // We should not have multiple property ids with the same name.
        DCHECK(!animated_property_values.contains(element.first));
        const PaintWorkletInput::PropertyValue& animated_property_value =
            paint_worklet_tracker_.GetPropertyAnimationValue(element);
        // No value indicates that the input property was not mutated by CC
        // animation.
        if (animated_property_value.has_value()) {
          animated_property_values.emplace(element.first,
                                           animated_property_value);
        }
      }

      job_vector->data.emplace_back(layer->id(), input,
                                    std::move(animated_property_values));
    }
  }
  return dirty_paint_worklets;
}

void LayerTreeHostImpl::OnPaintWorkletResultsReady(PaintWorkletJobMap results) {
#if DCHECK_IS_ON()
  // Nothing else should have painted the PaintWorklets while we were waiting,
  // and the results should have painted every PaintWorklet, so these should be
  // the same.
  PaintImageIdFlatSet dirty_paint_worklet_ids;
  DCHECK_EQ(results.size(),
            GatherDirtyPaintWorklets(&dirty_paint_worklet_ids).size());
#endif

  for (const auto& entry : results) {
    for (const PaintWorkletJob& job : entry.second->data) {
      LayerImpl* layer_impl =
          pending_tree_->FindPendingTreeLayerById(job.layer_id());
      // Painting the pending tree occurs asynchronously but stalls the pending
      // tree pipeline, so nothing should have changed while we were doing that.
      DCHECK(layer_impl);
      static_cast<PictureLayerImpl*>(layer_impl)
          ->SetPaintWorkletRecord(job.input(), job.output());
    }
  }

  // While the pending tree is being painted by PaintWorklets, we restrict the
  // tiles the TileManager is able to see. This may cause the TileManager to
  // believe that it has finished rastering all the necessary tiles. When we
  // finish painting the tree and release all the tiles, we need to mark the
  // tile priorities as dirty so that the TileManager logic properly re-runs.
  tile_priorities_dirty_ = true;

  // Set the painted state before calling the scheduler, to ensure any callback
  // running as a result sees the correct painted state.
  pending_tree_fully_painted_ = true;
  client_->NotifyPaintWorkletStateChange(Scheduler::PaintWorkletState::IDLE);

  // The pending tree may have been force activated from the signal to the
  // scheduler above, in which case there is no longer a tree to paint.
  if (pending_tree_)
    NotifyPendingTreeFullyPainted();
}

void LayerTreeHostImpl::NotifyPendingTreeFullyPainted() {
  // The pending tree must be fully painted at this point.
  DCHECK(pending_tree_fully_painted_);

  // Nobody should claim the pending tree is fully painted if there is an
  // ongoing dispatch.
  DCHECK(!paint_worklet_painter_ ||
         !paint_worklet_painter_->HasOngoingDispatch());

  // Start working on newly created tiles immediately if needed.
  // TODO(vmpstr): Investigate always having PrepareTiles issue
  // NotifyReadyToActivate, instead of handling it here.
  bool did_prepare_tiles = PrepareTiles();
  if (!did_prepare_tiles) {
    NotifyReadyToActivate();

    // Ensure we get ReadyToDraw signal even when PrepareTiles not run. This
    // is important for SingleThreadProxy and impl-side painting case. For
    // STP, we commit to active tree and RequiresHighResToDraw, and set
    // Scheduler to wait for ReadyToDraw signal to avoid Checkerboard.
    if (CommitToActiveTree())
      NotifyReadyToDraw();
  } else if (!CommitToActiveTree()) {
    DCHECK(!pending_tree_raster_duration_timer_);
    pending_tree_raster_duration_timer_ =
        std::make_unique<PendingTreeRasterDurationHistogramTimer>();
  }
}

bool LayerTreeHostImpl::CanDraw() const {
  // Note: If you are changing this function or any other function that might
  // affect the result of CanDraw, make sure to call
  // client_->OnCanDrawStateChanged in the proper places and update the
  // NotifyIfCanDrawChanged test.

  if (!layer_tree_frame_sink_) {
    TRACE_EVENT_INSTANT0("cc",
                         "LayerTreeHostImpl::CanDraw no LayerTreeFrameSink",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // TODO(boliu): Make draws without layers work and move this below
  // |resourceless_software_draw_| check. Tracked in crbug.com/264967.
  if (active_tree_->LayerListIsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no root layer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (resourceless_software_draw_)
    return true;

  if (active_tree_->GetDeviceViewport().IsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw empty viewport",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (EvictedUIResourcesExist()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw UI resources evicted not recreated",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return true;
}

void LayerTreeHostImpl::AnimatePendingTreeAfterCommit() {
  // Animate the pending tree layer animations to put them at initial positions
  // and starting state. There is no need to run other animations on pending
  // tree because they depend on user inputs so the state is identical to what
  // the active tree has.
  AnimateLayers(CurrentBeginFrameArgs().frame_time, /* is_active_tree */ false);
}

void LayerTreeHostImpl::Animate() {
  AnimateInternal();
}

void LayerTreeHostImpl::AnimateInternal() {
  DCHECK(task_runner_provider_->IsImplThread());
  base::TimeTicks monotonic_time = CurrentBeginFrameArgs().frame_time;

  // mithro(TODO): Enable these checks.
  // DCHECK(!current_begin_frame_tracker_.HasFinished());
  // DCHECK(monotonic_time == current_begin_frame_tracker_.Current().frame_time)
  //  << "Called animate with unknown frame time!?";

  bool did_animate = false;

  if (input_handler_client_) {
    // This animates fling scrolls. But on Android WebView root flings are
    // controlled by the application, so the compositor does not animate them.
    bool ignore_fling =
        settings_.ignore_root_layer_flings && IsCurrentlyScrollingViewport();
    if (!ignore_fling) {
      // This does not set did_animate, because if the InputHandlerClient
      // changes anything it will be through the InputHandler interface which
      // does SetNeedsRedraw.
      input_handler_client_->Animate(monotonic_time);
    }
  }

  did_animate |= AnimatePageScale(monotonic_time);
  did_animate |= AnimateLayers(monotonic_time, /* is_active_tree */ true);
  did_animate |= AnimateScrollbars(monotonic_time);
  did_animate |= AnimateBrowserControls(monotonic_time);

  // Animating stuff can change the root scroll offset, so inform the
  // synchronous input handler.
  UpdateRootLayerStateForSynchronousInputHandler();
  if (did_animate) {
    // If the tree changed, then we want to draw at the end of the current
    // frame.
    SetNeedsRedraw();
  }
}

bool LayerTreeHostImpl::PrepareTiles() {
  if (!tile_priorities_dirty_)
    return false;

  client_->WillPrepareTiles();
  bool did_prepare_tiles = tile_manager_.PrepareTiles(global_tile_state_);
  if (did_prepare_tiles)
    tile_priorities_dirty_ = false;
  client_->DidPrepareTiles();
  return did_prepare_tiles;
}

void LayerTreeHostImpl::StartPageScaleAnimation(
    const gfx::Vector2d& target_offset,
    bool anchor_point,
    float page_scale,
    base::TimeDelta duration) {
  // Temporary crash logging for https://crbug.com/845097.
  static bool has_dumped_without_crashing = false;
  if (settings().is_layer_tree_for_subframe && !has_dumped_without_crashing) {
    has_dumped_without_crashing = true;
    static auto* psf_oopif_animation_error =
        base::debug::AllocateCrashKeyString("psf_oopif_animation_error",
                                            base::debug::CrashKeySize::Size32);
    base::debug::SetCrashKeyString(
        psf_oopif_animation_error,
        base::StringPrintf("%p", InnerViewportScrollNode()));
    base::debug::DumpWithoutCrashing();
  }

  if (!InnerViewportScrollNode())
    return;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();
  gfx::SizeF scrollable_size = active_tree_->ScrollableSize();
  gfx::SizeF viewport_size(
      active_tree_->InnerViewportScrollNode()->container_bounds);

  // TODO(miletus) : Pass in ScrollOffset.
  page_scale_animation_ =
      PageScaleAnimation::Create(ScrollOffsetToVector2dF(scroll_total),
                                 active_tree_->current_page_scale_factor(),
                                 viewport_size, scrollable_size);

  if (anchor_point) {
    gfx::Vector2dF anchor(target_offset);
    page_scale_animation_->ZoomWithAnchor(anchor, page_scale,
                                          duration.InSecondsF());
  } else {
    gfx::Vector2dF scaled_target_offset = target_offset;
    page_scale_animation_->ZoomTo(scaled_target_offset, page_scale,
                                  duration.InSecondsF());
  }

  SetNeedsOneBeginImplFrame();
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetNeedsAnimateInput() {
  DCHECK(!IsCurrentlyScrollingViewport() ||
         !settings_.ignore_root_layer_flings);
  SetNeedsOneBeginImplFrame();
}

bool LayerTreeHostImpl::IsCurrentlyScrollingViewport() const {
  auto* node = CurrentlyScrollingNode();
  if (!node)
    return false;
  return viewport().ShouldScroll(*node);
}

EventListenerProperties LayerTreeHostImpl::GetEventListenerProperties(
    EventListenerClass event_class) const {
  return active_tree_->event_listener_properties(event_class);
}

// Return true if scrollable node for 'ancestor' is the same as 'child' or an
// ancestor along the scroll tree.
bool LayerTreeHostImpl::IsScrolledBy(LayerImpl* child, ScrollNode* ancestor) {
  DCHECK(ancestor && ancestor->scrollable);
  if (!child)
    return false;
  DCHECK_EQ(child->layer_tree_impl(), active_tree_.get());
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  for (ScrollNode* scroll_node = scroll_tree.Node(child->scroll_tree_index());
       scroll_node; scroll_node = scroll_tree.parent(scroll_node)) {
    if (scroll_node->id == ancestor->id)
      return true;
  }
  return false;
}

InputHandler::TouchStartOrMoveEventListenerType
LayerTreeHostImpl::EventListenerTypeForTouchStartOrMoveAt(
    const gfx::Point& viewport_point,
    TouchAction* out_touch_action) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  LayerImpl* layer_impl_with_touch_handler =
      active_tree_->FindLayerThatIsHitByPointInTouchHandlerRegion(
          device_viewport_point);

  if (layer_impl_with_touch_handler == nullptr) {
    if (out_touch_action)
      *out_touch_action = TouchAction::kAuto;
    return InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
  }

  if (out_touch_action) {
    gfx::Transform layer_screen_space_transform =
        layer_impl_with_touch_handler->ScreenSpaceTransform();
    gfx::Transform inverse_layer_screen_space(
        gfx::Transform::kSkipInitialization);
    bool can_be_inversed =
        layer_screen_space_transform.GetInverse(&inverse_layer_screen_space);
    // Getting here indicates that |layer_impl_with_touch_handler| is non-null,
    // which means that the |hit| in FindClosestMatchingLayer() is true, which
    // indicates that the inverse is available.
    DCHECK(can_be_inversed);
    bool clipped = false;
    gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
        inverse_layer_screen_space, device_viewport_point, &clipped);
    gfx::PointF hit_test_point_in_layer_space =
        gfx::PointF(planar_point.x(), planar_point.y());
    const auto& region = layer_impl_with_touch_handler->touch_action_region();
    gfx::Point point = gfx::ToRoundedPoint(hit_test_point_in_layer_space);
    *out_touch_action = region.GetAllowedTouchAction(point);
  }

  if (!CurrentlyScrollingNode())
    return InputHandler::TouchStartOrMoveEventListenerType::HANDLER;

  // Check if the touch start (or move) hits on the current scrolling layer or
  // its descendant. layer_impl_with_touch_handler is the layer hit by the
  // pointer and has an event handler, otherwise it is null. We want to compare
  // the most inner layer we are hitting on which may not have an event listener
  // with the actual scrolling layer.
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);
  bool is_ancestor = IsScrolledBy(layer_impl, CurrentlyScrollingNode());
  return is_ancestor ? InputHandler::TouchStartOrMoveEventListenerType::
                           HANDLER_ON_SCROLLING_LAYER
                     : InputHandler::TouchStartOrMoveEventListenerType::HANDLER;
}

bool LayerTreeHostImpl::HasBlockingWheelEventHandlerAt(
    const gfx::Point& viewport_point) const {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  LayerImpl* layer_impl_with_wheel_event_handler =
      active_tree_->FindLayerThatIsHitByPointInWheelEventHandlerRegion(
          device_viewport_point);

  return layer_impl_with_wheel_event_handler;
}

std::unique_ptr<SwapPromiseMonitor>
LayerTreeHostImpl::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return base::WrapUnique(
      new LatencyInfoSwapPromiseMonitor(latency, nullptr, this));
}

std::unique_ptr<EventsMetricsManager::ScopedMonitor>
LayerTreeHostImpl::GetScopedEventMetricsMonitor(
    std::unique_ptr<EventMetrics> event_metrics) {
  return events_metrics_manager_.GetScopedMonitor(std::move(event_metrics));
}

ScrollElasticityHelper* LayerTreeHostImpl::CreateScrollElasticityHelper() {
  DCHECK(!scroll_elasticity_helper_);
  if (settings_.enable_elastic_overscroll) {
    scroll_elasticity_helper_.reset(
        ScrollElasticityHelper::CreateForLayerTreeHostImpl(this));
  }
  return scroll_elasticity_helper_.get();
}

bool LayerTreeHostImpl::GetScrollOffsetForLayer(ElementId element_id,
                                                gfx::ScrollOffset* offset) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  if (!scroll_node)
    return false;
  *offset = scroll_tree.current_scroll_offset(element_id);
  return true;
}

bool LayerTreeHostImpl::ScrollLayerTo(ElementId element_id,
                                      const gfx::ScrollOffset& offset) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.FindNodeFromElementId(element_id);
  if (!scroll_node)
    return false;

  scroll_tree.ScrollBy(
      *scroll_node,
      ScrollOffsetToVector2dF(offset -
                              scroll_tree.current_scroll_offset(element_id)),
      active_tree());
  return true;
}

bool LayerTreeHostImpl::ScrollingShouldSwitchtoMainThread() {
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();

  if (!scroll_node)
    return true;

  for (; scroll_tree.parent(scroll_node);
       scroll_node = scroll_tree.parent(scroll_node)) {
    if (!!scroll_node->main_thread_scrolling_reasons)
      return true;
  }

  return false;
}

void LayerTreeHostImpl::NotifyInputEvent() {
  frame_rate_estimator_.NotifyInputEvent();
}

void LayerTreeHostImpl::QueueSwapPromiseForMainThreadScrollUpdate(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promises_for_main_thread_scroll_update_.push_back(
      std::move(swap_promise));
}

void LayerTreeHostImpl::FrameData::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetBoolean("has_no_damage", has_no_damage);

  // Quad data can be quite large, so only dump render passes if we are
  // logging verbosely or viz.quads tracing category is enabled.
  bool quads_enabled = VLOG_IS_ON(3);
  if (!quads_enabled) {
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.quads"),
                                       &quads_enabled);
  }
  if (quads_enabled) {
    value->BeginArray("render_passes");
    for (size_t i = 0; i < render_passes.size(); ++i) {
      value->BeginDictionary();
      render_passes[i]->AsValueInto(value);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

std::string LayerTreeHostImpl::FrameData::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

DrawMode LayerTreeHostImpl::GetDrawMode() const {
  if (resourceless_software_draw_) {
    return DRAW_MODE_RESOURCELESS_SOFTWARE;
  } else if (layer_tree_frame_sink_->context_provider()) {
    return DRAW_MODE_HARDWARE;
  } else {
    return DRAW_MODE_SOFTWARE;
  }
}

static void AppendQuadsToFillScreen(
    viz::RenderPass* target_render_pass,
    const RenderSurfaceImpl* root_render_surface,
    SkColor screen_background_color,
    const Region& fill_region) {
  if (!root_render_surface || !SkColorGetA(screen_background_color))
    return;
  if (fill_region.IsEmpty())
    return;

  // Manually create the quad state for the gutter quads, as the root layer
  // doesn't have any bounds and so can't generate this itself.
  // TODO(danakj): Make the gutter quads generated by the solid color layer
  // (make it smarter about generating quads to fill unoccluded areas).

  gfx::Rect root_target_rect = root_render_surface->content_rect();
  float opacity = 1.f;
  int sorting_context_id = 0;
  bool are_contents_opaque = SkColorGetA(screen_background_color) == 0xFF;
  viz::SharedQuadState* shared_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(), root_target_rect,
                            root_target_rect, gfx::RRectF(), root_target_rect,
                            false, are_contents_opaque, opacity,
                            SkBlendMode::kSrcOver, sorting_context_id);

  for (gfx::Rect screen_space_rect : fill_region) {
    gfx::Rect visible_screen_space_rect = screen_space_rect;
    // Skip the quad culler and just append the quads directly to avoid
    // occlusion checks.
    auto* quad =
        target_render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, screen_space_rect,
                 visible_screen_space_rect, screen_background_color, false);
  }
}

static viz::RenderPass* FindRenderPassById(const viz::RenderPassList& list,
                                           viz::RenderPassId id) {
  auto it = std::find_if(
      list.begin(), list.end(),
      [id](const std::unique_ptr<viz::RenderPass>& p) { return p->id == id; });
  return it == list.end() ? nullptr : it->get();
}

bool LayerTreeHostImpl::HasDamage() const {
  DCHECK(!active_tree()->needs_update_draw_properties());
  DCHECK(CanDraw());

  // When touch handle visibility changes there is no visible damage
  // because touch handles are composited in the browser. However we
  // still want the browser to be notified that the handles changed
  // through the |ViewHostMsg_SwapCompositorFrame| IPC so we keep
  // track of handle visibility changes here.
  if (active_tree()->HandleVisibilityChanged())
    return true;

  if (!viewport_damage_rect_.IsEmpty())
    return true;

  // If the set of referenced surfaces has changed then we must submit a new
  // CompositorFrame to update surface references.
  if (last_draw_referenced_surfaces_ != active_tree()->SurfaceRanges())
    return true;

  // If we have a new LocalSurfaceId, we must always submit a CompositorFrame
  // because the parent is blocking on us.
  if (last_draw_local_surface_id_allocation_ !=
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation()) {
    return true;
  }

  const LayerTreeImpl* active_tree = active_tree_.get();

  // If the root render surface has no visible damage, then don't generate a
  // frame at all.
  const RenderSurfaceImpl* root_surface = active_tree->RootRenderSurface();
  bool root_surface_has_visible_damage =
      root_surface->GetDamageRect().Intersects(root_surface->content_rect());
  bool hud_wants_to_draw_ = active_tree->hud_layer() &&
                            active_tree->hud_layer()->IsAnimatingHUDContents();

  return root_surface_has_visible_damage ||
         active_tree_->property_trees()->effect_tree.HasCopyRequests() ||
         hud_wants_to_draw_;
}

DrawResult LayerTreeHostImpl::CalculateRenderPasses(FrameData* frame) {
  DCHECK(frame->render_passes.empty());
  DCHECK(CanDraw());
  DCHECK(!active_tree_->LayerListIsEmpty());

  // For now, we use damage tracking to compute a global scissor. To do this, we
  // must compute all damage tracking before drawing anything, so that we know
  // the root damage rect. The root damage rect is then used to scissor each
  // surface.
  DamageTracker::UpdateDamageTracking(active_tree_.get());

  if (HasDamage()) {
    consecutive_frame_with_damage_count_++;
  } else {
    TRACE_EVENT0("cc",
                 "LayerTreeHostImpl::CalculateRenderPasses::EmptyDamageRect");
    frame->has_no_damage = true;
    DCHECK(!resourceless_software_draw_);
    consecutive_frame_with_damage_count_ = 0;
    return DRAW_SUCCESS;
  }

  TRACE_EVENT_BEGIN2("cc,benchmark", "LayerTreeHostImpl::CalculateRenderPasses",
                     "render_surface_list.size()",
                     static_cast<uint64_t>(frame->render_surface_list->size()),
                     "RequiresHighResToDraw", RequiresHighResToDraw());

  // HandleVisibilityChanged contributed to the above damage check, so reset it
  // now that we're going to draw.
  // TODO(jamwalla): only call this if we are sure the frame draws. Tracked in
  // crbug.com/805673.
  active_tree_->ResetHandleVisibilityChanged();

  // Create the render passes in dependency order.
  size_t render_surface_list_size = frame->render_surface_list->size();
  for (size_t i = 0; i < render_surface_list_size; ++i) {
    size_t surface_index = render_surface_list_size - 1 - i;
    RenderSurfaceImpl* render_surface =
        (*frame->render_surface_list)[surface_index];

    bool is_root_surface =
        render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId;
    bool should_draw_into_render_pass =
        is_root_surface || render_surface->contributes_to_drawn_surface() ||
        render_surface->HasCopyRequest() ||
        render_surface->ShouldCacheRenderSurface();
    if (should_draw_into_render_pass)
      frame->render_passes.push_back(render_surface->CreateRenderPass());
  }

  // Damage rects for non-root passes aren't meaningful, so set them to be
  // equal to the output rect.
  for (size_t i = 0; i + 1 < frame->render_passes.size(); ++i) {
    viz::RenderPass* pass = frame->render_passes[i].get();
    pass->damage_rect = pass->output_rect;
  }

  // When we are displaying the HUD, change the root damage rect to cover the
  // entire root surface. This will disable partial-swap/scissor optimizations
  // that would prevent the HUD from updating, since the HUD does not cause
  // damage itself, to prevent it from messing with damage visualizations. Since
  // damage visualizations are done off the LayerImpls and RenderSurfaceImpls,
  // changing the RenderPass does not affect them.
  if (active_tree_->hud_layer()) {
    viz::RenderPass* root_pass = frame->render_passes.back().get();
    root_pass->damage_rect = root_pass->output_rect;
  }

  // Grab this region here before iterating layers. Taking copy requests from
  // the layers while constructing the render passes will dirty the render
  // surface layer list and this unoccluded region, flipping the dirty bit to
  // true, and making us able to query for it without doing
  // UpdateDrawProperties again. The value inside the Region is not actually
  // changed until UpdateDrawProperties happens, so a reference to it is safe.
  const Region& unoccluded_screen_space_region =
      active_tree_->UnoccludedScreenSpaceRegion();

  // Typically when we are missing a texture and use a checkerboard quad, we
  // still draw the frame. However when the layer being checkerboarded is moving
  // due to an impl-animation, we drop the frame to avoid flashing due to the
  // texture suddenly appearing in the future.
  DrawResult draw_result = DRAW_SUCCESS;

  const DrawMode draw_mode = GetDrawMode();

  int num_missing_tiles = 0;
  int num_incomplete_tiles = 0;
  int64_t checkerboarded_no_recording_content_area = 0;
  int64_t checkerboarded_needs_raster_content_area = 0;
  int64_t total_visible_area = 0;
  bool have_copy_request =
      active_tree()->property_trees()->effect_tree.HasCopyRequests();
  bool have_missing_animated_tiles = false;
  int num_of_layers_with_videos = 0;

  // Advance our de-jelly state. This is a no-op if de-jelly is not active.
  de_jelly_state_.AdvanceFrame(active_tree_.get());

  for (EffectTreeLayerListIterator it(active_tree());
       it.state() != EffectTreeLayerListIterator::State::END; ++it) {
    auto target_render_pass_id = it.target_render_surface()->id();
    viz::RenderPass* target_render_pass =
        FindRenderPassById(frame->render_passes, target_render_pass_id);

    AppendQuadsData append_quads_data;

    if (it.state() == EffectTreeLayerListIterator::State::TARGET_SURFACE) {
      RenderSurfaceImpl* render_surface = it.target_render_surface();
      if (render_surface->HasCopyRequest()) {
        active_tree()
            ->property_trees()
            ->effect_tree.TakeCopyRequestsAndTransformToSurface(
                render_surface->EffectTreeIndex(),
                &target_render_pass->copy_requests);
      }
    } else if (it.state() ==
               EffectTreeLayerListIterator::State::CONTRIBUTING_SURFACE) {
      RenderSurfaceImpl* render_surface = it.current_render_surface();
      if (render_surface->contributes_to_drawn_surface()) {
        render_surface->AppendQuads(draw_mode, target_render_pass,
                                    &append_quads_data);
        if (settings_.allow_de_jelly_effect) {
          de_jelly_state_.UpdateSharedQuadState(
              active_tree_.get(), render_surface->TransformTreeIndex(),
              target_render_pass);
        }
      }
    } else if (it.state() == EffectTreeLayerListIterator::State::LAYER) {
      LayerImpl* layer = it.current_layer();
      if (layer->WillDraw(draw_mode, &resource_provider_)) {
        DCHECK_EQ(active_tree_.get(), layer->layer_tree_impl());

        frame->will_draw_layers.push_back(layer);
        if (layer->may_contain_video()) {
          num_of_layers_with_videos++;
          frame->may_contain_video = true;
        }

        layer->AppendQuads(target_render_pass, &append_quads_data);
        if (settings_.allow_de_jelly_effect) {
          de_jelly_state_.UpdateSharedQuadState(active_tree_.get(),
                                                layer->transform_tree_index(),
                                                target_render_pass);
        }
      }

      rendering_stats_instrumentation_->AddVisibleContentArea(
          append_quads_data.visible_layer_area);
      rendering_stats_instrumentation_->AddApproximatedVisibleContentArea(
          append_quads_data.approximated_visible_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedVisibleContentArea(
          append_quads_data.checkerboarded_visible_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedNoRecordingContentArea(
          append_quads_data.checkerboarded_no_recording_content_area);
      rendering_stats_instrumentation_->AddCheckerboardedNeedsRasterContentArea(
          append_quads_data.checkerboarded_needs_raster_content_area);

      num_missing_tiles += append_quads_data.num_missing_tiles;
      num_incomplete_tiles += append_quads_data.num_incomplete_tiles;
      checkerboarded_no_recording_content_area +=
          append_quads_data.checkerboarded_no_recording_content_area;
      checkerboarded_needs_raster_content_area +=
          append_quads_data.checkerboarded_needs_raster_content_area;
      total_visible_area += append_quads_data.visible_layer_area;
      if (append_quads_data.num_missing_tiles > 0) {
        have_missing_animated_tiles |=
            layer->screen_space_transform_is_animating();
      }
    }
    frame->activation_dependencies.insert(
        frame->activation_dependencies.end(),
        append_quads_data.activation_dependencies.begin(),
        append_quads_data.activation_dependencies.end());
    if (append_quads_data.deadline_in_frames) {
      if (!frame->deadline_in_frames) {
        frame->deadline_in_frames = append_quads_data.deadline_in_frames;
      } else {
        frame->deadline_in_frames = std::max(
            *frame->deadline_in_frames, *append_quads_data.deadline_in_frames);
      }
    }
    frame->use_default_lower_bound_deadline |=
        append_quads_data.use_default_lower_bound_deadline;
  }

  // If CommitToActiveTree() is true, then we wait to draw until
  // NotifyReadyToDraw. That means we're in as good shape as is possible now,
  // so there's no reason to stop the draw now (and this is not supported by
  // SingleThreadProxy).
  if (have_missing_animated_tiles && !CommitToActiveTree())
    draw_result = DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;

  // When we require high res to draw, abort the draw (almost) always. This does
  // not cause the scheduler to do a main frame, instead it will continue to try
  // drawing until we finally complete, so the copy request will not be lost.
  // TODO(weiliangc): Remove RequiresHighResToDraw. crbug.com/469175
  if (num_incomplete_tiles || num_missing_tiles) {
    if (RequiresHighResToDraw())
      draw_result = DRAW_ABORTED_MISSING_HIGH_RES_CONTENT;
  }

  // Only enable frame rate estimation if it would help lower the composition
  // rate for videos.
  const bool enable_frame_rate_estimation = num_of_layers_with_videos > 1;
  frame_rate_estimator_.SetFrameEstimationEnabled(enable_frame_rate_estimation);

  // When doing a resourceless software draw, we don't have control over the
  // surface the compositor draws to, so even though the frame may not be
  // complete, the previous frame has already been potentially lost, so an
  // incomplete frame is better than nothing, so this takes highest precidence.
  if (resourceless_software_draw_)
    draw_result = DRAW_SUCCESS;

#if DCHECK_IS_ON()
  for (const auto& render_pass : frame->render_passes) {
    for (auto* quad : render_pass->quad_list)
      DCHECK(quad->shared_quad_state);
  }
  DCHECK_EQ(frame->render_passes.back()->output_rect.origin(),
            active_tree_->GetDeviceViewport().origin());
#endif
  bool has_transparent_background =
      SkColorGetA(active_tree_->background_color()) != SK_AlphaOPAQUE;
  auto* root_render_surface = active_tree_->RootRenderSurface();
  if (root_render_surface && !has_transparent_background) {
    frame->render_passes.back()->has_transparent_background = false;

    // If any tiles are missing, then fill behind the entire root render
    // surface.  This is a workaround for this edge case, instead of tracking
    // individual tiles that are missing.
    Region fill_region = unoccluded_screen_space_region;
    if (num_missing_tiles > 0)
      fill_region = root_render_surface->content_rect();

    AppendQuadsToFillScreen(frame->render_passes.back().get(),
                            root_render_surface,
                            active_tree_->background_color(), fill_region);
  }

  RemoveRenderPasses(frame);
  // If we're making a frame to draw, it better have at least one render pass.
  DCHECK(!frame->render_passes.empty());

  if (have_copy_request) {
    // Any copy requests left in the tree are not going to get serviced, and
    // should be aborted.
    active_tree()->property_trees()->effect_tree.ClearCopyRequests();

    // Draw properties depend on copy requests.
    active_tree()->set_needs_update_draw_properties();
  }

  frame->has_missing_content =
      num_missing_tiles > 0 || num_incomplete_tiles > 0;

  if (ukm_manager_) {
    ukm_manager_->AddCheckerboardStatsForFrame(
        checkerboarded_no_recording_content_area +
            checkerboarded_needs_raster_content_area,
        num_missing_tiles, total_visible_area);
  }

  if (active_tree_->has_ever_been_drawn()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumMissingTiles",
        num_missing_tiles);
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumIncompleteTiles",
        num_incomplete_tiles);
    UMA_HISTOGRAM_COUNTS_1M(
        "Compositing.RenderPass.AppendQuadData."
        "CheckerboardedNoRecordingContentArea",
        checkerboarded_no_recording_content_area);
    UMA_HISTOGRAM_COUNTS_1M(
        "Compositing.RenderPass.AppendQuadData."
        "CheckerboardedNeedRasterContentArea",
        checkerboarded_needs_raster_content_area);
  }

  TRACE_EVENT_END2("cc,benchmark", "LayerTreeHostImpl::CalculateRenderPasses",
                   "draw_result", draw_result, "missing tiles",
                   num_missing_tiles);

  // Draw has to be successful to not drop the copy request layer.
  // When we have a copy request for a layer, we need to draw even if there
  // would be animating checkerboards, because failing under those conditions
  // triggers a new main frame, which may cause the copy request layer to be
  // destroyed.
  // TODO(weiliangc): Test copy request w/ LayerTreeFrameSink recreation. Would
  // trigger this DCHECK.
  DCHECK(!have_copy_request || draw_result == DRAW_SUCCESS);

  // TODO(crbug.com/564832): This workaround to prevent creating unnecessarily
  // persistent render passes. When a copy request is made, it may force a
  // separate render pass for the layer, which will persist until a new commit
  // removes it. Force a commit after copy requests, to remove extra render
  // passes.
  if (have_copy_request)
    client_->SetNeedsCommitOnImplThread();

  return draw_result;
}

void LayerTreeHostImpl::DidAnimateScrollOffset() {
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetViewportDamage(const gfx::Rect& damage_rect) {
  viewport_damage_rect_.Union(damage_rect);
}

void LayerTreeHostImpl::UpdateElements(ElementListType changed_list) {
  mutator_host()->UpdateRegisteredElementIds(changed_list);
}

void LayerTreeHostImpl::InvalidateContentOnImplSide() {
  DCHECK(!pending_tree_);
  // Invalidation should never be ran outside the impl frame for non
  // synchronous compositor mode. For devices that use synchronous compositor,
  // e.g. Android Webview, the assertion is not guaranteed because it may ask
  // for a frame at any time.
  DCHECK(impl_thread_phase_ == ImplThreadPhase::INSIDE_IMPL_FRAME ||
         settings_.using_synchronous_renderer_compositor);

  if (!CommitToActiveTree())
    CreatePendingTree();

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
}

void LayerTreeHostImpl::InvalidateLayerTreeFrameSink(bool needs_redraw) {
  DCHECK(layer_tree_frame_sink());

  layer_tree_frame_sink()->Invalidate(needs_redraw);
}

DrawResult LayerTreeHostImpl::PrepareToDraw(FrameData* frame) {
  TRACE_EVENT1("cc", "LayerTreeHostImpl::PrepareToDraw", "SourceFrameNumber",
               active_tree_->source_frame_number());
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(CurrentBeginFrameArgs().trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "GenerateRenderPass");
  if (input_handler_client_)
    input_handler_client_->ReconcileElasticOverscrollAndRootScroll();

  // |client_name| is used for various UMA histograms below.
  // GetClientNameForMetrics only returns one non-null value over the lifetime
  // of the process, so the histogram names are runtime constant.
  const char* client_name = GetClientNameForMetrics();
  if (client_name) {
    size_t total_gpu_memory_for_tilings_in_bytes = 0;
    for (const PictureLayerImpl* layer : active_tree()->picture_layers())
      total_gpu_memory_for_tilings_in_bytes += layer->GPUMemoryUsageInBytes();

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        base::StringPrintf("Compositing.%s.NumActiveLayers", client_name),
        base::saturated_cast<int>(active_tree_->NumLayers()), 1, 400, 20);

    UMA_HISTOGRAM_CUSTOM_COUNTS(
        base::StringPrintf("Compositing.%s.NumActivePictureLayers",
                           client_name),
        base::saturated_cast<int>(active_tree_->picture_layers().size()), 1,
        400, 20);

    // TODO(yigu): Maybe we should use the same check above. Need to figure out
    // why exactly we skip 0.
    if (!active_tree()->picture_layers().empty()) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          base::StringPrintf("Compositing.%s.GPUMemoryForTilingsInKb",
                             client_name),
          base::saturated_cast<int>(total_gpu_memory_for_tilings_in_bytes /
                                    1024),
          1, kGPUMemoryForTilingsLargestBucketKb,
          kGPUMemoryForTilingsBucketCount);
    }
  }

  // Tick worklet animations here, just before draw, to give animation worklets
  // as much time as possible to produce their output for this frame. Note that
  // an animation worklet is asked to produce its output at the beginning of the
  // frame along side other animations but its output arrives asynchronously so
  // we tick worklet animations and apply that output here instead.
  mutator_host_->TickWorkletAnimations();

  bool ok = active_tree_->UpdateDrawProperties();
  DCHECK(ok) << "UpdateDrawProperties failed during draw";

  // This will cause NotifyTileStateChanged() to be called for any tiles that
  // completed, which will add damage for visible tiles to the frame for them so
  // they appear as part of the current frame being drawn.
  tile_manager_.CheckForCompletedTasks();

  frame->render_surface_list = &active_tree_->GetRenderSurfaceList();
  frame->render_passes.clear();
  frame->will_draw_layers.clear();
  frame->has_no_damage = false;
  frame->may_contain_video = false;

  if (active_tree_->RootRenderSurface()) {
    active_tree_->RootRenderSurface()->damage_tracker()->AddDamageNextUpdate(
        viewport_damage_rect_);
    viewport_damage_rect_ = gfx::Rect();
  }

  DrawResult draw_result = CalculateRenderPasses(frame);

  // Dump render passes and draw quads if run with:
  //   --vmodule=layer_tree_host_impl=3
  if (VLOG_IS_ON(3)) {
    VLOG(3) << "Prepare to draw ("
            << (client_name ? client_name : "<unknown client>") << ")\n"
            << frame->ToString();
  }

  if (draw_result != DRAW_SUCCESS) {
    DCHECK(!resourceless_software_draw_);
    return draw_result;
  }

  // If we return DRAW_SUCCESS, then we expect DrawLayers() to be called before
  // this function is called again.
  return DRAW_SUCCESS;
}

void LayerTreeHostImpl::RemoveRenderPasses(FrameData* frame) {
  // There is always at least a root RenderPass.
  DCHECK_GE(frame->render_passes.size(), 1u);

  // A set of RenderPasses that we have seen.
  base::flat_set<viz::RenderPassId> pass_exists;
  // A set of viz::RenderPassDrawQuads that we have seen (stored by the
  // RenderPasses they refer to).
  base::flat_map<viz::RenderPassId, int> pass_references;

  // Iterate RenderPasses in draw order, removing empty render passes (except
  // the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size(); ++i) {
    viz::RenderPass* pass = frame->render_passes[i].get();

    // Remove orphan viz::RenderPassDrawQuads.
    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end();) {
      if (it->material != viz::DrawQuad::Material::kRenderPass) {
        ++it;
        continue;
      }
      const viz::RenderPassDrawQuad* quad =
          viz::RenderPassDrawQuad::MaterialCast(*it);
      // If the RenderPass doesn't exist, we can remove the quad.
      if (pass_exists.count(quad->render_pass_id)) {
        // Otherwise, save a reference to the RenderPass so we know there's a
        // quad using it.
        pass_references[quad->render_pass_id]++;
        ++it;
      } else {
        it = pass->quad_list.EraseAndInvalidateAllPointers(it);
      }
    }

    if (i == frame->render_passes.size() - 1) {
      // Don't remove the root RenderPass.
      break;
    }

    if (pass->quad_list.empty() && pass->copy_requests.empty() &&
        pass->filters.IsEmpty() && pass->backdrop_filters.IsEmpty()) {
      // Remove the pass and decrement |i| to counter the for loop's increment,
      // so we don't skip the next pass in the loop.
      frame->render_passes.erase(frame->render_passes.begin() + i);
      --i;
      continue;
    }

    pass_exists.insert(pass->id);
  }

  // Remove RenderPasses that are not referenced by any draw quads or copy
  // requests (except the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size() - 1; ++i) {
    // Iterating from the back of the list to the front, skipping over the
    // back-most (root) pass, in order to remove each qualified RenderPass, and
    // drop references to earlier RenderPasses allowing them to be removed to.
    viz::RenderPass* pass =
        frame->render_passes[frame->render_passes.size() - 2 - i].get();
    if (!pass->copy_requests.empty())
      continue;
    if (pass_references[pass->id])
      continue;

    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end(); ++it) {
      if (it->material != viz::DrawQuad::Material::kRenderPass)
        continue;
      const viz::RenderPassDrawQuad* quad =
          viz::RenderPassDrawQuad::MaterialCast(*it);
      pass_references[quad->render_pass_id]--;
    }

    frame->render_passes.erase(frame->render_passes.end() - 2 - i);
    --i;
  }
}

void LayerTreeHostImpl::EvictTexturesForTesting() {
  UpdateTileManagerMemoryPolicy(ManagedMemoryPolicy(0));
}

void LayerTreeHostImpl::BlockNotifyReadyToActivateForTesting(bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::BlockImplSideInvalidationRequestsForTesting(
    bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::ResetTreesForTesting() {
  if (active_tree_)
    active_tree_->DetachLayers();
  active_tree_ = std::make_unique<LayerTreeImpl>(
      this, active_tree()->page_scale_factor(),
      active_tree()->top_controls_shown_ratio(),
      active_tree()->bottom_controls_shown_ratio(),
      active_tree()->elastic_overscroll());
  active_tree_->property_trees()->is_active = true;
  active_tree_->property_trees()->clear();
  if (pending_tree_)
    pending_tree_->DetachLayers();
  pending_tree_ = nullptr;
  if (recycle_tree_)
    recycle_tree_->DetachLayers();
  recycle_tree_ = nullptr;
}

size_t LayerTreeHostImpl::SourceAnimationFrameNumberForTesting() const {
  return fps_counter_->current_frame_number();
}

void LayerTreeHostImpl::UpdateTileManagerMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (!resource_pool_)
    return;

  global_tile_state_.hard_memory_limit_in_bytes = 0;
  global_tile_state_.soft_memory_limit_in_bytes = 0;
  if (visible_ && policy.bytes_limit_when_visible > 0) {
    global_tile_state_.hard_memory_limit_in_bytes =
        policy.bytes_limit_when_visible;
    global_tile_state_.soft_memory_limit_in_bytes =
        (static_cast<int64_t>(global_tile_state_.hard_memory_limit_in_bytes) *
         settings_.max_memory_for_prepaint_percentage) /
        100;
  }
  global_tile_state_.memory_limit_policy =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
          visible_ ? policy.priority_cutoff_when_visible
                   : gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);
  global_tile_state_.num_resources_limit = policy.num_resources_limit;

  if (global_tile_state_.hard_memory_limit_in_bytes > 0) {
    // If |global_tile_state_.hard_memory_limit_in_bytes| is greater than 0, we
    // consider our contexts visible. Notify the contexts here. We handle
    // becoming invisible in NotifyAllTileTasksComplete to avoid interrupting
    // running work.
    SetContextVisibility(true);

    // If |global_tile_state_.hard_memory_limit_in_bytes| is greater than 0, we
    // allow the image decode controller to retain resources. We handle the
    // equal to 0 case in NotifyAllTileTasksComplete to avoid interrupting
    // running work.
    if (image_decode_cache_)
      image_decode_cache_->SetShouldAggressivelyFreeResources(false);
  } else {
    // When the memory policy is set to zero, its important to release any
    // decoded images cached by the tracker. But we can not re-checker any
    // images that have been displayed since the resources, if held by the
    // browser, may be re-used. Which is why its important to maintain the
    // decode policy tracking.
    bool can_clear_decode_policy_tracking = false;
    tile_manager_.ClearCheckerImageTracking(can_clear_decode_policy_tracking);
  }

  DCHECK(resource_pool_);
  // Soft limit is used for resource pool such that memory returns to soft
  // limit after going over.
  resource_pool_->SetResourceUsageLimits(
      global_tile_state_.soft_memory_limit_in_bytes,
      global_tile_state_.num_resources_limit);

  DidModifyTilePriorities();
}

void LayerTreeHostImpl::DidModifyTilePriorities() {
  // Mark priorities as dirty and schedule a PrepareTiles().
  tile_priorities_dirty_ = true;
  tile_manager_.DidModifyTilePriorities();
  client_->SetNeedsPrepareTilesOnImplThread();
}

std::unique_ptr<RasterTilePriorityQueue> LayerTreeHostImpl::BuildRasterQueue(
    TreePriority tree_priority,
    RasterTilePriorityQueue::Type type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::BuildRasterQueue");

  return RasterTilePriorityQueue::Create(
      active_tree_->picture_layers(),
      pending_tree_ && pending_tree_fully_painted_
          ? pending_tree_->picture_layers()
          : std::vector<PictureLayerImpl*>(),
      tree_priority, type);
}

std::unique_ptr<EvictionTilePriorityQueue>
LayerTreeHostImpl::BuildEvictionQueue(TreePriority tree_priority) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::BuildEvictionQueue");

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      new EvictionTilePriorityQueue);
  queue->Build(active_tree_->picture_layers(),
               pending_tree_ ? pending_tree_->picture_layers()
                             : std::vector<PictureLayerImpl*>(),
               tree_priority);
  return queue;
}

void LayerTreeHostImpl::SetIsLikelyToRequireADraw(
    bool is_likely_to_require_a_draw) {
  // Proactively tell the scheduler that we expect to draw within each vsync
  // until we get all the tiles ready to draw. If we happen to miss a required
  // for draw tile here, then we will miss telling the scheduler each frame that
  // we intend to draw so it may make worse scheduling decisions.
  is_likely_to_require_a_draw_ = is_likely_to_require_a_draw;
}

gfx::ColorSpace LayerTreeHostImpl::GetRasterColorSpace(
    gfx::ContentColorUsage content_color_usage) const {
  constexpr gfx::ColorSpace srgb = gfx::ColorSpace::CreateSRGB();

  if (settings_.prefer_raster_in_srgb &&
      content_color_usage == gfx::ContentColorUsage::kSRGB)
    return srgb;

  gfx::ColorSpace result;
  // The pending tree will have the most recently updated color space, so
  // prefer that.
  if (pending_tree_) {
    result = pending_tree_->raster_color_space();
  } else if (active_tree_) {
    result = active_tree_->raster_color_space();
  }

  // If we are likely to software composite the resource, we use sRGB because
  // software compositing is unable to perform color conversion. Also always
  // specify a color space if color correct rasterization is requested
  // (not specifying a color space indicates that no color conversion is
  // required).
  if (!layer_tree_frame_sink_ || !layer_tree_frame_sink_->context_provider() ||
      !result.IsValid()) {
    result = srgb;
  }
  return result;
}

void LayerTreeHostImpl::RequestImplSideInvalidationForCheckerImagedTiles() {
  // When using impl-side invalidation for checker-imaging, a pending tree does
  // not need to be flushed as an independent update through the pipeline.
  bool needs_first_draw_on_activation = false;
  client_->NeedsImplSideInvalidation(needs_first_draw_on_activation);
}

size_t LayerTreeHostImpl::GetFrameIndexForImage(const PaintImage& paint_image,
                                                WhichTree tree) const {
  if (!paint_image.ShouldAnimate())
    return PaintImage::kDefaultFrameIndex;

  return image_animation_controller_.GetFrameIndexForImage(
      paint_image.stable_id(), tree);
}

int LayerTreeHostImpl::GetMSAASampleCountForRaster(
    const scoped_refptr<DisplayItemList>& display_list) {
  constexpr int kMinNumberOfSlowPathsForMSAA = 6;
  if (display_list->NumSlowPaths() < kMinNumberOfSlowPathsForMSAA)
    return 0;

  if (!can_use_msaa_)
    return 0;

  if (display_list->HasNonAAPaint() && !supports_disable_msaa_)
    return 0;

  return RequestedMSAASampleCount();
}

void LayerTreeHostImpl::NotifyReadyToActivate() {
  // The TileManager may call this method while the pending tree is still being
  // painted, as it isn't aware of the ongoing paint. We shouldn't tell the
  // scheduler we are ready to activate in that case, as if we do it will
  // immediately activate once we call NotifyPaintWorkletStateChange, rather
  // than wait for the TileManager to actually raster the content!
  if (!pending_tree_fully_painted_)
    return;
  pending_tree_raster_duration_timer_.reset();
  client_->NotifyReadyToActivate();
}

void LayerTreeHostImpl::NotifyReadyToDraw() {
  // Tiles that are ready will cause NotifyTileStateChanged() to be called so we
  // don't need to schedule a draw here. Just stop WillBeginImplFrame() from
  // causing optimistic requests to draw a frame.
  is_likely_to_require_a_draw_ = false;

  client_->NotifyReadyToDraw();
}

void LayerTreeHostImpl::NotifyAllTileTasksCompleted() {
  // The tile tasks started by the most recent call to PrepareTiles have
  // completed. Now is a good time to free resources if necessary.
  if (global_tile_state_.hard_memory_limit_in_bytes == 0) {
    // Free image decode controller resources before notifying the
    // contexts of visibility change. This ensures that the imaged decode
    // controller has released all Skia refs at the time Skia's cleanup
    // executes (within worker context's cleanup).
    if (image_decode_cache_)
      image_decode_cache_->SetShouldAggressivelyFreeResources(true);
    SetContextVisibility(false);
  }
}

void LayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::NotifyTileStateChanged");

  LayerImpl* layer_impl = nullptr;

  // We must have a pending or active tree layer here, since the layer is
  // guaranteed to outlive its tiles.
  if (tile->tiling()->tree() == WhichTree::PENDING_TREE)
    layer_impl = pending_tree_->FindPendingTreeLayerById(tile->layer_id());
  else
    layer_impl = active_tree_->FindActiveTreeLayerById(tile->layer_id());

  layer_impl->NotifyTileStateChanged(tile);

  if (!client_->IsInsideDraw() && tile->required_for_draw()) {
    // The LayerImpl::NotifyTileStateChanged() should damage the layer, so this
    // redraw will make those tiles be displayed.
    SetNeedsRedraw();
  }
}

void LayerTreeHostImpl::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  DCHECK(task_runner_provider_->IsImplThread());

  SetManagedMemoryPolicy(policy);

  // This is short term solution to synchronously drop tile resources when
  // using synchronous compositing to avoid memory usage regression.
  // TODO(boliu): crbug.com/499004 to track removing this.
  if (!policy.bytes_limit_when_visible && resource_pool_ &&
      settings_.using_synchronous_renderer_compositor) {
    ReleaseTileResources();
    CleanUpTileManagerResources();

    // Force a call to NotifyAllTileTasks completed - otherwise this logic may
    // be skipped if no work was enqueued at the time the tile manager was
    // destroyed.
    NotifyAllTileTasksCompleted();

    CreateTileManagerResources();
    RecreateTileResources();
  }
}

void LayerTreeHostImpl::SetTreeActivationCallback(
    base::RepeatingClosure callback) {
  DCHECK(task_runner_provider_->IsImplThread());
  tree_activation_callback_ = std::move(callback);
}

void LayerTreeHostImpl::SetManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (cached_managed_memory_policy_ == policy)
    return;

  ManagedMemoryPolicy old_policy = ActualManagedMemoryPolicy();
  cached_managed_memory_policy_ = policy;
  ManagedMemoryPolicy actual_policy = ActualManagedMemoryPolicy();

  if (old_policy == actual_policy)
    return;

  UpdateTileManagerMemoryPolicy(actual_policy);

  // If there is already enough memory to draw everything imaginable and the
  // new memory limit does not change this, then do not re-commit. Don't bother
  // skipping commits if this is not visible (commits don't happen when not
  // visible, there will almost always be a commit when this becomes visible).
  bool needs_commit = true;
  if (visible() &&
      actual_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      old_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      actual_policy.priority_cutoff_when_visible ==
          old_policy.priority_cutoff_when_visible) {
    needs_commit = false;
  }

  if (needs_commit)
    client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {
  const bool tile_priority_params_changed =
      viewport_rect_for_tile_priority_ != viewport_rect;
  viewport_rect_for_tile_priority_ = viewport_rect;

  if (tile_priority_params_changed) {
    active_tree_->set_needs_update_draw_properties();
    if (pending_tree_)
      pending_tree_->set_needs_update_draw_properties();

    // Compositor, not LayerTreeFrameSink, is responsible for setting damage
    // and triggering redraw for constraint changes.
    SetFullViewportDamage();
    SetNeedsRedraw();
  }
}

void LayerTreeHostImpl::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAckOnImplThread();
}

void LayerTreeHostImpl::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  frame_trackers_.NotifyFramePresented(frame_token,
                                       details.presentation_feedback);
  PresentationTimeCallbackBuffer::PendingCallbacks activated =
      presentation_time_callbacks_.PopPendingCallbacks(frame_token);

  // The callbacks in |compositor_thread_callbacks| expect to be run on the
  // compositor thread so we'll run them now.
  for (LayerTreeHost::PresentationTimeCallback& callback :
       activated.compositor_thread_callbacks) {
    std::move(callback).Run(details.presentation_feedback);
  }

  // Send all the main-thread callbacks to the client in one batch. The client
  // is in charge of posting them to the main thread.
  client_->DidPresentCompositorFrameOnImplThread(
      frame_token, std::move(activated.main_thread_callbacks), details);

  // Send throughput tracker results to main-thread if any.
  auto throughput_tracker_results = frame_trackers_.TakeCustomTrackerResults();
  if (!throughput_tracker_results.empty()) {
    client_->NotifyThroughputTrackerResults(
        std::move(throughput_tracker_results));
  }
}

void LayerTreeHostImpl::DidNotNeedBeginFrame() {
  frame_trackers_.NotifyPauseFrameProduction();
  if (lcd_text_metrics_reporter_)
    lcd_text_metrics_reporter_->NotifyPauseFrameProduction();
}

void LayerTreeHostImpl::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  resource_provider_.ReceiveReturnsFromParent(resources);

  // In OOM, we now might be able to release more resources that were held
  // because they were exported.
  if (resource_pool_) {
    if (resource_pool_->memory_usage_bytes()) {
      const size_t kMegabyte = 1024 * 1024;

      // This is a good time to log memory usage. A chunk of work has just
      // completed but none of the memory used for that work has likely been
      // freed.
      UMA_HISTOGRAM_MEMORY_MB(
          "Renderer4.ResourcePoolMemoryUsage",
          static_cast<int>(resource_pool_->memory_usage_bytes() / kMegabyte));
    }

    resource_pool_->ReduceResourceUsage();
  }

  // If we're not visible, we likely released resources, so we want to
  // aggressively flush here to make sure those DeleteTextures make it to the
  // GPU process to free up the memory.
  if (!visible_ && layer_tree_frame_sink_->context_provider()) {
    auto* gl = layer_tree_frame_sink_->context_provider()->ContextGL();
    gl->ShallowFlushCHROMIUM();
  }
}

void LayerTreeHostImpl::OnDraw(const gfx::Transform& transform,
                               const gfx::Rect& viewport,
                               bool resourceless_software_draw,
                               bool skip_draw) {
  DCHECK(!resourceless_software_draw_);
  // This function is only ever called by Android WebView, in which case we
  // expect the device viewport to be at the origin. We never expect an
  // external viewport to be set otherwise.
  DCHECK(active_tree_->internal_device_viewport().origin().IsOrigin());

#if DCHECK_IS_ON()
  base::AutoReset<bool> reset_sync_draw(&doing_sync_draw_, true);
#endif

  if (skip_draw) {
    client_->OnDrawForLayerTreeFrameSink(resourceless_software_draw_, true);
    return;
  }

  const bool transform_changed = external_transform_ != transform;
  const bool viewport_changed = external_viewport_ != viewport;

  external_transform_ = transform;
  external_viewport_ = viewport;

  {
    base::AutoReset<bool> resourceless_software_draw_reset(
        &resourceless_software_draw_, resourceless_software_draw);

    // For resourceless software draw, always set full damage to ensure they
    // always swap. Otherwise, need to set redraw for any changes to draw
    // parameters.
    if (transform_changed || viewport_changed || resourceless_software_draw_) {
      SetFullViewportDamage();
      SetNeedsRedraw();
      active_tree_->set_needs_update_draw_properties();
    }

    if (resourceless_software_draw)
      client_->OnCanDrawStateChanged(CanDraw());

    client_->OnDrawForLayerTreeFrameSink(resourceless_software_draw_,
                                         skip_draw);
  }

  if (resourceless_software_draw) {
    active_tree_->set_needs_update_draw_properties();
    client_->OnCanDrawStateChanged(CanDraw());
    // This draw may have reset all damage, which would lead to subsequent
    // incorrect hardware draw, so explicitly set damage for next hardware
    // draw as well.
    SetFullViewportDamage();
  }
}

void LayerTreeHostImpl::OnCanDrawStateChangedForTree() {
  client_->OnCanDrawStateChanged(CanDraw());
}

viz::CompositorFrameMetadata LayerTreeHostImpl::MakeCompositorFrameMetadata() {
  viz::CompositorFrameMetadata metadata;
  metadata.frame_token = ++next_frame_token_;
  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.root_background_color = active_tree_->background_color();

  if (active_tree_->has_presentation_callbacks()) {
    presentation_time_callbacks_.RegisterMainThreadPresentationCallbacks(
        metadata.frame_token, active_tree_->TakePresentationCallbacks());
    presentation_time_callbacks_.RegisterFrameTime(
        metadata.frame_token, CurrentBeginFrameArgs().frame_time);
  }

  if (GetDrawMode() == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    metadata.is_resourceless_software_draw_with_scroll_or_animation =
        IsActivelyPrecisionScrolling() || mutator_host_->NeedsTickAnimations();
  }

  const base::flat_set<viz::SurfaceRange>& referenced_surfaces =
      active_tree_->SurfaceRanges();
  for (auto& surface_range : referenced_surfaces)
    metadata.referenced_surfaces.push_back(surface_range);

  if (last_draw_referenced_surfaces_ != referenced_surfaces)
    last_draw_referenced_surfaces_ = referenced_surfaces;

  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();

  if (browser_controls_offset_manager_->TopControlsHeight() > 0) {
    metadata.top_controls_visible_height.emplace(
        browser_controls_offset_manager_->TopControlsHeight() *
        browser_controls_offset_manager_->TopControlsShownRatio());
  }

  metadata.local_surface_id_allocation_time =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation()
          .allocation_time();

  if (InnerViewportScrollNode()) {
    // TODO(miletus) : Change the metadata to hold ScrollOffset.
    metadata.root_scroll_offset =
        gfx::ScrollOffsetToVector2dF(active_tree_->TotalScrollOffset());
  }

  metadata.display_transform_hint = active_tree_->display_transform_hint();

  return metadata;
}

RenderFrameMetadata LayerTreeHostImpl::MakeRenderFrameMetadata(
    FrameData* frame) {
  RenderFrameMetadata metadata;
  metadata.root_scroll_offset =
      gfx::ScrollOffsetToVector2dF(active_tree_->TotalScrollOffset());

  metadata.root_background_color = active_tree_->background_color();
  metadata.is_scroll_offset_at_top = active_tree_->TotalScrollOffset().y() == 0;
  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();
  active_tree_->GetViewportSelection(&metadata.selection);
  metadata.is_mobile_optimized = IsMobileOptimized(active_tree_.get());
  metadata.viewport_size_in_pixels = active_tree_->GetDeviceViewport().size();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();
  metadata.external_page_scale_factor =
      active_tree_->external_page_scale_factor();

  metadata.top_controls_height =
      browser_controls_offset_manager_->TopControlsHeight();
  metadata.top_controls_shown_ratio =
      browser_controls_offset_manager_->TopControlsShownRatio();
#if defined(OS_ANDROID)
  metadata.bottom_controls_height =
      browser_controls_offset_manager_->BottomControlsHeight();
  metadata.bottom_controls_shown_ratio =
      browser_controls_offset_manager_->BottomControlsShownRatio();
  metadata.top_controls_min_height_offset =
      browser_controls_offset_manager_->TopControlsMinHeightOffset();
  metadata.bottom_controls_min_height_offset =
      browser_controls_offset_manager_->BottomControlsMinHeightOffset();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  metadata.root_layer_size = active_tree_->ScrollableSize();
  if (InnerViewportScrollNode()) {
    DCHECK(OuterViewportScrollNode());
    metadata.root_overflow_y_hidden =
        !OuterViewportScrollNode()->user_scrollable_vertical ||
        !InnerViewportScrollNode()->user_scrollable_vertical;
  }
  metadata.has_transparent_background =
      frame->render_passes.back()->has_transparent_background;
#endif

  if (last_draw_render_frame_metadata_) {
    const float last_root_scroll_offset_y =
        last_draw_render_frame_metadata_->root_scroll_offset
            .value_or(gfx::Vector2dF())
            .y();

    const float new_root_scroll_offset_y =
        metadata.root_scroll_offset.value().y();

    if (!MathUtil::IsWithinEpsilon(last_root_scroll_offset_y,
                                   new_root_scroll_offset_y)) {
      viz::VerticalScrollDirection new_vertical_scroll_direction =
          (last_root_scroll_offset_y < new_root_scroll_offset_y)
              ? viz::VerticalScrollDirection::kDown
              : viz::VerticalScrollDirection::kUp;

      // Changes in vertical scroll direction happen instantaneously. This being
      // the case, a new vertical scroll direction should only be present in the
      // singular metadata for the render frame in which the direction change
      // occurred. If the vertical scroll direction detected here matches that
      // which we've previously cached, then this frame is not the instant in
      // which the direction change occurred and is therefore not propagated.
      if (last_vertical_scroll_direction_ != new_vertical_scroll_direction)
        metadata.new_vertical_scroll_direction = new_vertical_scroll_direction;
    }
  }

  bool allocate_new_local_surface_id =
#if !defined(OS_ANDROID)
      last_draw_render_frame_metadata_ &&
      (last_draw_render_frame_metadata_->top_controls_height !=
           metadata.top_controls_height ||
       last_draw_render_frame_metadata_->top_controls_shown_ratio !=
           metadata.top_controls_shown_ratio);
#else
      last_draw_render_frame_metadata_ &&
      (last_draw_render_frame_metadata_->top_controls_height !=
           metadata.top_controls_height ||
       last_draw_render_frame_metadata_->top_controls_shown_ratio !=
           metadata.top_controls_shown_ratio ||
       last_draw_render_frame_metadata_->bottom_controls_height !=
           metadata.bottom_controls_height ||
       last_draw_render_frame_metadata_->bottom_controls_shown_ratio !=
           metadata.bottom_controls_shown_ratio ||
       last_draw_render_frame_metadata_->selection != metadata.selection ||
       last_draw_render_frame_metadata_->has_transparent_background !=
           metadata.has_transparent_background);
#endif

  if (child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation()
          .IsValid()) {
    if (allocate_new_local_surface_id)
      AllocateLocalSurfaceId();
    metadata.local_surface_id_allocation =
        child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  }

  return metadata;
}

bool LayerTreeHostImpl::DrawLayers(FrameData* frame) {
  DCHECK(CanDraw());
  DCHECK_EQ(frame->has_no_damage, frame->render_passes.empty());
  ResetRequiresHighResToDraw();

  if (frame->has_no_damage) {
    DCHECK(!resourceless_software_draw_);

    frame_trackers_.NotifyImplFrameCausedNoDamage(frame->begin_frame_ack);
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoDamage", TRACE_EVENT_SCOPE_THREAD);
    active_tree()->BreakSwapPromises(SwapPromise::SWAP_FAILS);
    active_tree()->ResetAllChangeTracking();
    return false;
  }

  layer_tree_frame_sink_->set_source_frame_number(
      active_tree_->source_frame_number());

  auto compositor_frame = GenerateCompositorFrame(frame);
  frame->frame_token = compositor_frame.metadata.frame_token;
  layer_tree_frame_sink_->SubmitCompositorFrame(
      std::move(compositor_frame),
      /*hit_test_data_changed=*/false, debug_state_.show_hit_test_borders);

#if DCHECK_IS_ON()
  if (!doing_sync_draw_) {
    // The throughput computation (in |FrameSequenceTracker|) depends on the
    // compositor-frame submission to happen while a BeginFrameArgs is 'active'
    // (i.e. between calls to WillBeginImplFrame() and DidFinishImplFrame()).
    // Verify that this is the case.
    // No begin-frame is available when doing sync draws, so avoid doing this
    // check in that case.
    const auto& bfargs = current_begin_frame_tracker_.Current();
    const auto& ack = compositor_frame.metadata.begin_frame_ack;
    DCHECK_EQ(bfargs.frame_id, ack.frame_id);
  }
#endif

  // In some cases (e.g. for android-webviews), the frame-submission happens
  // outside of begin-impl frame pipeline. Avoid notifying the trackers in such
  // cases.
  if (impl_thread_phase_ == ImplThreadPhase::INSIDE_IMPL_FRAME) {
    frame_trackers_.NotifySubmitFrame(
        compositor_frame.metadata.frame_token, frame->has_missing_content,
        frame->begin_frame_ack, frame->origin_begin_main_frame_args);
  }

  if (!mutator_host_->NextFrameHasPendingRAF())
    frame_trackers_.StopSequence(FrameSequenceTrackerType::kRAF);

  if (mutator_host_->MainThreadAnimationsCount() == 0) {
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kMainThreadAnimation);
  }

  if (lcd_text_metrics_reporter_) {
    lcd_text_metrics_reporter_->NotifySubmitFrame(
        frame->origin_begin_main_frame_args);
  }

  // Clears the list of swap promises after calling DidSwap on each of them to
  // signal that the swap is over.
  active_tree()->ClearSwapPromises();

  // The next frame should start by assuming nothing has changed, and changes
  // are noted as they occur.
  // TODO(boliu): If we did a temporary software renderer frame, propogate the
  // damage forward to the next frame.
  for (size_t i = 0; i < frame->render_surface_list->size(); i++) {
    auto* surface = (*frame->render_surface_list)[i];
    surface->damage_tracker()->DidDrawDamagedArea();
  }
  active_tree_->ResetAllChangeTracking();

  active_tree_->set_has_ever_been_drawn(true);
  devtools_instrumentation::DidDrawFrame(id_);
  benchmark_instrumentation::IssueImplThreadRenderingStatsEvent(
      rendering_stats_instrumentation_->TakeImplThreadRenderingStats());
  return true;
}

viz::CompositorFrame LayerTreeHostImpl::GenerateCompositorFrame(
    FrameData* frame) {
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(CurrentBeginFrameArgs().trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "GenerateCompositorFrame");
  base::TimeTicks frame_time = CurrentBeginFrameArgs().frame_time;
  fps_counter_->SaveTimeStamp(frame_time);
  rendering_stats_instrumentation_->IncrementFrameCount(1);

  memory_history_->SaveEntry(tile_manager_.memory_stats_from_last_assign());

  if (debug_state_.ShowHudRects()) {
    debug_rect_history_->SaveDebugRectsForCurrentFrame(
        active_tree(), active_tree_->hud_layer(), *frame->render_surface_list,
        debug_state_);
  }

  TRACE_EVENT_INSTANT2("cc", "Scroll Delta This Frame",
                       TRACE_EVENT_SCOPE_THREAD, "x",
                       scroll_accumulated_this_frame_.x(), "y",
                       scroll_accumulated_this_frame_.y());
  scroll_accumulated_this_frame_ = gfx::Vector2dF();

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace) {
    if (pending_tree_) {
      for (auto* layer : *pending_tree_)
        layer->DidBeginTracing();
    }
    for (auto* layer : *active_tree_)
      layer->DidBeginTracing();
  }

  {
    TRACE_EVENT0("cc", "DrawLayers.FrameViewerTracing");
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        frame_viewer_instrumentation::CategoryLayerTree(),
        "cc::LayerTreeHostImpl", id_, AsValueWithFrame(frame));
  }

  const DrawMode draw_mode = GetDrawMode();

  // Because the contents of the HUD depend on everything else in the frame, the
  // contents of its texture are updated as the last thing before the frame is
  // drawn.
  if (active_tree_->hud_layer()) {
    TRACE_EVENT0("cc", "DrawLayers.UpdateHudTexture");
    active_tree_->hud_layer()->UpdateHudTexture(
        draw_mode, layer_tree_frame_sink_, &resource_provider_,
        // The hud uses Gpu rasterization if the device is capable, not related
        // to the content of the web page.
        gpu_rasterization_status_ != GpuRasterizationStatus::OFF_DEVICE,
        frame->render_passes);
  }

  viz::CompositorFrameMetadata metadata = MakeCompositorFrameMetadata();
  PopulateMetadataContentColorUsage(frame, &metadata);
  metadata.may_contain_video = frame->may_contain_video;
  metadata.deadline = viz::FrameDeadline(
      CurrentBeginFrameArgs().frame_time,
      frame->deadline_in_frames.value_or(0u), CurrentBeginFrameArgs().interval,
      frame->use_default_lower_bound_deadline);

  frame_rate_estimator_.WillDraw(CurrentBeginFrameArgs().frame_time);
  metadata.preferred_frame_interval =
      frame_rate_estimator_.GetPreferredInterval();

  metadata.activation_dependencies = std::move(frame->activation_dependencies);
  active_tree()->FinishSwapPromises(&metadata);
  // The swap-promises should not change the frame-token.
  DCHECK_EQ(metadata.frame_token, *next_frame_token_);

  if (render_frame_metadata_observer_) {
    last_draw_render_frame_metadata_ = MakeRenderFrameMetadata(frame);

    // We cache the value of any new vertical scroll direction so that we can
    // accurately determine when the next change in vertical scroll direction
    // occurs. Note that |kNull| is only used to indicate the absence of a
    // vertical scroll direction and should therefore be ignored.
    if (last_draw_render_frame_metadata_->new_vertical_scroll_direction !=
        viz::VerticalScrollDirection::kNull) {
      last_vertical_scroll_direction_ =
          last_draw_render_frame_metadata_->new_vertical_scroll_direction;
    }

    render_frame_metadata_observer_->OnRenderFrameSubmission(
        *last_draw_render_frame_metadata_, &metadata,
        active_tree()->TakeForceSendMetadataRequest());
  }

  if (!CommitToActiveTree() && !metadata.latency_info.empty()) {
    base::TimeTicks draw_time = base::TimeTicks::Now();

    ukm::SourceId exemplar = metadata.latency_info.front().ukm_source_id();
    bool all_valid = true;
    bool all_unique = true;
    for (auto& latency : metadata.latency_info) {
      latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, draw_time);
      all_valid &= latency.ukm_source_id() != ukm::kInvalidSourceId;
      all_unique &= latency.ukm_source_id() == exemplar;
    }

    RecordSourceIdConsistency(all_valid, all_unique);
  }
  ui::LatencyInfo::TraceIntermediateFlowEvents(
      metadata.latency_info,
      perfetto::protos::pbzero::ChromeLatencyInfo::STEP_SWAP_BUFFERS);

  // Collect all resource ids in the render passes into a single array.
  std::vector<viz::ResourceId> resources;
  for (const auto& render_pass : frame->render_passes) {
    for (auto* quad : render_pass->quad_list) {
      for (viz::ResourceId resource_id : quad->resources)
        resources.push_back(resource_id);
    }
  }

  DCHECK(frame->begin_frame_ack.frame_id.IsSequenceValid());
  metadata.begin_frame_ack = frame->begin_frame_ack;

  viz::CompositorFrame compositor_frame;
  compositor_frame.metadata = std::move(metadata);
  resource_provider_.PrepareSendToParent(
      resources, &compositor_frame.resource_list,
      layer_tree_frame_sink_->context_provider());
  compositor_frame.render_pass_list = std::move(frame->render_passes);

  // We should always have a valid LocalSurfaceId in LayerTreeImpl unless we
  // don't have a scheduler because without a scheduler commits are not deferred
  // and LayerTrees without valid LocalSurfaceId might slip through, but
  // single-thread-without-scheduler mode is only used in tests so it doesn't
  // matter.
  CHECK(!settings_.single_thread_proxy_scheduler ||
        active_tree()->local_surface_id_allocation_from_parent().IsValid());
  layer_tree_frame_sink_->SetLocalSurfaceId(
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id());

  last_draw_local_surface_id_allocation_ =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  return compositor_frame;
}

void LayerTreeHostImpl::DidDrawAllLayers(const FrameData& frame) {
  // TODO(lethalantidote): LayerImpl::DidDraw can be removed when
  // VideoLayerImpl is removed.
  for (size_t i = 0; i < frame.will_draw_layers.size(); ++i)
    frame.will_draw_layers[i]->DidDraw(&resource_provider_);

  for (auto* it : video_frame_controllers_)
    it->DidDrawFrame();
}

int LayerTreeHostImpl::RequestedMSAASampleCount() const {
  if (settings_.gpu_rasterization_msaa_sample_count == -1) {
    // On "low-end" devices use 4 samples per pixel to save memory.
    if (base::SysInfo::IsLowEndDevice())
      return 4;

    // Use the most up-to-date version of device_scale_factor that we have.
    float device_scale_factor = pending_tree_
                                    ? pending_tree_->device_scale_factor()
                                    : active_tree_->device_scale_factor();
    return device_scale_factor >= 2.0f ? 4 : 8;
  }

  return settings_.gpu_rasterization_msaa_sample_count;
}

void LayerTreeHostImpl::GetGpuRasterizationCapabilities(
    bool* gpu_rasterization_enabled,
    bool* gpu_rasterization_supported,
    int* max_msaa_samples,
    bool* supports_disable_msaa) {
  *gpu_rasterization_enabled = false;
  *gpu_rasterization_supported = false;
  *max_msaa_samples = 0;
  *supports_disable_msaa = false;

  if (settings_.gpu_rasterization_disabled)
    return;

  if (!(layer_tree_frame_sink_ && layer_tree_frame_sink_->context_provider() &&
        layer_tree_frame_sink_->worker_context_provider())) {
    return;
  }

  viz::RasterContextProvider* context_provider =
      layer_tree_frame_sink_->worker_context_provider();
  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      context_provider);

  const auto& caps = context_provider->ContextCapabilities();
  *gpu_rasterization_enabled = caps.gpu_rasterization;
  if (!*gpu_rasterization_enabled)
    return;

  bool use_msaa = !caps.msaa_is_slow && !caps.avoid_stencil_buffers;

  if (use_oop_rasterization_) {
    *gpu_rasterization_supported = true;
    *supports_disable_msaa = caps.multisample_compatibility;
    // For OOP raster, the gpu service side will disable msaa if the
    // requested samples are not enough.  GPU raster does this same
    // logic below client side.
    if (use_msaa)
      *max_msaa_samples = RequestedMSAASampleCount();
    return;
  }

  if (!context_provider->ContextSupport()->HasGrContextSupport())
    return;

  // Do not check GrContext above. It is lazy-created, and we only want to
  // create it if it might be used.
  GrContext* gr_context = context_provider->GrContext();
  *gpu_rasterization_supported = !!gr_context;
  if (!*gpu_rasterization_supported)
    return;

  *supports_disable_msaa = caps.multisample_compatibility;
  if (use_msaa) {
    // Skia may blacklist MSAA independently of Chrome. Query Skia for its max
    // supported sample count. Assume gpu compositing + gpu raster for this, as
    // that is what we are hoping to use.
    viz::ResourceFormat tile_format = TileRasterBufferFormat(
        settings_, layer_tree_frame_sink_->context_provider(),
        /*use_gpu_rasterization=*/true);
    SkColorType color_type = ResourceFormatToClosestSkColorType(
        /*gpu_compositing=*/true, tile_format);
    *max_msaa_samples =
        gr_context->maxSurfaceSampleCountForColorType(color_type);
  }
}

bool LayerTreeHostImpl::UpdateGpuRasterizationStatus() {
  if (!need_update_gpu_rasterization_status_)
    return false;
  need_update_gpu_rasterization_status_ = false;

  // TODO(danakj): Can we avoid having this run when there's no
  // LayerTreeFrameSink?
  // For now just early out and leave things unchanged, we'll come back here
  // when we get a LayerTreeFrameSink.
  if (!layer_tree_frame_sink_)
    return false;

  int requested_msaa_samples = RequestedMSAASampleCount();
  int max_msaa_samples = 0;
  bool gpu_rasterization_enabled = false;
  bool gpu_rasterization_supported = false;
  bool supports_disable_msaa = false;
  GetGpuRasterizationCapabilities(&gpu_rasterization_enabled,
                                  &gpu_rasterization_supported,
                                  &max_msaa_samples, &supports_disable_msaa);

  bool use_gpu = false;
  bool can_use_msaa =
      requested_msaa_samples > 0 && max_msaa_samples >= requested_msaa_samples;

  if (!gpu_rasterization_enabled) {
    if (gpu_rasterization_supported)
      gpu_rasterization_status_ = GpuRasterizationStatus::OFF_FORCED;
    else
      gpu_rasterization_status_ = GpuRasterizationStatus::OFF_DEVICE;
  } else {
    use_gpu = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::ON;
  }

  if (use_gpu && !use_gpu_rasterization_) {
    if (!gpu_rasterization_supported) {
      // If GPU rasterization is unusable, e.g. if GlContext could not
      // be created due to losing the GL context, force use of software
      // raster.
      use_gpu = false;
      can_use_msaa_ = false;
      supports_disable_msaa_ = false;
      gpu_rasterization_status_ = GpuRasterizationStatus::OFF_DEVICE;
    }
  }

  // Changes in MSAA settings require that we re-raster resources for the
  // settings to take effect. But we don't need to trigger any raster
  // invalidation in this case since these settings change only if the context
  // changed. In this case we already re-allocate and re-raster all resources.
  if (use_gpu == use_gpu_rasterization_ && can_use_msaa == can_use_msaa_ &&
      supports_disable_msaa == supports_disable_msaa_) {
    return false;
  }

  use_gpu_rasterization_ = use_gpu;
  can_use_msaa_ = can_use_msaa;
  supports_disable_msaa_ = supports_disable_msaa;
  return true;
}

void LayerTreeHostImpl::UpdateTreeResourcesForGpuRasterizationIfNeeded() {
  if (!UpdateGpuRasterizationStatus())
    return;

  // Clean up and replace existing tile manager with another one that uses
  // appropriate rasterizer. Only do this however if we already have a
  // resource pool, since otherwise we might not be able to create a new
  // one.
  ReleaseTileResources();
  if (resource_pool_) {
    CleanUpTileManagerResources();
    CreateTileManagerResources();
  }
  RecreateTileResources();

  // We have released tilings for both active and pending tree.
  // We would not have any content to draw until the pending tree is activated.
  // Prevent the active tree from drawing until activation.
  // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();
}

void LayerTreeHostImpl::RegisterMainThreadPresentationTimeCallback(
    uint32_t frame_token,
    LayerTreeHost::PresentationTimeCallback callback) {
  std::vector<LayerTreeHost::PresentationTimeCallback> as_vector;
  as_vector.emplace_back(std::move(callback));
  presentation_time_callbacks_.RegisterMainThreadPresentationCallbacks(
      frame_token, std::move(as_vector));
}

void LayerTreeHostImpl::RegisterCompositorPresentationTimeCallback(
    uint32_t frame_token,
    LayerTreeHost::PresentationTimeCallback callback) {
  std::vector<LayerTreeHost::PresentationTimeCallback> as_vector;
  as_vector.emplace_back(std::move(callback));
  presentation_time_callbacks_.RegisterCompositorPresentationCallbacks(
      frame_token, std::move(as_vector));
}

bool LayerTreeHostImpl::WillBeginImplFrame(const viz::BeginFrameArgs& args) {
  impl_thread_phase_ = ImplThreadPhase::INSIDE_IMPL_FRAME;
  current_begin_frame_tracker_.Start(args);
  frame_trackers_.NotifyBeginImplFrame(args);

  if (is_likely_to_require_a_draw_) {
    // Optimistically schedule a draw. This will let us expect the tile manager
    // to complete its work so that we can draw new tiles within the impl frame
    // we are beginning now.
    SetNeedsRedraw();
  }

  if (input_handler_client_) {
    scrollbar_controller_->WillBeginImplFrame();
    input_handler_client_->DeliverInputForBeginFrame(args);
  }

  Animate();

  image_animation_controller_.WillBeginImplFrame(args);

  for (auto* it : video_frame_controllers_)
    it->OnBeginFrame(args);

  bool recent_frame_had_no_damage =
      consecutive_frame_with_damage_count_ < settings_.damaged_frame_limit;
  // Check damage early if the setting is enabled and a recent frame had no
  // damage. HasDamage() expects CanDraw to be true. If we can't check damage,
  // return true to indicate that there might be damage in this frame.
  if (settings_.enable_early_damage_check && recent_frame_had_no_damage &&
      CanDraw()) {
    bool ok = active_tree()->UpdateDrawProperties();
    DCHECK(ok);
    DamageTracker::UpdateDamageTracking(active_tree_.get());
    bool has_damage = HasDamage();
    // Animations are updated after we attempt to draw. If the frame is aborted,
    // update animations now.
    if (!has_damage)
      UpdateAnimationState(true);
    return has_damage;
  }
  // Assume there is damage if we cannot check for damage.
  return true;
}

void LayerTreeHostImpl::DidFinishImplFrame(const viz::BeginFrameArgs& args) {
  frame_trackers_.NotifyFrameEnd(current_begin_frame_tracker_.Current(), args);
  impl_thread_phase_ = ImplThreadPhase::IDLE;
  current_begin_frame_tracker_.Finish();
}

void LayerTreeHostImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                           FrameSkippedReason reason) {
  if (layer_tree_frame_sink_)
    layer_tree_frame_sink_->DidNotProduceFrame(ack);

  // If a frame was not submitted because there was no damage, or the scheduler
  // hit the frame-deadline while waiting for the main-thread, notify the
  // trackers.
  if (reason != FrameSkippedReason::kRecoverLatency &&
      impl_thread_phase_ == ImplThreadPhase::INSIDE_IMPL_FRAME) {
    // It is possible that |ack| is for a 'future frame', i.e. for the next
    // frame from the one currently being handled by the compositor (represented
    // by the BeginFrameArgs instance in |current_begin_frame_tracker_|). This
    // can happen, for example, when a frame is skipped early for
    // latency-recovery, while the previous frame is still being processed.
    // Notify the trackers only when this is *not* the case (since the trackers
    // are not notified about the start of the future frame either).
    const auto& args = current_begin_frame_tracker_.Current();
    if (args.frame_id == ack.frame_id) {
      frame_trackers_.NotifyImplFrameCausedNoDamage(ack);
    }
  }
}

void LayerTreeHostImpl::SynchronouslyInitializeAllTiles() {
  // Only valid for the single-threaded non-scheduled/synchronous case
  // using the zero copy raster worker pool.
  single_thread_synchronous_task_graph_runner_->RunUntilIdle();
}

static uint32_t GetFlagsForSurfaceLayer(const SurfaceLayerImpl* layer) {
  uint32_t flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch;
  if (layer->range().IsValid()) {
    flags |= viz::HitTestRegionFlags::kHitTestChildSurface;
  } else {
    flags |= viz::HitTestRegionFlags::kHitTestMine;
  }
  return flags;
}

static void PopulateHitTestRegion(viz::HitTestRegion* hit_test_region,
                                  const LayerImpl* layer,
                                  uint32_t flags,
                                  uint32_t async_hit_test_reasons,
                                  const gfx::Rect& rect,
                                  const viz::SurfaceId& surface_id,
                                  float device_scale_factor) {
  hit_test_region->frame_sink_id = surface_id.frame_sink_id();
  hit_test_region->flags = flags;
  hit_test_region->async_hit_test_reasons = async_hit_test_reasons;
  DCHECK_EQ(!!async_hit_test_reasons,
            !!(flags & viz::HitTestRegionFlags::kHitTestAsk));

  hit_test_region->rect = rect;
  // The transform of hit test region maps a point from parent hit test region
  // to the local space. This is the inverse of screen space transform. Because
  // hit test query wants the point in target to be in Pixel space, we
  // counterscale the transform here. Note that the rect is scaled by dsf, so
  // the point and the rect are still in the same space.
  gfx::Transform surface_to_root_transform = layer->ScreenSpaceTransform();
  surface_to_root_transform.Scale(SK_Scalar1 / device_scale_factor,
                                  SK_Scalar1 / device_scale_factor);
  surface_to_root_transform.FlattenTo2d();
  // TODO(sunxd): Avoid losing precision by not using inverse if possible.
  bool ok = surface_to_root_transform.GetInverse(&hit_test_region->transform);
  // Note: If |ok| is false, the |transform| is set to the identity before
  // returning, which is what we want.
  ALLOW_UNUSED_LOCAL(ok);
}

base::Optional<viz::HitTestRegionList> LayerTreeHostImpl::BuildHitTestData() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BuildHitTestData");

  base::Optional<viz::HitTestRegionList> hit_test_region_list(base::in_place);
  hit_test_region_list->flags = viz::HitTestRegionFlags::kHitTestMine |
                                viz::HitTestRegionFlags::kHitTestMouse |
                                viz::HitTestRegionFlags::kHitTestTouch;
  hit_test_region_list->bounds = active_tree_->GetDeviceViewport();
  hit_test_region_list->transform = DrawTransform();

  float device_scale_factor = active_tree()->device_scale_factor();

  Region overlapping_region;
  size_t num_iterated_layers = 0;
  // If the layer tree contains more than 100 layers, we stop accumulating
  // layers in |overlapping_region| to save compositor frame submitting time, as
  // a result we do async hit test on any surface layers that
  bool assume_overlap = false;
  for (const auto* layer : base::Reversed(*active_tree())) {
    // Viz hit test needs to collect information for pointer-events: none OOPIFs
    // as well. Now Layer::HitTestable ignores pointer-events property, but this
    // early out will not work correctly if we integrate has_pointer_events_none
    // into Layer::HitTestable, so we make sure we don't skip surface layers
    // that draws content but has pointer-events: none property.
    if (!(layer->HitTestable() ||
          (layer->is_surface_layer() && layer->DrawsContent())))
      continue;

    if (layer->is_surface_layer()) {
      const auto* surface_layer = static_cast<const SurfaceLayerImpl*>(layer);
      // If a surface layer is created not by child frame compositor or the
      // frame owner has pointer-events: none property, the surface layer
      // becomes not hit testable. We should not generate data for it.
      if (!surface_layer->surface_hit_testable() ||
          !surface_layer->range().IsValid()) {
        // We collect any overlapped regions that does not have pointer-events:
        // none.
        if (!surface_layer->has_pointer_events_none() && !assume_overlap) {
          overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
              layer->ScreenSpaceTransform(),
              gfx::Rect(surface_layer->bounds())));
        }
        continue;
      }

      gfx::Rect content_rect(
          gfx::ScaleToEnclosingRect(gfx::Rect(surface_layer->bounds()),
                                    device_scale_factor, device_scale_factor));

      gfx::Rect layer_screen_space_rect = MathUtil::MapEnclosingClippedRect(
          surface_layer->ScreenSpaceTransform(),
          gfx::Rect(surface_layer->bounds()));
      auto flag = GetFlagsForSurfaceLayer(surface_layer);
      uint32_t async_hit_test_reasons =
          viz::AsyncHitTestReasons::kNotAsyncHitTest;
      if (surface_layer->has_pointer_events_none())
        flag |= viz::HitTestRegionFlags::kHitTestIgnore;
      if (assume_overlap ||
          overlapping_region.Intersects(layer_screen_space_rect)) {
        flag |= viz::HitTestRegionFlags::kHitTestAsk;
        async_hit_test_reasons |= viz::AsyncHitTestReasons::kOverlappedRegion;
      }
      bool layer_hit_test_region_is_masked =
          active_tree()
              ->property_trees()
              ->effect_tree.HitTestMayBeAffectedByMask(
                  surface_layer->effect_tree_index());
      if (surface_layer->is_clipped() || layer_hit_test_region_is_masked) {
        bool layer_hit_test_region_is_rectangle =
            !layer_hit_test_region_is_masked &&
            surface_layer->ScreenSpaceTransform().Preserves2dAxisAlignment() &&
            active_tree()
                ->property_trees()
                ->effect_tree.ClippedHitTestRegionIsRectangle(
                    surface_layer->effect_tree_index());
        content_rect =
            gfx::ScaleToEnclosingRect(surface_layer->visible_layer_rect(),
                                      device_scale_factor, device_scale_factor);
        if (!layer_hit_test_region_is_rectangle) {
          flag |= viz::HitTestRegionFlags::kHitTestAsk;
          async_hit_test_reasons |= viz::AsyncHitTestReasons::kIrregularClip;
        }
      }
      const auto& surface_id = surface_layer->range().end();
      hit_test_region_list->regions.emplace_back();
      PopulateHitTestRegion(&hit_test_region_list->regions.back(), layer, flag,
                            async_hit_test_reasons, content_rect, surface_id,
                            device_scale_factor);
      continue;
    }
    // TODO(sunxd): Submit all overlapping layer bounds as hit test regions.
    // Also investigate if we can use visible layer rect as overlapping regions.
    num_iterated_layers++;
    if (num_iterated_layers > kAssumeOverlapThreshold)
      assume_overlap = true;
    if (!assume_overlap) {
      overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
          layer->ScreenSpaceTransform(), gfx::Rect(layer->bounds())));
    }
  }

  return hit_test_region_list;
}

void LayerTreeHostImpl::DidLoseLayerTreeFrameSink() {
  // Check that we haven't already detected context loss because we get it via
  // two paths: compositor context loss on the compositor thread and worker
  // context loss posted from main thread to compositor thread. We do not want
  // to reset the context recovery state in the scheduler.
  if (!has_valid_layer_tree_frame_sink_)
    return;
  has_valid_layer_tree_frame_sink_ = false;
  client_->DidLoseLayerTreeFrameSinkOnImplThread();
}

bool LayerTreeHostImpl::HaveRootScrollNode() const {
  return InnerViewportScrollNode();
}

void LayerTreeHostImpl::SetNeedsCommit() {
  client_->SetNeedsCommitOnImplThread();
}

ScrollNode* LayerTreeHostImpl::InnerViewportScrollNode() const {
  return active_tree_->InnerViewportScrollNode();
}

ScrollNode* LayerTreeHostImpl::OuterViewportScrollNode() const {
  return active_tree_->OuterViewportScrollNode();
}

ScrollNode* LayerTreeHostImpl::CurrentlyScrollingNode() {
  return active_tree()->CurrentlyScrollingNode();
}

const ScrollNode* LayerTreeHostImpl::CurrentlyScrollingNode() const {
  return active_tree()->CurrentlyScrollingNode();
}

bool LayerTreeHostImpl::IsActivelyPrecisionScrolling() const {
  if (!CurrentlyScrollingNode())
    return false;
  // On Android WebView root flings are controlled by the application,
  // so the compositor does not animate them and can't tell if they
  // are actually animating. So assume there are none.
  if (settings_.ignore_root_layer_flings && IsCurrentlyScrollingViewport())
    return false;

  if (!last_scroll_update_state_)
    return false;

  bool did_scroll_content =
      did_scroll_x_for_scroll_gesture_ || did_scroll_y_for_scroll_gesture_;
  return !ShouldAnimateScroll(last_scroll_update_state_.value()) &&
         did_scroll_content;
}

void LayerTreeHostImpl::CreatePendingTree() {
  CHECK(!pending_tree_);
  if (recycle_tree_) {
    recycle_tree_.swap(pending_tree_);
  } else {
    pending_tree_ = std::make_unique<LayerTreeImpl>(
        this, active_tree()->page_scale_factor(),
        active_tree()->top_controls_shown_ratio(),
        active_tree()->bottom_controls_shown_ratio(),
        active_tree()->elastic_overscroll());
  }
  pending_tree_fully_painted_ = false;

  client_->OnCanDrawStateChanged(CanDraw());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("cc", "PendingTree:waiting",
                                    TRACE_ID_LOCAL(pending_tree_.get()));
}

void LayerTreeHostImpl::PushScrollbarOpacitiesFromActiveToPending() {
  if (!active_tree())
    return;
  for (auto& pair : scrollbar_animation_controllers_) {
    for (auto* scrollbar : pair.second->Scrollbars()) {
      if (const EffectNode* source_effect_node =
              active_tree()
                  ->property_trees()
                  ->effect_tree.FindNodeFromElementId(
                      scrollbar->element_id())) {
        if (EffectNode* target_effect_node =
                pending_tree()
                    ->property_trees()
                    ->effect_tree.FindNodeFromElementId(
                        scrollbar->element_id())) {
          DCHECK(target_effect_node);
          float source_opacity = source_effect_node->opacity;
          float target_opacity = target_effect_node->opacity;
          if (source_opacity == target_opacity)
            continue;
          target_effect_node->opacity = source_opacity;
          pending_tree()->property_trees()->effect_tree.set_needs_update(true);
        }
      }
    }
  }
}

void LayerTreeHostImpl::ActivateSyncTree() {
  TRACE_EVENT0("cc,benchmark", "LayerTreeHostImpl::ActivateSyncTree()");
  if (pending_tree_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("cc", "PendingTree:waiting",
                                    TRACE_ID_LOCAL(pending_tree_.get()));
    active_tree_->lifecycle().AdvanceTo(LayerTreeLifecycle::kBeginningSync);

    // In most cases, this will be reset in NotifyReadyToActivate, since we
    // activate the pending tree only when its ready. But an activation may be
    // forced, in the case of a context loss for instance, so reset it here as
    // well.
    pending_tree_raster_duration_timer_.reset();

    // Process any requests in the UI resource queue.  The request queue is
    // given in LayerTreeHost::FinishCommitOnImplThread.  This must take place
    // before the swap.
    pending_tree_->ProcessUIResourceRequestQueue();

    if (pending_tree_->needs_full_tree_sync()) {
      TreeSynchronizer::SynchronizeTrees(pending_tree_.get(),
                                         active_tree_.get());
    }

    PushScrollbarOpacitiesFromActiveToPending();
    pending_tree_->PushPropertyTreesTo(active_tree_.get());
    active_tree_->lifecycle().AdvanceTo(
        LayerTreeLifecycle::kSyncedPropertyTrees);

    TreeSynchronizer::PushLayerProperties(pending_tree(), active_tree());

    active_tree_->lifecycle().AdvanceTo(
        LayerTreeLifecycle::kSyncedLayerProperties);

    pending_tree_->PushPropertiesTo(active_tree_.get());
    if (!pending_tree_->LayerListIsEmpty())
      pending_tree_->property_trees()->ResetAllChangeTracking();

    // Property tree nodes have been updated by PushLayerProperties. Update
    // elements available on active tree to start/stop ticking animations.
    UpdateElements(ElementListType::ACTIVE);

    active_tree_->lifecycle().AdvanceTo(LayerTreeLifecycle::kNotSyncing);

    // The previous scrolling node no longer exists in the new tree.
    if (!active_tree_->CurrentlyScrollingNode())
      ClearCurrentlyScrollingNode();

    // Now that we've synced everything from the pending tree to the active
    // tree, rename the pending tree the recycle tree so we can reuse it on the
    // next sync.
    DCHECK(!recycle_tree_);
    pending_tree_.swap(recycle_tree_);

    // ScrollTimelines track a scroll source (i.e. a scroll node in the scroll
    // tree), whose ElementId may change between the active and pending trees.
    // Therefore we must inform all ScrollTimelines when the pending tree is
    // promoted to active.
    mutator_host_->PromoteScrollTimelinesPendingToActive();

    // If we commit to the active tree directly, this is already done during
    // commit.
    ActivateAnimations();

    // Update the state for images in ImageAnimationController and TileManager
    // before dirtying tile priorities. Since these components cache tree
    // specific state, these should be updated before DidModifyTilePriorities
    // which can synchronously issue a PrepareTiles. Note that if we commit to
    // the active tree directly, this is already done during commit.
    ActivateStateForImages();
  } else {
    active_tree_->ProcessUIResourceRequestQueue();
  }

  active_tree_->UpdateViewportContainerSizes();

  if (InnerViewportScrollNode()) {
    active_tree_->property_trees()->scroll_tree.ClampScrollToMaxScrollOffset(
        *InnerViewportScrollNode(), active_tree_.get());

    DCHECK(OuterViewportScrollNode());
    active_tree_->property_trees()->scroll_tree.ClampScrollToMaxScrollOffset(
        *OuterViewportScrollNode(), active_tree_.get());
  }

  active_tree_->DidBecomeActive();
  client_->RenewTreePriority();

  // If we have any picture layers, then by activating we also modified tile
  // priorities.
  if (!active_tree_->picture_layers().empty())
    DidModifyTilePriorities();

  client_->OnCanDrawStateChanged(CanDraw());
  client_->DidActivateSyncTree();
  if (!tree_activation_callback_.is_null())
    tree_activation_callback_.Run();

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation =
      active_tree_->TakePendingPageScaleAnimation();
  if (pending_page_scale_animation) {
    StartPageScaleAnimation(pending_page_scale_animation->target_offset,
                            pending_page_scale_animation->use_anchor,
                            pending_page_scale_animation->scale,
                            pending_page_scale_animation->duration);
  }
  // Activation can change the root scroll offset, so inform the synchronous
  // input handler.
  UpdateRootLayerStateForSynchronousInputHandler();

  // Update the child's LocalSurfaceId.
  if (active_tree()->local_surface_id_allocation_from_parent().IsValid()) {
    child_local_surface_id_allocator_.UpdateFromParent(
        active_tree()->local_surface_id_allocation_from_parent());
    if (active_tree()->TakeNewLocalSurfaceIdRequest())
      AllocateLocalSurfaceId();
  }

  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_host_impl=3
  if (VLOG_IS_ON(3)) {
    const char* client_name = GetClientNameForMetrics();
    if (!client_name)
      client_name = "<unknown client>";
    VLOG(3) << "After activating (" << client_name
            << ") sync tree, the active tree:"
            << "\nproperty_trees:\n"
            << active_tree_->property_trees()->ToString() << "\n"
            << "cc::LayerImpls:\n"
            << active_tree_->LayerListAsJson();
  }
}

void LayerTreeHostImpl::ActivateStateForImages() {
  image_animation_controller_.DidActivate();
  tile_manager_.DidActivateSyncTree();
}

void LayerTreeHostImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // Only work for low-end devices for now.
  if (!base::SysInfo::IsLowEndDevice())
    return;

  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

  ReleaseTileResources();
  active_tree_->OnPurgeMemory();
  if (pending_tree_)
    pending_tree_->OnPurgeMemory();
  if (recycle_tree_)
    recycle_tree_->OnPurgeMemory();

  EvictAllUIResources();
  if (image_decode_cache_) {
    image_decode_cache_->SetShouldAggressivelyFreeResources(true);
    image_decode_cache_->SetShouldAggressivelyFreeResources(false);
  }
  if (resource_pool_)
    resource_pool_->OnMemoryPressure(level);
  tile_manager_.decoded_image_tracker().UnlockAllImages();
}

void LayerTreeHostImpl::SetVisible(bool visible) {
  DCHECK(task_runner_provider_->IsImplThread());

  if (visible_ == visible)
    return;
  visible_ = visible;
  DidVisibilityChange(this, visible_);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());

  // If we just became visible, we have to ensure that we draw high res tiles,
  // to prevent checkerboard/low res flashes.
  if (visible_) {
    // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
    SetRequiresHighResToDraw();
    // Prior CompositorFrame may have been discarded and thus we need to ensure
    // that we submit a new one, even if there are no tiles. Therefore, force a
    // full viewport redraw. However, this is unnecessary when we become visible
    // for the first time (before the first commit) as there is no prior
    // CompositorFrame to replace. We can safely use |!active_tree_->
    // LayerListIsEmpty()| as a proxy for this, because we wouldn't be able to
    // draw anything even if this is not the first time we become visible.
    if (!active_tree_->LayerListIsEmpty()) {
      SetFullViewportDamage();
      SetNeedsRedraw();
    }
  } else {
    EvictAllUIResources();
    // Call PrepareTiles to evict tiles when we become invisible.
    PrepareTiles();
    tile_manager_.decoded_image_tracker().UnlockAllImages();
  }
}

void LayerTreeHostImpl::SetNeedsOneBeginImplFrame() {
  // TODO(miletus): This is just the compositor-thread-side call to the
  // SwapPromiseMonitor to say something happened that may cause a swap in the
  // future. The name should not refer to SetNeedsRedraw but it does for now.
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  events_metrics_manager_.SaveActiveEventMetrics();
  client_->SetNeedsOneBeginImplFrameOnImplThread();
}

void LayerTreeHostImpl::SetNeedsRedraw() {
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  events_metrics_manager_.SaveActiveEventMetrics();
  client_->SetNeedsRedrawOnImplThread();
}

ManagedMemoryPolicy LayerTreeHostImpl::ActualManagedMemoryPolicy() const {
  ManagedMemoryPolicy actual = cached_managed_memory_policy_;
  if (debug_state_.rasterize_only_visible_content) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;
  } else if (use_gpu_rasterization()) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
  }
  return actual;
}

void LayerTreeHostImpl::ReleaseTreeResources() {
  active_tree_->ReleaseResources();
  if (pending_tree_)
    pending_tree_->ReleaseResources();
  if (recycle_tree_)
    recycle_tree_->ReleaseResources();

  EvictAllUIResources();
}

void LayerTreeHostImpl::ReleaseTileResources() {
  active_tree_->ReleaseTileResources();
  if (pending_tree_)
    pending_tree_->ReleaseTileResources();
  if (recycle_tree_)
    recycle_tree_->ReleaseTileResources();

  // Need to update tiles again in order to kick of raster work for all the
  // tiles that are dropped here.
  active_tree_->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::RecreateTileResources() {
  active_tree_->RecreateTileResources();
  if (pending_tree_)
    pending_tree_->RecreateTileResources();
  if (recycle_tree_)
    recycle_tree_->RecreateTileResources();
}

void LayerTreeHostImpl::CreateTileManagerResources() {
  raster_buffer_provider_ = CreateRasterBufferProvider();

  viz::ResourceFormat tile_format = TileRasterBufferFormat(
      settings_, layer_tree_frame_sink_->context_provider(),
      use_gpu_rasterization_);

  if (use_gpu_rasterization_) {
    image_decode_cache_ = std::make_unique<GpuImageDecodeCache>(
        layer_tree_frame_sink_->worker_context_provider(),
        use_oop_rasterization_,
        viz::ResourceFormatToClosestSkColorType(/*gpu_compositing=*/true,
                                                tile_format),
        settings_.decoded_image_working_set_budget_bytes, max_texture_size_,
        paint_image_generator_client_id_);
  } else {
    bool gpu_compositing = !!layer_tree_frame_sink_->context_provider();
    image_decode_cache_ = std::make_unique<SoftwareImageDecodeCache>(
        viz::ResourceFormatToClosestSkColorType(gpu_compositing, tile_format),
        settings_.decoded_image_working_set_budget_bytes,
        paint_image_generator_client_id_);
  }

  // Pass the single-threaded synchronous task graph runner to the worker pool
  // if we're in synchronous single-threaded mode.
  TaskGraphRunner* task_graph_runner = task_graph_runner_;
  if (is_synchronous_single_threaded_) {
    DCHECK(!single_thread_synchronous_task_graph_runner_);
    single_thread_synchronous_task_graph_runner_.reset(
        new SynchronousTaskGraphRunner);
    task_graph_runner = single_thread_synchronous_task_graph_runner_.get();
  }

  tile_manager_.SetResources(resource_pool_.get(), image_decode_cache_.get(),
                             task_graph_runner, raster_buffer_provider_.get(),
                             use_gpu_rasterization_);
  tile_manager_.SetCheckerImagingForceDisabled(
      settings_.only_checker_images_with_gpu_raster && !use_gpu_rasterization_);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
}

std::unique_ptr<RasterBufferProvider>
LayerTreeHostImpl::CreateRasterBufferProvider() {
  DCHECK(GetTaskRunner());

  viz::ContextProvider* compositor_context_provider =
      layer_tree_frame_sink_->context_provider();
  if (!compositor_context_provider)
    return std::make_unique<BitmapRasterBufferProvider>(layer_tree_frame_sink_);

  const gpu::Capabilities& caps =
      compositor_context_provider->ContextCapabilities();
  viz::RasterContextProvider* worker_context_provider =
      layer_tree_frame_sink_->worker_context_provider();

  viz::ResourceFormat tile_format = TileRasterBufferFormat(
      settings_, compositor_context_provider, use_gpu_rasterization_);

  if (use_gpu_rasterization_) {
    DCHECK(worker_context_provider);

    return std::make_unique<GpuRasterBufferProvider>(
        compositor_context_provider, worker_context_provider,
        settings_.resource_settings.use_gpu_memory_buffer_resources,
        tile_format, settings_.max_gpu_raster_tile_size,
        settings_.unpremultiply_and_dither_low_bit_depth_tiles,
        use_oop_rasterization_);
  }

  bool use_zero_copy = settings_.use_zero_copy;
  // TODO(reveman): Remove this when mojo supports worker contexts.
  // crbug.com/522440
  if (!use_zero_copy && !worker_context_provider) {
    LOG(ERROR)
        << "Forcing zero-copy tile initialization as worker context is missing";
    use_zero_copy = true;
  }

  if (use_zero_copy) {
    return std::make_unique<ZeroCopyRasterBufferProvider>(
        layer_tree_frame_sink_->gpu_memory_buffer_manager(),
        compositor_context_provider, tile_format);
  }

  const int max_copy_texture_chromium_size =
      caps.max_copy_texture_chromium_size;
  return std::make_unique<OneCopyRasterBufferProvider>(
      GetTaskRunner(), compositor_context_provider, worker_context_provider,
      layer_tree_frame_sink_->gpu_memory_buffer_manager(),
      max_copy_texture_chromium_size, settings_.use_partial_raster,
      settings_.resource_settings.use_gpu_memory_buffer_resources,
      settings_.max_staging_buffer_usage_in_bytes, tile_format);
}

void LayerTreeHostImpl::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  mutator_host_->SetLayerTreeMutator(std::move(mutator));
}

void LayerTreeHostImpl::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  paint_worklet_painter_ = std::move(painter);
}

void LayerTreeHostImpl::QueueImageDecode(int request_id,
                                         const PaintImage& image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::QueueImageDecode", "frame_key",
               image.GetKeyForFrame(PaintImage::kDefaultFrameIndex).ToString());
  // Optimistically specify the current raster color space, since we assume that
  // it won't change.
  auto content_color_usage = image.isSRGB()
                                 ? gfx::ContentColorUsage::kSRGB
                                 : gfx::ContentColorUsage::kWideColorGamut;
  tile_manager_.decoded_image_tracker().QueueImageDecode(
      image, GetRasterColorSpace(content_color_usage),
      base::BindOnce(&LayerTreeHostImpl::ImageDecodeFinished,
                     weak_factory_.GetWeakPtr(), request_id));
  tile_manager_.checker_image_tracker().DisallowCheckeringForImage(image);
}

void LayerTreeHostImpl::ImageDecodeFinished(int request_id,
                                            bool decode_succeeded) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::ImageDecodeFinished");
  completed_image_decode_requests_.emplace_back(request_id, decode_succeeded);
  client_->NotifyImageDecodeRequestFinished();
}

std::vector<std::pair<int, bool>>
LayerTreeHostImpl::TakeCompletedImageDecodeRequests() {
  auto result = std::move(completed_image_decode_requests_);
  completed_image_decode_requests_.clear();
  return result;
}

std::unique_ptr<MutatorEvents> LayerTreeHostImpl::TakeMutatorEvents() {
  std::unique_ptr<MutatorEvents> events = mutator_host_->CreateEvents();
  std::swap(events, mutator_events_);
  mutator_host_->TakeTimeUpdatedEvents(events.get());
  return events;
}

void LayerTreeHostImpl::ClearCaches() {
  // It is safe to clear the decode policy tracking on navigations since it
  // comes with an invalidation and the image ids are never re-used.
  bool can_clear_decode_policy_tracking = true;
  tile_manager_.ClearCheckerImageTracking(can_clear_decode_policy_tracking);
  if (image_decode_cache_)
    image_decode_cache_->ClearCache();
  image_animation_controller_.set_did_navigate();
}

void LayerTreeHostImpl::DidChangeScrollbarVisibility() {
  // Need a commit since input handling for scrollbars is handled in Blink so
  // we need to communicate to Blink when the compositor shows/hides the
  // scrollbars.
  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::CleanUpTileManagerResources() {
  tile_manager_.FinishTasksAndCleanUp();
  single_thread_synchronous_task_graph_runner_ = nullptr;
  image_decode_cache_ = nullptr;
  raster_buffer_provider_ = nullptr;
  // Any resources that were allocated previously should be considered not good
  // for reuse, as the RasterBufferProvider will be replaced and it may choose
  // to allocate future resources differently.
  resource_pool_->InvalidateResources();

  // We've potentially just freed a large number of resources on our various
  // contexts. Flushing now helps ensure these are cleaned up quickly
  // preventing driver cache growth. See crbug.com/643251
  if (layer_tree_frame_sink_) {
    if (auto* compositor_context = layer_tree_frame_sink_->context_provider()) {
      // TODO(ericrk): Remove ordering barrier once |compositor_context| no
      // longer uses GL.
      compositor_context->ContextGL()->OrderingBarrierCHROMIUM();
      compositor_context->ContextSupport()->FlushPendingWork();
    }
    if (auto* worker_context =
            layer_tree_frame_sink_->worker_context_provider()) {
      viz::RasterContextProvider::ScopedRasterContextLock hold(worker_context);
      hold.RasterInterface()->ShallowFlushCHROMIUM();
    }
  }
}

void LayerTreeHostImpl::ReleaseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ReleaseLayerTreeFrameSink");

  if (!layer_tree_frame_sink_) {
    DCHECK(!has_valid_layer_tree_frame_sink_);
    return;
  }

  has_valid_layer_tree_frame_sink_ = false;

  ReleaseTreeResources();
  CleanUpTileManagerResources();
  resource_pool_ = nullptr;
  ClearUIResources();

  if (layer_tree_frame_sink_->context_provider()) {
    // TODO(ericrk): Remove this once all uses of ContextGL from LTFS are
    // removed.
    auto* gl = layer_tree_frame_sink_->context_provider()->ContextGL();
    gl->Finish();
  }

  // Release any context visibility before we destroy the LayerTreeFrameSink.
  SetContextVisibility(false);

  bool all_resources_are_lost = layer_tree_frame_sink_->context_provider();

  // Destroy the submit-frame trackers before destroying the frame sink.
  frame_trackers_.ClearAll();

  // Detach from the old LayerTreeFrameSink and reset |layer_tree_frame_sink_|
  // pointer as this surface is going to be destroyed independent of if binding
  // the new LayerTreeFrameSink succeeds or not.
  layer_tree_frame_sink_->DetachFromClient();
  layer_tree_frame_sink_ = nullptr;

  // If gpu compositing, then any resources created with the gpu context in the
  // LayerTreeFrameSink were exported to the display compositor may be modified
  // by it, and thus we would be unable to determine what state they are in, in
  // order to reuse them, so they must be lost. Note that this includes
  // resources created using the gpu context associated with
  // |layer_tree_frame_sink_| internally by the compositor and any resources
  // received from an external source (for instance, TextureLayers). This is
  // because the API contract for releasing these external resources requires
  // that the compositor return them with a valid sync token and no
  // modifications to their GL state. Since that can not be guaranteed, these
  // must also be marked lost.
  //
  // In software compositing, the resources are not modified by the display
  // compositor (there is no stateful metadata for shared memory), so we do not
  // need to consider them lost.
  //
  // In both cases, the resources that are exported to the display compositor
  // will have no means of being returned to this client without the
  // LayerTreeFrameSink, so they should no longer be considered as exported. Do
  // this *after* any interactions with the |layer_tree_frame_sink_| in case it
  // tries to return resources during destruction.
  //
  // The assumption being made here is that the display compositor WILL NOT use
  // any resources previously exported when the CompositorFrameSink is closed.
  // This should be true as the connection is closed when the display compositor
  // shuts down/crashes, or when it believes we are a malicious client in which
  // case it will not display content from the previous CompositorFrameSink. If
  // this assumption is violated, we may modify resources no longer considered
  // as exported while the display compositor is still making use of them,
  // leading to visual mistakes.
  resource_provider_.ReleaseAllExportedResources(all_resources_are_lost);

  // We don't know if the next LayerTreeFrameSink will support GPU
  // rasterization. Make sure to clear the flag so that we force a
  // re-computation.
  use_gpu_rasterization_ = false;
}

bool LayerTreeHostImpl::InitializeFrameSink(
    LayerTreeFrameSink* layer_tree_frame_sink) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::InitializeFrameSink");

  ReleaseLayerTreeFrameSink();
  if (!layer_tree_frame_sink->BindToClient(this)) {
    // Avoid recreating tree resources because we might not have enough
    // information to do this yet (eg. we don't have a TileManager at this
    // point).
    return false;
  }

  layer_tree_frame_sink_ = layer_tree_frame_sink;
  has_valid_layer_tree_frame_sink_ = true;

  auto* context_provider = layer_tree_frame_sink_->context_provider();
  frame_trackers_.StartSequence(FrameSequenceTrackerType::kUniversal);

  if (context_provider) {
    max_texture_size_ =
        context_provider->ContextCapabilities().max_texture_size;
  } else {
    // Pick an arbitrary limit here similar to what hardware might.
    max_texture_size_ = 16 * 1024;
  }

  resource_pool_ = std::make_unique<ResourcePool>(
      &resource_provider_, context_provider, GetTaskRunner(),
      ResourcePool::kDefaultExpirationDelay,
      settings_.disallow_non_exact_resource_reuse);

  auto* context = layer_tree_frame_sink_->worker_context_provider();
  if (context) {
    viz::RasterContextProvider::ScopedRasterContextLock hold(context);
    use_oop_rasterization_ = context->ContextCapabilities().supports_oop_raster;
  } else {
    use_oop_rasterization_ = false;
  }

  // Since the new context may support GPU raster or be capable of MSAA, update
  // status here. We don't need to check the return value since we are
  // recreating all resources already.
  SetNeedUpdateGpuRasterizationStatus();
  UpdateGpuRasterizationStatus();

  // See note in LayerTreeImpl::UpdateDrawProperties, new LayerTreeFrameSink
  // means a new max texture size which affects draw properties. Also, if the
  // draw properties were up to date, layers still lost resources and we need to
  // UpdateDrawProperties() after calling RecreateTreeResources().
  active_tree_->set_needs_update_draw_properties();
  if (pending_tree_)
    pending_tree_->set_needs_update_draw_properties();

  CreateTileManagerResources();
  RecreateTileResources();

  client_->OnCanDrawStateChanged(CanDraw());
  SetFullViewportDamage();
  // There will not be anything to draw here, so set high res
  // to avoid checkerboards, typically when we are recovering
  // from lost context.
  // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();

  // Always allocate a new viz::LocalSurfaceId when we get a new
  // LayerTreeFrameSink to ensure that we do not reuse the same surface after
  // it might have been garbage collected.
  const viz::LocalSurfaceIdAllocation& local_surface_id_allocation =
      child_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  if (local_surface_id_allocation.IsValid())
    AllocateLocalSurfaceId();

  return true;
}

void LayerTreeHostImpl::SetBeginFrameSource(viz::BeginFrameSource* source) {
  client_->SetBeginFrameSource(source);
}

const gfx::Transform& LayerTreeHostImpl::DrawTransform() const {
  return external_transform_;
}

void LayerTreeHostImpl::DidChangeBrowserControlsPosition() {
  active_tree_->UpdateViewportContainerSizes();
  if (pending_tree_)
    pending_tree_->UpdateViewportContainerSizes();
  SetNeedsRedraw();
  SetNeedsOneBeginImplFrame();
  SetFullViewportDamage();
}

float LayerTreeHostImpl::TopControlsHeight() const {
  return active_tree_->top_controls_height();
}

float LayerTreeHostImpl::TopControlsMinHeight() const {
  return active_tree_->top_controls_min_height();
}

float LayerTreeHostImpl::BottomControlsHeight() const {
  return active_tree_->bottom_controls_height();
}

float LayerTreeHostImpl::BottomControlsMinHeight() const {
  return active_tree_->bottom_controls_min_height();
}

void LayerTreeHostImpl::SetCurrentBrowserControlsShownRatio(
    float top_ratio,
    float bottom_ratio) {
  if (active_tree_->SetCurrentBrowserControlsShownRatio(top_ratio,
                                                        bottom_ratio))
    DidChangeBrowserControlsPosition();
}

float LayerTreeHostImpl::CurrentTopControlsShownRatio() const {
  return active_tree_->CurrentTopControlsShownRatio();
}

float LayerTreeHostImpl::CurrentBottomControlsShownRatio() const {
  return active_tree_->CurrentBottomControlsShownRatio();
}

void LayerTreeHostImpl::BindToClient(InputHandlerClient* client) {
  DCHECK(input_handler_client_ == nullptr);
  input_handler_client_ = client;
}

gfx::Vector2dF LayerTreeHostImpl::ResolveScrollPercentageToPixels(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& scroll_delta) {
  gfx::Vector2dF scroll_delta_in_pixels;
  scroll_delta_in_pixels.set_x(scroll_delta.x() *
                               scroll_node.container_bounds.width());
  scroll_delta_in_pixels.set_y(scroll_delta.y() *
                               scroll_node.container_bounds.height());
  return scroll_delta_in_pixels;
}

InputHandler::ScrollStatus LayerTreeHostImpl::TryScroll(
    const ScrollTree& scroll_tree,
    ScrollNode* scroll_node) const {
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  if (scroll_node->main_thread_scrolling_reasons) {
    TRACE_EVENT1("cc", "LayerImpl::TryScroll: Failed ShouldScrollOnMainThread",
                 "MainThreadScrollingReason",
                 scroll_node->main_thread_scrolling_reasons);
    scroll_status.thread = InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        scroll_node->main_thread_scrolling_reasons;
    return scroll_status;
  }

  gfx::Transform screen_space_transform =
      scroll_tree.ScreenSpaceTransform(scroll_node->id);
  if (!screen_space_transform.IsInvertible()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Ignored NonInvertibleTransform");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNonInvertibleTransform;
    return scroll_status;
  }

  if (!scroll_node->scrollable) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
    return scroll_status;
  }

  // If an associated scrolling layer is not found, the scroll node must not
  // support impl-scrolling. The root, secondary root, and inner viewports are
  // all exceptions to this and may not have a layer because it is not required
  // for hit testing.
  if (scroll_node->id != ScrollTree::kRootNodeId &&
      scroll_node->id != ScrollTree::kSecondaryRootNodeId &&
      !scroll_node->scrolls_inner_viewport &&
      !active_tree_->LayerByElementId(scroll_node->element_id)) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Failed due to no scrolling layer");
    scroll_status.thread = InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNonFastScrollableRegion;
    return scroll_status;
  }

  // The a viewport node should be scrolled even if it has no scroll extent
  // since it'll scroll using the Viewport class which will generate browser
  // controls movement and overscroll delta.
  gfx::ScrollOffset max_scroll_offset =
      scroll_tree.MaxScrollOffset(scroll_node->id);
  if (max_scroll_offset.x() <= 0 && max_scroll_offset.y() <= 0 &&
      !viewport().ShouldScroll(*scroll_node)) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Ignored. Technically scrollable,"
                 " but has no affordance in either direction.");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
    return scroll_status;
  }

  scroll_status.thread = InputHandler::SCROLL_ON_IMPL_THREAD;
  return scroll_status;
}

static bool IsMainThreadScrolling(const InputHandler::ScrollStatus& status,
                                  const ScrollNode* scroll_node) {
  if (status.thread == InputHandler::SCROLL_ON_MAIN_THREAD) {
    if (!!scroll_node->main_thread_scrolling_reasons) {
      DCHECK(MainThreadScrollingReason::MainThreadCanSetScrollReasons(
          status.main_thread_scrolling_reasons));
    } else {
      DCHECK(MainThreadScrollingReason::CompositorCanSetScrollReasons(
          status.main_thread_scrolling_reasons));
    }
    return true;
  }
  return false;
}

base::flat_set<int> LayerTreeHostImpl::NonFastScrollableNodes(
    const gfx::PointF& device_viewport_point) const {
  base::flat_set<int> non_fast_scrollable_nodes;

  const auto& non_fast_layers =
      active_tree_->FindLayersHitByPointInNonFastScrollableRegion(
          device_viewport_point);
  for (const auto* layer : non_fast_layers)
    non_fast_scrollable_nodes.insert(layer->scroll_tree_index());

  return non_fast_scrollable_nodes;
}

ScrollNode* LayerTreeHostImpl::FindScrollNodeForCompositedScrolling(
    const gfx::PointF& device_viewport_point,
    LayerImpl* layer_impl,
    bool* scroll_on_main_thread,
    uint32_t* main_thread_scrolling_reasons) const {
  DCHECK(scroll_on_main_thread);
  DCHECK(main_thread_scrolling_reasons);
  *main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

  const auto& non_fast_scrollable_nodes =
      NonFastScrollableNodes(device_viewport_point);

  // Walk up the hierarchy and look for a scrollable layer.
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* impl_scroll_node = nullptr;
  if (layer_impl) {
    // If this is a scrollbar layer, we can't directly use the associated
    // scroll_node (because the scroll_node associated with this layer will be
    // the owning scroller's parent). Instead, we first retrieve the scrollable
    // layer corresponding to the scrollbars owner and then use its
    // scroll_tree_index instead.
    int scroll_tree_index = layer_impl->scroll_tree_index();
    if (layer_impl->IsScrollbarLayer()) {
      LayerImpl* owner_scroll_layer = active_tree_->LayerByElementId(
          ToScrollbarLayer(layer_impl)->scroll_element_id());
      scroll_tree_index = owner_scroll_layer->scroll_tree_index();
    }

    ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index);
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      // The content layer can also block attempts to scroll outside the main
      // thread.
      ScrollStatus status = TryScroll(scroll_tree, scroll_node);
      if (IsMainThreadScrolling(status, scroll_node)) {
        *scroll_on_main_thread = true;
        *main_thread_scrolling_reasons = status.main_thread_scrolling_reasons;
        return scroll_node;
      }

      if (non_fast_scrollable_nodes.contains(scroll_node->id)) {
        *scroll_on_main_thread = true;
        *main_thread_scrolling_reasons =
            MainThreadScrollingReason::kNonFastScrollableRegion;
        return scroll_node;
      }

      if (status.thread == InputHandler::SCROLL_ON_IMPL_THREAD &&
          !impl_scroll_node) {
        impl_scroll_node = scroll_node;
      }
    }
  }

  // TODO(bokan): We shouldn't need this - ordinarily all scrolls should pass
  // through the outer viewport. If we aren't able to find a scroller we should
  // return nullptr here and ignore the scroll. However, it looks like on some
  // pages (reddit.com) we start scrolling from the inner node.
  if (!impl_scroll_node)
    impl_scroll_node = InnerViewportScrollNode();

  if (!impl_scroll_node)
    return nullptr;

  impl_scroll_node = GetNodeToScroll(impl_scroll_node);

  // Ensure that final scroll node scrolls on impl thread (crbug.com/625100)
  ScrollStatus status = TryScroll(scroll_tree, impl_scroll_node);
  if (IsMainThreadScrolling(status, impl_scroll_node)) {
    *scroll_on_main_thread = true;
    *main_thread_scrolling_reasons = status.main_thread_scrolling_reasons;
  } else if (non_fast_scrollable_nodes.contains(impl_scroll_node->id)) {
    *scroll_on_main_thread = true;
    *main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNonFastScrollableRegion;
  }

  return impl_scroll_node;
}

InputHandler::ScrollStatus LayerTreeHostImpl::RootScrollBegin(
    ScrollState* scroll_state,
    ui::ScrollInputType type) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::RootScrollBegin");
  if (!OuterViewportScrollNode()) {
    ScrollStatus scroll_status;
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNoScrollingLayer;
    return scroll_status;
  }

  scroll_state->data()->set_current_native_scrolling_element(
      OuterViewportScrollNode()->element_id);
  return ScrollBegin(scroll_state, type);
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollBegin(
    ScrollState* scroll_state,
    ui::ScrollInputType type) {
  DCHECK(scroll_state);
  DCHECK(scroll_state->delta_x() == 0 && scroll_state->delta_y() == 0);

  ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBegin");

  // If this ScrollBegin is non-animated then ensure we cancel any ongoing
  // animated scrolls.
  // TODO(bokan): This preserves existing behavior when we had diverging
  // paths for animated and non-animated scrolls but we should probably
  // decide when it best makes sense to cancel a scroll animation (maybe
  // ScrollBy is a better place to do it).
  if (scroll_state->delta_granularity() ==
      ui::ScrollGranularity::kScrollByPrecisePixel) {
    mutator_host_->ScrollAnimationAbort();
    scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  }

  if (CurrentlyScrollingNode() && type == latched_scroll_type_) {
    // It's possible we haven't yet cleared the CurrentlyScrollingNode if we
    // received a GSE but we're still animating the last scroll. If that's the
    // case, we'll simply un-defer the GSE and continue latching to the same
    // node.
    DCHECK(deferred_scroll_end_);
    deferred_scroll_end_ = false;
    return scroll_status;
  }

  ScrollNode* scrolling_node = nullptr;
  bool scroll_on_main_thread = false;

  // TODO(bokan): ClearCurrentlyScrollingNode shouldn't happen in ScrollBegin,
  // this should only happen in ScrollEnd. We should DCHECK here that the state
  // is cleared instead. https://crbug.com/1016229
  ClearCurrentlyScrollingNode();

  if (auto specified_element_id =
          scroll_state->data()->current_native_scrolling_element()) {
    // If the caller passed in an element_id we can skip all the hit-testing
    // bits and provide a node straight-away.
    auto& scroll_tree = active_tree_->property_trees()->scroll_tree;
    scrolling_node = scroll_tree.FindNodeFromElementId(specified_element_id);

    // We still need to confirm the targeted node exists and can scroll on the
    // compositor.
    if (scrolling_node) {
      scroll_status = TryScroll(active_tree_->property_trees()->scroll_tree,
                                scrolling_node);
      if (IsMainThreadScrolling(scroll_status, scrolling_node))
        scroll_on_main_thread = true;
    }
  } else {
    gfx::Point viewport_point(scroll_state->position_x(),
                              scroll_state->position_y());

    gfx::PointF device_viewport_point = gfx::ScalePoint(
        gfx::PointF(viewport_point), active_tree_->device_scale_factor());
    LayerImpl* layer_impl =
        active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

    if (layer_impl) {
      LayerImpl* first_scrolling_layer_or_scrollbar =
          active_tree_->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
              device_viewport_point);

      // Touch dragging the scrollbar requires falling back to main-thread
      // scrolling.
      if (IsTouchDraggingScrollbar(first_scrolling_layer_or_scrollbar, type)) {
        TRACE_EVENT_INSTANT0("cc", "Scrollbar Scrolling",
                             TRACE_EVENT_SCOPE_THREAD);
        scroll_status.thread = SCROLL_ON_MAIN_THREAD;
        scroll_status.main_thread_scrolling_reasons =
            MainThreadScrollingReason::kScrollbarScrolling;
        return scroll_status;
      } else if (!IsInitialScrollHitTestReliable(
                     layer_impl, first_scrolling_layer_or_scrollbar)) {
        TRACE_EVENT_INSTANT0("cc", "Failed Hit Test", TRACE_EVENT_SCOPE_THREAD);
        scroll_status.thread = SCROLL_UNKNOWN;
        scroll_status.main_thread_scrolling_reasons =
            MainThreadScrollingReason::kFailedHitTest;
        return scroll_status;
      }
    }

    ScrollNode* starting_node = FindScrollNodeForCompositedScrolling(
        device_viewport_point, layer_impl, &scroll_on_main_thread,
        &scroll_status.main_thread_scrolling_reasons);

    // The above finds the ScrollNode that's hit by the given point but we
    // still need to walk up the scroll tree looking for the first node that
    // can consume delta from the scroll state.
    scrolling_node = FindNodeToLatch(scroll_state, starting_node, type);
  }

  if (scroll_on_main_thread) {
    RecordCompositorSlowScrollMetric(type, MAIN_THREAD);
    scroll_status.thread = SCROLL_ON_MAIN_THREAD;
    return scroll_status;
  } else if (!scrolling_node) {
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNoScrollingLayer;
    if (settings_.is_layer_tree_for_subframe) {
      TRACE_EVENT_INSTANT0("cc", "Ignored - No ScrollNode (OOPIF)",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_status.thread = SCROLL_UNKNOWN;
    } else {
      TRACE_EVENT_INSTANT0("cc", "Ignroed - No ScrollNode",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_status.thread = SCROLL_IGNORED;
    }
    return scroll_status;
  }

  DCHECK_EQ(scroll_status.main_thread_scrolling_reasons,
            MainThreadScrollingReason::kNotScrollingOnMain);
  DCHECK_EQ(scroll_status.thread, SCROLL_ON_IMPL_THREAD);

  active_tree_->SetCurrentlyScrollingNode(scrolling_node);

  DidLatchToScroller(*scroll_state, type);

  // If the viewport is scrolling and it cannot consume any delta hints, the
  // scroll event will need to get bubbled if the viewport is for a guest or
  // oopif.
  if (viewport().ShouldScroll(*CurrentlyScrollingNode()) &&
      !viewport().CanScroll(*CurrentlyScrollingNode(), *scroll_state)) {
    scroll_status.bubble = true;
  }

  return scroll_status;
}

ScrollNode* LayerTreeHostImpl::HitTestScrollNode(
    const gfx::PointF& device_viewport_point) const {
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  if (!layer_impl)
    return nullptr;

  // There are some cases where the hit layer may not be correct (e.g. layer
  // squashing). If we detect this case, we can't target a scroll node here.
  {
    LayerImpl* first_scrolling_layer_or_scrollbar =
        active_tree_->FindFirstScrollingLayerOrScrollbarThatIsHitByPoint(
            device_viewport_point);

    if (!IsInitialScrollHitTestReliable(layer_impl,
                                        first_scrolling_layer_or_scrollbar)) {
      TRACE_EVENT_INSTANT0("cc", "Failed Hit Test", TRACE_EVENT_SCOPE_THREAD);
      return nullptr;
    }
  }

  // If we hit a scrollbar layer, get the ScrollNode from its associated
  // scrolling layer, rather than directly from the scrollbar layer. The latter
  // would return the parent scroller's ScrollNode.
  if (layer_impl->IsScrollbarLayer()) {
    layer_impl = active_tree_->LayerByElementId(
        ToScrollbarLayer(layer_impl)->scroll_element_id());
    DCHECK(layer_impl);
  }

  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.Node(layer_impl->scroll_tree_index());

  return GetNodeToScroll(scroll_node);
}

// Requires falling back to main thread scrolling when it hit tests in scrollbar
// from touch.
bool LayerTreeHostImpl::IsTouchDraggingScrollbar(
    LayerImpl* first_scrolling_layer_or_scrollbar,
    ui::ScrollInputType type) {
  return first_scrolling_layer_or_scrollbar &&
         first_scrolling_layer_or_scrollbar->IsScrollbarLayer() &&
         type == ui::ScrollInputType::kTouchscreen;
}

ScrollNode* LayerTreeHostImpl::GetNodeToScroll(ScrollNode* node) const {
  // Blink has a notion of a "root scroller", which is the scroller in a page
  // that is considered to host the main content. Typically this will be the
  // document/LayoutView contents; however, in some situations Blink may choose
  // a sub-scroller (div, iframe) that should scroll with "viewport" behavior.
  // The "root scroller" is the node designated as the outer viewport in CC.
  // See third_party/blink/renderer/core/page/scrolling/README.md for details.
  //
  // "Viewport" scrolling ensures generation of overscroll events, top controls
  // movement, as well as correct multi-viewport panning in pinch-zoom and
  // other scenarios.  We use the viewport's outer scroll node to represent the
  // viewport in the scroll chain and apply scroll delta using CC's Viewport
  // class.
  //
  // Scrolling from position: fixed layers will chain directly up to the inner
  // viewport. Whether that should use the outer viewport (and thus the
  // Viewport class) to scroll or not depends on the root scroller scenario
  // because we don't want setting a root scroller to change the scroll chain
  // order. The |prevent_viewport_scrolling_from_inner| bit is used to
  // communicate that context.
  DCHECK(!node->prevent_viewport_scrolling_from_inner ||
         node->scrolls_inner_viewport);

  if (node->scrolls_inner_viewport &&
      !node->prevent_viewport_scrolling_from_inner) {
    DCHECK(OuterViewportScrollNode());
    return OuterViewportScrollNode();
  }

  return node;
}

bool LayerTreeHostImpl::IsInitialScrollHitTestReliable(
    LayerImpl* layer_impl,
    LayerImpl* first_scrolling_layer_or_scrollbar) const {
  if (!first_scrolling_layer_or_scrollbar)
    return true;

  // Hit tests directly on a composited scrollbar are always reliable.
  if (layer_impl->IsScrollbarLayer()) {
    DCHECK(layer_impl == first_scrolling_layer_or_scrollbar);
    return true;
  }

  ScrollNode* closest_scroll_node = nullptr;
  auto& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.Node(layer_impl->scroll_tree_index());
  for (; scroll_tree.parent(scroll_node);
       scroll_node = scroll_tree.parent(scroll_node)) {
    if (scroll_node->scrollable) {
      closest_scroll_node = GetNodeToScroll(scroll_node);
      break;
    }
  }
  if (!closest_scroll_node)
    return false;

  // If |first_scrolling_layer_or_scrollbar| is not a scrollbar, it must be
  // a scrollabe layer with a scroll node. If this scroll node corresponds to
  // first scrollable ancestor along the scroll tree for |layer_impl|, the hit
  // test has not escaped to other areas of the scroll tree and is reliable.
  if (!first_scrolling_layer_or_scrollbar->IsScrollbarLayer()) {
    return closest_scroll_node->id ==
           first_scrolling_layer_or_scrollbar->scroll_tree_index();
  }

  return false;
}

gfx::Vector2dF LayerTreeHostImpl::ComputeScrollDelta(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& delta) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  float scale_factor = active_tree()->page_scale_factor_for_scroll();

  gfx::Vector2dF adjusted_scroll(delta);
  adjusted_scroll.Scale(1.f / scale_factor);
  adjusted_scroll = UserScrollableDelta(scroll_node, adjusted_scroll);

  gfx::ScrollOffset old_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::ScrollOffset new_offset = scroll_tree.ClampScrollOffsetToLimits(
      old_offset + gfx::ScrollOffset(adjusted_scroll), scroll_node);

  gfx::ScrollOffset scrolled = new_offset - old_offset;
  return gfx::Vector2dF(scrolled.x(), scrolled.y());
}

bool LayerTreeHostImpl::AutoScrollAnimationCreate(const ScrollNode& scroll_node,
                                                  const gfx::Vector2dF& delta,
                                                  float autoscroll_velocity) {
  return ScrollAnimationCreateInternal(scroll_node, delta, base::TimeDelta(),
                                       autoscroll_velocity);
}

bool LayerTreeHostImpl::ScrollAnimationCreate(const ScrollNode& scroll_node,
                                              const gfx::Vector2dF& delta,
                                              base::TimeDelta delayed_by) {
  return ScrollAnimationCreateInternal(scroll_node, delta, delayed_by,
                                       base::nullopt);
}

bool LayerTreeHostImpl::ScrollAnimationCreateInternal(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& delta,
    base::TimeDelta delayed_by,
    base::Optional<float> autoscroll_velocity) {
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;

  const float kEpsilon = 0.1f;
  bool scroll_animated =
      (std::abs(delta.x()) > kEpsilon || std::abs(delta.y()) > kEpsilon) ||
      autoscroll_velocity;
  if (!scroll_animated) {
    scroll_tree.ScrollBy(scroll_node, delta, active_tree());
    TRACE_EVENT_INSTANT0("cc", "no scroll animation due to small delta",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  gfx::ScrollOffset current_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::ScrollOffset target_offset = scroll_tree.ClampScrollOffsetToLimits(
      current_offset + gfx::ScrollOffset(delta), scroll_node);

  // Start the animation one full frame in. Without any offset, the animation
  // doesn't start until next frame, increasing latency, and preventing our
  // input latency tracking architecture from working.
  base::TimeDelta animation_start_offset = CurrentBeginFrameArgs().interval;

  if (autoscroll_velocity) {
    mutator_host_->ImplOnlyAutoScrollAnimationCreate(
        scroll_node.element_id, gfx::ScrollOffset(delta), current_offset,
        autoscroll_velocity.value(), animation_start_offset);
  } else {
    mutator_host_->ImplOnlyScrollAnimationCreate(
        scroll_node.element_id, target_offset, current_offset, delayed_by,
        animation_start_offset);
  }

  SetNeedsOneBeginImplFrame();

  return true;
}

bool LayerTreeHostImpl::CalculateLocalScrollDeltaAndStartPoint(
    const ScrollNode& scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta,
    const ScrollTree& scroll_tree,
    gfx::Vector2dF* out_local_scroll_delta,
    gfx::PointF* out_local_start_point /*= nullptr*/) {
  // Layers with non-invertible screen space transforms should not have passed
  // the scroll hit test in the first place.
  const gfx::Transform screen_space_transform =
      scroll_tree.ScreenSpaceTransform(scroll_node.id);
  DCHECK(screen_space_transform.IsInvertible());
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert =
      screen_space_transform.GetInverse(&inverse_screen_space_transform);
  // TODO(shawnsingh): With the advent of impl-side scrolling for non-root
  // layers, we may need to explicitly handle uninvertible transforms here.
  DCHECK(did_invert);

  float scale_from_viewport_to_screen_space =
      active_tree_->device_scale_factor();
  gfx::PointF screen_space_point =
      gfx::ScalePoint(viewport_point, scale_from_viewport_to_screen_space);

  gfx::Vector2dF screen_space_delta = viewport_delta;
  screen_space_delta.Scale(scale_from_viewport_to_screen_space);

  // Project the scroll start and end points to local layer space to find the
  // scroll delta in layer coordinates.
  bool start_clipped, end_clipped;
  gfx::PointF screen_space_end_point = screen_space_point + screen_space_delta;
  gfx::PointF local_start_point = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &start_clipped);
  gfx::PointF local_end_point = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_end_point, &end_clipped);
  DCHECK(out_local_scroll_delta);
  *out_local_scroll_delta = local_end_point - local_start_point;

  if (out_local_start_point)
    *out_local_start_point = local_start_point;

  if (start_clipped || end_clipped)
    return false;

  return true;
}

gfx::Vector2dF LayerTreeHostImpl::ScrollNodeWithViewportSpaceDelta(
    const ScrollNode& scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta,
    ScrollTree* scroll_tree) {
  gfx::PointF local_start_point;
  gfx::Vector2dF local_scroll_delta;
  if (!CalculateLocalScrollDeltaAndStartPoint(
          scroll_node, viewport_point, viewport_delta, *scroll_tree,
          &local_scroll_delta, &local_start_point)) {
    return gfx::Vector2dF();
  }

  bool scrolls_outer_viewport = scroll_node.scrolls_outer_viewport;
  TRACE_EVENT2("cc", "ScrollNodeWithViewportSpaceDelta", "delta_y",
               local_scroll_delta.y(), "is_outer", scrolls_outer_viewport);

  // Apply the scroll delta.
  gfx::ScrollOffset previous_offset =
      scroll_tree->current_scroll_offset(scroll_node.element_id);
  scroll_tree->ScrollBy(scroll_node, local_scroll_delta, active_tree());
  gfx::ScrollOffset scrolled =
      scroll_tree->current_scroll_offset(scroll_node.element_id) -
      previous_offset;

  TRACE_EVENT_INSTANT1("cc", "ConsumedDelta", TRACE_EVENT_SCOPE_THREAD, "y",
                       scrolled.y());

  // Get the end point in the layer's content space so we can apply its
  // ScreenSpaceTransform.
  gfx::PointF actual_local_end_point =
      local_start_point + gfx::Vector2dF(scrolled.x(), scrolled.y());

  // Calculate the applied scroll delta in viewport space coordinates.
  bool end_clipped;
  const gfx::Transform screen_space_transform =
      scroll_tree->ScreenSpaceTransform(scroll_node.id);
  gfx::PointF actual_screen_space_end_point = MathUtil::MapPoint(
      screen_space_transform, actual_local_end_point, &end_clipped);
  DCHECK(!end_clipped);
  if (end_clipped)
    return gfx::Vector2dF();

  float scale_from_viewport_to_screen_space =
      active_tree_->device_scale_factor();
  gfx::PointF actual_viewport_end_point = gfx::ScalePoint(
      actual_screen_space_end_point, 1.f / scale_from_viewport_to_screen_space);
  return actual_viewport_end_point - viewport_point;
}

static gfx::Vector2dF ScrollNodeWithLocalDelta(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& local_delta,
    float page_scale_factor,
    LayerTreeImpl* layer_tree_impl) {
  bool scrolls_outer_viewport = scroll_node.scrolls_outer_viewport;
  TRACE_EVENT2("cc", "ScrollNodeWithLocalDelta", "delta_y", local_delta.y(),
               "is_outer", scrolls_outer_viewport);

  ScrollTree& scroll_tree = layer_tree_impl->property_trees()->scroll_tree;
  gfx::ScrollOffset previous_offset =
      scroll_tree.current_scroll_offset(scroll_node.element_id);
  gfx::Vector2dF delta = local_delta;
  delta.Scale(1.f / page_scale_factor);
  scroll_tree.ScrollBy(scroll_node, delta, layer_tree_impl);
  gfx::ScrollOffset scrolled =
      scroll_tree.current_scroll_offset(scroll_node.element_id) -
      previous_offset;
  gfx::Vector2dF consumed_scroll(scrolled.x(), scrolled.y());
  consumed_scroll.Scale(page_scale_factor);
  TRACE_EVENT_INSTANT1("cc", "ConsumedDelta", TRACE_EVENT_SCOPE_THREAD, "y",
                       consumed_scroll.y());

  return consumed_scroll;
}

// TODO(danakj): Make this into two functions, one with delta, one with
// viewport_point, no bool required.
gfx::Vector2dF LayerTreeHostImpl::ScrollSingleNode(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& delta,
    const gfx::Point& viewport_point,
    bool is_direct_manipulation,
    ScrollTree* scroll_tree) {
  gfx::Vector2dF adjusted_delta = UserScrollableDelta(scroll_node, delta);

  // Events representing direct manipulation of the screen (such as gesture
  // events) need to be transformed from viewport coordinates to local layer
  // coordinates so that the scrolling contents exactly follow the user's
  // finger. In contrast, events not representing direct manipulation of the
  // screen (such as wheel events) represent a fixed amount of scrolling so we
  // can just apply them directly, but the page scale factor is applied to the
  // scroll delta.
  if (is_direct_manipulation) {
    // For touch-scroll we need to scale the delta here, as the transform tree
    // won't know anything about the external page scale factors used by OOPIFs.
    gfx::Vector2dF scaled_delta(adjusted_delta);
    scaled_delta.Scale(1 / active_tree()->external_page_scale_factor());
    return ScrollNodeWithViewportSpaceDelta(
        scroll_node, gfx::PointF(viewport_point), scaled_delta, scroll_tree);
  }
  float scale_factor = active_tree()->page_scale_factor_for_scroll();
  return ScrollNodeWithLocalDelta(scroll_node, adjusted_delta, scale_factor,
                                  active_tree());
}

void LayerTreeHostImpl::ScrollLatchedScroller(ScrollState* scroll_state,
                                              base::TimeDelta delayed_by) {
  DCHECK(CurrentlyScrollingNode());
  DCHECK(scroll_state);
  DCHECK(latched_scroll_type_.has_value());

  ScrollNode& scroll_node = *CurrentlyScrollingNode();
  const gfx::Vector2dF delta(scroll_state->delta_x(), scroll_state->delta_y());
  TRACE_EVENT2("cc", "LayerTreeHostImpl::ScrollLatchedScroller", "delta_x",
               delta.x(), "delta_y", delta.y());
  gfx::Vector2dF applied_delta;
  gfx::Vector2dF delta_applied_to_content;
  // TODO(tdresser): Use a more rational epsilon. See crbug.com/510550 for
  // details.
  const float kEpsilon = 0.1f;

  if (ShouldAnimateScroll(*scroll_state)) {
    DCHECK(!scroll_state->is_in_inertial_phase());

    if (ElementId id = mutator_host_->ImplOnlyScrollAnimatingElement()) {
      TRACE_EVENT_INSTANT0("cc", "UpdateExistingAnimation",
                           TRACE_EVENT_SCOPE_THREAD);

      ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
      ScrollNode* animating_scroll_node = scroll_tree.FindNodeFromElementId(id);
      DCHECK(animating_scroll_node);

      // Usually the CurrentlyScrollingNode will be the currently animating
      // one. The one exception is the inner viewport. Scrolling the combined
      // viewport will always set the outer viewport as the currently scrolling
      // node. However, if an animation is created on the inner viewport we
      // must use it when updating the animation curve.
      DCHECK(animating_scroll_node->id == scroll_node.id ||
             animating_scroll_node->scrolls_inner_viewport);

      bool animation_updated = ScrollAnimationUpdateTarget(
          *animating_scroll_node, delta, delayed_by);

      if (animation_updated) {
        // Because we updated the animation target, consume delta so we notify
        // the SwapPromiseMonitor to tell it that something happened that will
        // cause a swap in the future.  This will happen within the scope of
        // the dispatch of a gesture scroll update input event. If we don't
        // notify during the handling of the input event, the LatencyInfo
        // associated with the input event will not be added as a swap promise
        // and we won't get any swap results.
        applied_delta = delta;
      } else {
        TRACE_EVENT_INSTANT0("cc", "Didn't Update Animation",
                             TRACE_EVENT_SCOPE_THREAD);
      }
    } else {
      TRACE_EVENT_INSTANT0("cc", "CreateNewAnimation",
                           TRACE_EVENT_SCOPE_THREAD);
      if (scroll_node.scrolls_outer_viewport) {
        applied_delta = viewport().ScrollAnimated(delta, delayed_by);
      } else {
        applied_delta = ComputeScrollDelta(scroll_node, delta);
        ScrollAnimationCreate(scroll_node, applied_delta, delayed_by);
      }
    }

    // Animated scrolling always applied only to the content (i.e. not to the
    // browser controls).
    delta_applied_to_content = delta;
  } else {
    gfx::Point viewport_point(scroll_state->position_x(),
                              scroll_state->position_y());
    if (viewport().ShouldScroll(scroll_node)) {
      // |scrolls_outer_viewport| will only ever be false if the scroll chains
      // up to the viewport without going through the outer viewport scroll
      // node. This is because we normally terminate the scroll chain at the
      // outer viewport node.  For example, if we start scrolling from an
      // element that's not a descendant of the root scroller. In these cases we
      // want to scroll *only* the inner viewport -- to allow panning while
      // zoomed -- but still use Viewport::ScrollBy to also move browser
      // controls if needed.
      Viewport::ScrollResult result = viewport().ScrollBy(
          delta, viewport_point, scroll_state->is_direct_manipulation(),
          latched_scroll_type_ != ui::ScrollInputType::kWheel,
          scroll_node.scrolls_outer_viewport);

      applied_delta = result.consumed_delta;
      delta_applied_to_content = result.content_scrolled_delta;
    } else {
      applied_delta =
          ScrollSingleNode(scroll_node, delta, viewport_point,
                           scroll_state->is_direct_manipulation(),
                           &active_tree_->property_trees()->scroll_tree);
    }
  }

  // If the layer wasn't able to move, try the next one in the hierarchy.
  bool scrolled = std::abs(applied_delta.x()) > kEpsilon;
  scrolled = scrolled || std::abs(applied_delta.y()) > kEpsilon;
  if (!scrolled) {
    // TODO(bokan): This preserves existing behavior by not allowing tiny
    // scrolls to produce overscroll but is inconsistent in how delta gets
    // chained up. We need to clean this up.
    if (scroll_node.scrolls_outer_viewport)
      scroll_state->ConsumeDelta(applied_delta.x(), applied_delta.y());
    return;
  }

  if (!viewport().ShouldScroll(scroll_node)) {
    // If the applied delta is within 45 degrees of the input
    // delta, bail out to make it easier to scroll just one layer
    // in one direction without affecting any of its parents.
    float angle_threshold = 45;
    if (MathUtil::SmallestAngleBetweenVectors(applied_delta, delta) <
        angle_threshold) {
      applied_delta = delta;
    } else {
      // Allow further movement only on an axis perpendicular to the direction
      // in which the layer moved.
      applied_delta = MathUtil::ProjectVector(delta, applied_delta);
    }
    delta_applied_to_content = applied_delta;
  }

  scroll_state->set_caused_scroll(
      std::abs(delta_applied_to_content.x()) > kEpsilon,
      std::abs(delta_applied_to_content.y()) > kEpsilon);
  scroll_state->ConsumeDelta(applied_delta.x(), applied_delta.y());
}

static bool CanPropagate(ScrollNode* scroll_node, float x, float y) {
  return (x == 0 || scroll_node->overscroll_behavior.x ==
                        OverscrollBehavior::kOverscrollBehaviorTypeAuto) &&
         (y == 0 || scroll_node->overscroll_behavior.y ==
                        OverscrollBehavior::kOverscrollBehaviorTypeAuto);
}

ScrollNode* LayerTreeHostImpl::FindNodeToLatch(ScrollState* scroll_state,
                                               ScrollNode* starting_node,
                                               ui::ScrollInputType type) {
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = nullptr;
  for (ScrollNode* cur_node = starting_node; cur_node;
       cur_node = scroll_tree.parent(cur_node)) {
    if (viewport().ShouldScroll(*cur_node)) {
      // Don't chain scrolls past a viewport node. Once we reach that, we
      // should scroll using the appropriate viewport node which may not be
      // |cur_node|.
      scroll_node = GetNodeToScroll(cur_node);
      break;
    }

    if (!cur_node->scrollable)
      continue;

    // For UX reasons, autoscrolling should always latch to the top-most
    // scroller, even if it can't scroll in the initial direction.
    if (type == ui::ScrollInputType::kAutoscroll ||
        CanConsumeDelta(*scroll_state, *cur_node)) {
      scroll_node = cur_node;
      break;
    }

    float delta_x = scroll_state->is_beginning() ? scroll_state->delta_x_hint()
                                                 : scroll_state->delta_x();
    float delta_y = scroll_state->is_beginning() ? scroll_state->delta_y_hint()
                                                 : scroll_state->delta_y();

    if (!CanPropagate(cur_node, delta_x, delta_y)) {
      // If we reach a node with non-auto overscroll-behavior and we still
      // haven't latched, we must latch to it. Consider a fully scrolled node
      // with non-auto overscroll-behavior: we are not allowed to further
      // chain scroll delta passed to it in the current direction but if we
      // reverse direction we should scroll it so we must be latched to it.
      scroll_node = cur_node;
      scroll_state->set_is_scroll_chain_cut(true);
      break;
    }
  }

  return scroll_node;
}

void LayerTreeHostImpl::DidLatchToScroller(const ScrollState& scroll_state,
                                           ui::ScrollInputType type) {
  DCHECK(CurrentlyScrollingNode());
  deferred_scroll_end_ = false;
  browser_controls_offset_manager_->ScrollBegin();
  mutator_host_->ScrollAnimationAbort();

  scroll_affects_scroll_handler_ = active_tree_->have_scroll_event_handlers();
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  last_latched_scroller_ = CurrentlyScrollingNode()->element_id;
  latched_scroll_type_ = type;
  last_scroll_begin_state_ = scroll_state;

  client_->RenewTreePriority();
  RecordCompositorSlowScrollMetric(type, CC_THREAD);

  UpdateScrollSourceInfo(scroll_state, type);
}

bool LayerTreeHostImpl::CanConsumeDelta(const ScrollState& scroll_state,
                                        const ScrollNode& scroll_node) {
  gfx::Vector2dF delta_to_scroll;
  if (scroll_state.is_beginning()) {
    delta_to_scroll = gfx::Vector2dF(scroll_state.delta_x_hint(),
                                     scroll_state.delta_y_hint());
  } else {
    delta_to_scroll =
        gfx::Vector2dF(scroll_state.delta_x(), scroll_state.delta_y());
  }

  if (delta_to_scroll == gfx::Vector2dF())
    return true;

  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  if (scroll_state.is_direct_manipulation()) {
    gfx::Vector2dF local_scroll_delta;
    if (!CalculateLocalScrollDeltaAndStartPoint(
            scroll_node,
            gfx::PointF(scroll_state.position_x(), scroll_state.position_y()),
            delta_to_scroll, scroll_tree, &local_scroll_delta)) {
      return false;
    }
    delta_to_scroll = local_scroll_delta;
  } else if (scroll_state.delta_granularity() ==
             ui::ScrollGranularity::kScrollByPercentage) {
    delta_to_scroll =
        ResolveScrollPercentageToPixels(scroll_node, delta_to_scroll);
  }

  if (ComputeScrollDelta(scroll_node, delta_to_scroll) != gfx::Vector2dF())
    return true;

  return false;
}

void LayerTreeHostImpl::UpdateImageDecodingHints(
    base::flat_map<PaintImage::Id, PaintImage::DecodingMode>
        decoding_mode_map) {
  tile_manager_.checker_image_tracker().UpdateImageDecodingHints(
      std::move(decoding_mode_map));
}

void LayerTreeHostImpl::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  render_frame_metadata_observer_ = std::move(observer);
  render_frame_metadata_observer_->BindToCurrentThread();
}

bool LayerTreeHostImpl::ShouldAnimateScroll(
    const ScrollState& scroll_state) const {
  if (!settings_.enable_smooth_scroll)
    return false;

  bool has_precise_scroll_deltas = scroll_state.delta_granularity() ==
                                   ui::ScrollGranularity::kScrollByPrecisePixel;

#if defined(OS_MACOSX)
  if (has_precise_scroll_deltas)
    return false;

  // Mac does not smooth scroll wheel events (crbug.com/574283). We allow tests
  // to force it on.
  return latched_scroll_type_ == ui::ScrollInputType::kScrollbar
             ? true
             : force_smooth_wheel_scrolling_for_testing_;
#else
  return !has_precise_scroll_deltas;
#endif
}

InputHandlerScrollResult LayerTreeHostImpl::ScrollUpdate(
    ScrollState* scroll_state,
    base::TimeDelta delayed_by) {
  DCHECK(scroll_state);

  // The current_native_scrolling_element should only be set for ScrollBegin.
  DCHECK(!scroll_state->data()->current_native_scrolling_element());

  if (!CurrentlyScrollingNode())
    return InputHandlerScrollResult();

  last_scroll_update_state_ = *scroll_state;

  bool is_delta_percent_units = scroll_state->delta_granularity() ==
                                ui::ScrollGranularity::kScrollByPercentage;
  if (is_delta_percent_units) {
    gfx::Vector2dF resolvedScrollDelta = ResolveScrollPercentageToPixels(
        *CurrentlyScrollingNode(),
        gfx::Vector2dF(scroll_state->delta_x(), scroll_state->delta_y()));

    scroll_state->data()->delta_x = resolvedScrollDelta.x();
    scroll_state->data()->delta_y = resolvedScrollDelta.y();
    scroll_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;
  }

  scroll_accumulated_this_frame_ +=
      gfx::Vector2dF(scroll_state->delta_x(), scroll_state->delta_y());

  // Flash the overlay scrollbar even if the scroll delta is 0.
  if (settings_.scrollbar_flash_after_any_scroll_update) {
    FlashAllScrollbars(false);
  } else {
    ScrollbarAnimationController* animation_controller =
        ScrollbarAnimationControllerForElementId(
            CurrentlyScrollingNode()->element_id);
    if (animation_controller)
      animation_controller->WillUpdateScroll();
  }

  float initial_top_controls_offset =
      browser_controls_offset_manager_->ControlsTopOffset();

  ScrollLatchedScroller(scroll_state, delayed_by);

  bool did_scroll_x = scroll_state->caused_scroll_x();
  bool did_scroll_y = scroll_state->caused_scroll_y();
  did_scroll_x_for_scroll_gesture_ |= did_scroll_x;
  did_scroll_y_for_scroll_gesture_ |= did_scroll_y;
  bool did_scroll_content = did_scroll_x || did_scroll_y;
  if (did_scroll_content) {
    ShowScrollbarsForImplScroll(CurrentlyScrollingNode()->element_id);
    client_->RenewTreePriority();
    if (!ShouldAnimateScroll(*scroll_state)) {
      // SetNeedsRedraw is only called in non-animated cases since an animation
      // won't actually update any scroll offsets until a frame produces a
      // tick. Scheduling a redraw here before ticking means the draw gets
      // aborted due to no damage and the swap promises broken so a LatencyInfo
      // won't be recorded.
      SetNeedsRedraw();
    }
  } else {
    overscroll_delta_for_main_thread_ +=
        gfx::Vector2dF(scroll_state->delta_x(), scroll_state->delta_y());
  }

  client_->SetNeedsCommitOnImplThread();

  // Scrolling along an axis resets accumulated root overscroll for that axis.
  if (did_scroll_x)
    accumulated_root_overscroll_.set_x(0);
  if (did_scroll_y)
    accumulated_root_overscroll_.set_y(0);

  gfx::Vector2dF unused_root_delta;
  if (viewport().ShouldScroll(*CurrentlyScrollingNode())) {
    unused_root_delta =
        gfx::Vector2dF(scroll_state->delta_x(), scroll_state->delta_y());
  }

  // When inner viewport is unscrollable, disable overscrolls.
  if (auto* inner_viewport_scroll_node = InnerViewportScrollNode()) {
    unused_root_delta =
        UserScrollableDelta(*inner_viewport_scroll_node, unused_root_delta);
  }

  accumulated_root_overscroll_ += unused_root_delta;

  bool did_scroll_top_controls =
      initial_top_controls_offset !=
      browser_controls_offset_manager_->ControlsTopOffset();

  InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = did_scroll_content || did_scroll_top_controls;
  scroll_result.did_overscroll_root = !unused_root_delta.IsZero();
  scroll_result.accumulated_root_overscroll = accumulated_root_overscroll_;
  scroll_result.unused_scroll_delta = unused_root_delta;
  scroll_result.overscroll_behavior =
      scroll_state->is_scroll_chain_cut()
          ? OverscrollBehavior(OverscrollBehavior::OverscrollBehaviorType::
                                   kOverscrollBehaviorTypeNone)
          : active_tree()->overscroll_behavior();

  if (scroll_result.did_scroll) {
    // Scrolling can change the root scroll offset, so inform the synchronous
    // input handler.
    UpdateRootLayerStateForSynchronousInputHandler();
  }

  scroll_result.current_visual_offset =
      ScrollOffsetToVector2dF(GetVisualScrollOffset(*CurrentlyScrollingNode()));
  float scale_factor = active_tree()->page_scale_factor_for_scroll();
  scroll_result.current_visual_offset.Scale(scale_factor);

  // Run animations which need to respond to updated scroll offset.
  mutator_host_->TickScrollAnimations(
      CurrentBeginFrameArgs().frame_time,
      active_tree_->property_trees()->scroll_tree);

  return scroll_result;
}

void LayerTreeHostImpl::RequestUpdateForSynchronousInputHandler() {
  UpdateRootLayerStateForSynchronousInputHandler();
}

static gfx::Vector2dF ContentToPhysical(const gfx::Vector2dF& content_delta,
                                        float page_scale_factor) {
  gfx::Vector2dF physical_delta = content_delta;
  physical_delta.Scale(page_scale_factor);
  return physical_delta;
}

void LayerTreeHostImpl::SetSynchronousInputHandlerRootScrollOffset(
    const gfx::ScrollOffset& root_content_offset) {
  TRACE_EVENT2(
      "cc", "LayerTreeHostImpl::SetSynchronousInputHandlerRootScrollOffset",
      "offset_x", root_content_offset.x(), "offset_y", root_content_offset.y());

  gfx::Vector2dF physical_delta = ContentToPhysical(
      root_content_offset.DeltaFrom(viewport().TotalScrollOffset()),
      active_tree()->page_scale_factor_for_scroll());

  bool changed = !viewport()
                      .ScrollBy(physical_delta,
                                /*viewport_point=*/gfx::Point(),
                                /*is_direct_manipulation=*/false,
                                /*affect_browser_controls=*/false,
                                /*scroll_outer_viewport=*/true)
                      .consumed_delta.IsZero();
  if (!changed)
    return;

  ShowScrollbarsForImplScroll(OuterViewportScrollNode()->element_id);
  client_->SetNeedsCommitOnImplThread();
  // After applying the synchronous input handler's scroll offset, tell it what
  // we ended up with.
  UpdateRootLayerStateForSynchronousInputHandler();
  SetFullViewportDamage();
  SetNeedsRedraw();
}

bool LayerTreeHostImpl::SnapAtScrollEnd() {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data.has_value())
    return false;

  SnapContainerData& data = scroll_node->snap_container_data.value();
  gfx::ScrollOffset current_position = GetVisualScrollOffset(*scroll_node);

  // You might think that if a scroll never received a scroll update we could
  // just drop the snap. However, if the GSB+GSE arrived while we were mid-snap
  // from a previous gesture, this would leave the scroller at a
  // non-snap-point.
  DCHECK(last_scroll_update_state_ || last_scroll_begin_state_);
  ScrollState& last_scroll_state = last_scroll_update_state_
                                       ? *last_scroll_update_state_
                                       : *last_scroll_begin_state_;

  bool imprecise_wheel_scrolling =
      latched_scroll_type_ == ui::ScrollInputType::kWheel &&
      last_scroll_state.delta_granularity() !=
          ui::ScrollGranularity::kScrollByPrecisePixel;
  gfx::ScrollOffset last_scroll_delta = last_scroll_state.DeltaOrHint();

  std::unique_ptr<SnapSelectionStrategy> strategy;

  if (imprecise_wheel_scrolling && !last_scroll_delta.IsZero()) {
    // This was an imprecise wheel scroll so use direction snapping.
    strategy = SnapSelectionStrategy::CreateForDirection(
        current_position, last_scroll_delta, true);
  } else {
    strategy = SnapSelectionStrategy::CreateForEndPosition(
        current_position, did_scroll_x_for_scroll_gesture_,
        did_scroll_y_for_scroll_gesture_);
  }

  gfx::ScrollOffset snap_position;
  TargetSnapAreaElementIds snap_target_ids;
  if (!data.FindSnapPosition(*strategy, &snap_position, &snap_target_ids))
    return false;

  if (viewport().ShouldScroll(*scroll_node)) {
    // Flash the overlay scrollbar even if the scroll delta is 0.
    if (settings_.scrollbar_flash_after_any_scroll_update) {
      FlashAllScrollbars(false);
    } else {
      ScrollbarAnimationController* animation_controller =
          ScrollbarAnimationControllerForElementId(scroll_node->element_id);
      if (animation_controller)
        animation_controller->WillUpdateScroll();
    }
  }

  gfx::Vector2dF delta =
      ScrollOffsetToVector2dF(snap_position - current_position);
  bool did_animate = false;
  if (scroll_node->scrolls_outer_viewport) {
    gfx::Vector2dF scaled_delta(delta);
    scaled_delta.Scale(active_tree()->page_scale_factor_for_scroll());
    gfx::Vector2dF consumed_delta =
        viewport().ScrollAnimated(scaled_delta, base::TimeDelta());
    did_animate = !consumed_delta.IsZero();
  } else {
    did_animate = ScrollAnimationCreate(*scroll_node, delta, base::TimeDelta());
  }
  DCHECK(!IsAnimatingForSnap());
  if (did_animate) {
    // The snap target will be set when the animation is completed.
    scroll_animating_snap_target_ids_ = snap_target_ids;
  } else if (data.SetTargetSnapAreaElementIds(snap_target_ids)) {
    updated_snapped_elements_.insert(scroll_node->element_id);
    client_->SetNeedsCommitOnImplThread();
  }
  return did_animate;
}

bool LayerTreeHostImpl::IsAnimatingForSnap() const {
  return scroll_animating_snap_target_ids_ != TargetSnapAreaElementIds();
}

gfx::ScrollOffset LayerTreeHostImpl::GetVisualScrollOffset(
    const ScrollNode& scroll_node) const {
  if (scroll_node.scrolls_outer_viewport)
    return viewport().TotalScrollOffset();
  return active_tree()->property_trees()->scroll_tree.current_scroll_offset(
      scroll_node.element_id);
}

bool LayerTreeHostImpl::GetSnapFlingInfoAndSetAnimatingSnapTarget(
    const gfx::Vector2dF& natural_displacement_in_viewport,
    gfx::Vector2dF* out_initial_position,
    gfx::Vector2dF* out_target_position) {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (!scroll_node || !scroll_node->snap_container_data.has_value())
    return false;
  const SnapContainerData& data = scroll_node->snap_container_data.value();

  float scale_factor = active_tree()->page_scale_factor_for_scroll();
  gfx::Vector2dF natural_displacement_in_content =
      gfx::ScaleVector2d(natural_displacement_in_viewport, 1.f / scale_factor);

  gfx::ScrollOffset current_offset = GetVisualScrollOffset(*scroll_node);
  *out_initial_position = ScrollOffsetToVector2dF(current_offset);

  // CC side always uses fractional scroll deltas.
  bool use_fractional_offsets = true;
  gfx::ScrollOffset snap_offset;
  TargetSnapAreaElementIds snap_target_ids;
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndAndDirection(
          current_offset, gfx::ScrollOffset(natural_displacement_in_content),
          use_fractional_offsets);
  if (!data.FindSnapPosition(*strategy, &snap_offset, &snap_target_ids))
    return false;
  scroll_animating_snap_target_ids_ = snap_target_ids;

  *out_target_position = ScrollOffsetToVector2dF(snap_offset);
  out_target_position->Scale(scale_factor);
  out_initial_position->Scale(scale_factor);
  return true;
}

void LayerTreeHostImpl::ScrollEndForSnapFling(bool did_finish) {
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  // When a snap fling animation reaches its intended target then we update the
  // scrolled node's snap targets. This also ensures blink learns about the new
  // snap targets for this scrolling element.
  if (did_finish && scroll_node &&
      scroll_node->snap_container_data.has_value()) {
    scroll_node->snap_container_data.value().SetTargetSnapAreaElementIds(
        scroll_animating_snap_target_ids_);
    updated_snapped_elements_.insert(scroll_node->element_id);
    client_->SetNeedsCommitOnImplThread();
  }
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  ScrollEnd(false /* should_snap */);
}

void LayerTreeHostImpl::ClearCurrentlyScrollingNode() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ClearCurrentlyScrollingNode");
  active_tree_->ClearCurrentlyScrollingNode();
  scroll_affects_scroll_handler_ = false;
  accumulated_root_overscroll_ = gfx::Vector2dF();
  did_scroll_x_for_scroll_gesture_ = false;
  did_scroll_y_for_scroll_gesture_ = false;
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  latched_scroll_type_.reset();
  last_scroll_update_state_.reset();
  last_scroll_begin_state_.reset();
}

void LayerTreeHostImpl::ScrollEnd(bool should_snap) {
  scrollbar_controller_->ResetState();
  if (!CurrentlyScrollingNode())
    return;

  // Note that if we deferred the scroll end then we should not snap. We will
  // snap once we deliver the deferred scroll end.
  if (mutator_host_->ImplOnlyScrollAnimatingElement()) {
    DCHECK(!deferred_scroll_end_);
    deferred_scroll_end_ = true;
    return;
  }

  if (should_snap && SnapAtScrollEnd()) {
    deferred_scroll_end_ = true;
    return;
  }

  DCHECK(latched_scroll_type_.has_value());

  browser_controls_offset_manager_->ScrollEnd();

  ClearCurrentlyScrollingNode();
  deferred_scroll_end_ = false;
  scroll_gesture_did_end_ = true;
  client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::RecordScrollBegin(
    ui::ScrollInputType input_type,
    ScrollBeginThreadState scroll_start_state) {
  auto tracker_type = GetTrackerTypeForScroll(input_type);
  DCHECK_NE(tracker_type, FrameSequenceTrackerType::kMaxType);
  auto* metrics = frame_trackers_.StartSequence(tracker_type);
  if (!metrics)
    return;

  // The main-thread is the 'scrolling thread' if:
  //   (1) the scroll is driven by the main thread, or
  //   (2) the scroll is driven by the compositor, but blocked on the main
  //       thread.
  // Otherwise, the compositor-thread is the 'scrolling thread'.
  // TODO(crbug.com/1060712): We should also count 'main thread' as the
  // 'scrolling thread' if the layer being scrolled has scroll-event handlers.
  FrameSequenceMetrics::ThreadType scrolling_thread;
  switch (scroll_start_state) {
    case ScrollBeginThreadState::kScrollingOnCompositor:
      scrolling_thread = FrameSequenceMetrics::ThreadType::kCompositor;
      break;
    case ScrollBeginThreadState::kScrollingOnMain:
    case ScrollBeginThreadState::kScrollingOnCompositorBlockedOnMain:
      scrolling_thread = FrameSequenceMetrics::ThreadType::kMain;
      break;
  }
  metrics->SetScrollingThread(scrolling_thread);
}

void LayerTreeHostImpl::RecordScrollEnd(ui::ScrollInputType input_type) {
  frame_trackers_.StopSequence(GetTrackerTypeForScroll(input_type));
}

InputHandlerPointerResult LayerTreeHostImpl::MouseDown(
    const gfx::PointF& viewport_point,
    bool shift_modifier) {
  ScrollbarAnimationController* animation_controller =
      ScrollbarAnimationControllerForElementId(
          scroll_element_id_mouse_currently_over_);
  if (animation_controller) {
    animation_controller->DidMouseDown();
    scroll_element_id_mouse_currently_captured_ =
        scroll_element_id_mouse_currently_over_;
  }

  InputHandlerPointerResult result;
  if (settings().compositor_threaded_scrollbar_scrolling) {
    result = scrollbar_controller_->HandlePointerDown(viewport_point,
                                                      shift_modifier);
  }

  return result;
}

InputHandlerPointerResult LayerTreeHostImpl::MouseUp(
    const gfx::PointF& viewport_point) {
  if (scroll_element_id_mouse_currently_captured_) {
    ScrollbarAnimationController* animation_controller =
        ScrollbarAnimationControllerForElementId(
            scroll_element_id_mouse_currently_captured_);

    scroll_element_id_mouse_currently_captured_ = ElementId();

    if (animation_controller)
      animation_controller->DidMouseUp();
  }

  InputHandlerPointerResult result;
  if (settings().compositor_threaded_scrollbar_scrolling)
    result = scrollbar_controller_->HandlePointerUp(viewport_point);

  return result;
}

InputHandlerPointerResult LayerTreeHostImpl::MouseMoveAt(
    const gfx::Point& viewport_point) {
  InputHandlerPointerResult result;
  if (settings().compositor_threaded_scrollbar_scrolling) {
    result =
        scrollbar_controller_->HandlePointerMove(gfx::PointF(viewport_point));
  }

  // Early out if there are no animation controllers and avoid the hit test.
  // This happens on platforms without animated scrollbars.
  if (scrollbar_animation_controllers_.empty())
    return result;

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());
  ScrollNode* scroll_node = HitTestScrollNode(device_viewport_point);

  // The hit test can fail in some cases, e.g. we don't know if a region of a
  // squashed layer has content or is empty.
  if (!scroll_node)
    return result;

  // Scrollbars for the viewport are registered with the outer viewport layer.
  if (scroll_node->scrolls_inner_viewport)
    scroll_node = OuterViewportScrollNode();

  ElementId scroll_element_id = scroll_node->element_id;
  ScrollbarAnimationController* new_animation_controller =
      ScrollbarAnimationControllerForElementId(scroll_element_id);
  if (scroll_element_id != scroll_element_id_mouse_currently_over_) {
    ScrollbarAnimationController* old_animation_controller =
        ScrollbarAnimationControllerForElementId(
            scroll_element_id_mouse_currently_over_);
    if (old_animation_controller)
      old_animation_controller->DidMouseLeave();

    scroll_element_id_mouse_currently_over_ = scroll_element_id;

    // Experiment: Enables will flash scrollbar when user move mouse enter a
    // scrollable area.
    if (settings_.scrollbar_flash_when_mouse_enter && new_animation_controller)
      new_animation_controller->DidScrollUpdate();
  }

  if (!new_animation_controller)
    return result;

  new_animation_controller->DidMouseMove(device_viewport_point);

  return result;
}

void LayerTreeHostImpl::MouseLeave() {
  for (auto& pair : scrollbar_animation_controllers_)
    pair.second->DidMouseLeave();
  scroll_element_id_mouse_currently_over_ = ElementId();
}

ElementId LayerTreeHostImpl::FindFrameElementIdAtPoint(
    const gfx::PointF& viewport_point) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());
  return active_tree_->FindFrameElementIdAtPoint(device_viewport_point);
}

void LayerTreeHostImpl::PinchGestureBegin() {
  pinch_gesture_active_ = true;
  client_->RenewTreePriority();
  pinch_gesture_end_should_clear_scrolling_node_ = !CurrentlyScrollingNode();

  TRACE_EVENT_INSTANT1("cc", "SetCurrentlyScrollingNode PinchGestureBegin",
                       TRACE_EVENT_SCOPE_THREAD, "isNull",
                       OuterViewportScrollNode() ? false : true);
  active_tree_->SetCurrentlyScrollingNode(OuterViewportScrollNode());
  browser_controls_offset_manager_->PinchBegin();
  frame_trackers_.StartSequence(FrameSequenceTrackerType::kPinchZoom);
}

void LayerTreeHostImpl::PinchGestureUpdate(float magnify_delta,
                                           const gfx::Point& anchor) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::PinchGestureUpdate");
  if (!InnerViewportScrollNode())
    return;
  has_pinch_zoomed_ = true;
  viewport().PinchUpdate(magnify_delta, anchor);
  client_->SetNeedsCommitOnImplThread();
  SetNeedsRedraw();
  client_->RenewTreePriority();
  // Pinching can change the root scroll offset, so inform the synchronous input
  // handler.
  UpdateRootLayerStateForSynchronousInputHandler();
}

void LayerTreeHostImpl::PinchGestureEnd(const gfx::Point& anchor,
                                        bool snap_to_min) {
  pinch_gesture_active_ = false;
  if (pinch_gesture_end_should_clear_scrolling_node_) {
    pinch_gesture_end_should_clear_scrolling_node_ = false;
    ClearCurrentlyScrollingNode();
  }
  viewport().PinchEnd(anchor, snap_to_min);
  browser_controls_offset_manager_->PinchEnd();
  client_->SetNeedsCommitOnImplThread();
  // When a pinch ends, we may be displaying content cached at incorrect scales,
  // so updating draw properties and drawing will ensure we are using the right
  // scales that we want when we're not inside a pinch.
  active_tree_->set_needs_update_draw_properties();
  SetNeedsRedraw();
  frame_trackers_.StopSequence(FrameSequenceTrackerType::kPinchZoom);
}

void LayerTreeHostImpl::CollectScrollDeltas(ScrollAndScaleSet* scroll_info) {
  if (active_tree_->LayerListIsEmpty())
    return;

  ElementId inner_viewport_scroll_element_id =
      InnerViewportScrollNode() ? InnerViewportScrollNode()->element_id
                                : ElementId();

  active_tree_->property_trees()->scroll_tree.CollectScrollDeltas(
      scroll_info, inner_viewport_scroll_element_id,
      active_tree_->settings().commit_fractional_scroll_deltas,
      updated_snapped_elements_);
  updated_snapped_elements_.clear();
}

void LayerTreeHostImpl::CollectScrollbarUpdates(
    ScrollAndScaleSet* scroll_info) const {
  scroll_info->scrollbars.reserve(scrollbar_animation_controllers_.size());
  for (auto& pair : scrollbar_animation_controllers_) {
    scroll_info->scrollbars.push_back(
        {pair.first, pair.second->ScrollbarsHidden()});
  }
}

std::unique_ptr<ScrollAndScaleSet> LayerTreeHostImpl::ProcessScrollDeltas() {
  auto scroll_info = std::make_unique<ScrollAndScaleSet>();

  CollectScrollDeltas(scroll_info.get());
  CollectScrollbarUpdates(scroll_info.get());
  scroll_info->page_scale_delta =
      active_tree_->page_scale_factor()->PullDeltaForMainThread();
  scroll_info->is_pinch_gesture_active = active_tree_->PinchGestureActive();
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this DCHECK as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(!settings().is_layer_tree_for_subframe ||
         scroll_info->page_scale_delta == 1.f);
  scroll_info->top_controls_delta =
      active_tree()->top_controls_shown_ratio()->PullDeltaForMainThread();
  scroll_info->bottom_controls_delta =
      active_tree()->bottom_controls_shown_ratio()->PullDeltaForMainThread();
  scroll_info->elastic_overscroll_delta =
      active_tree_->elastic_overscroll()->PullDeltaForMainThread();
  scroll_info->swap_promises.swap(swap_promises_for_main_thread_scroll_update_);

  // Record and reset scroll source flags.
  if (has_scrolled_by_wheel_) {
    scroll_info->manipulation_info |= kManipulationInfoHasScrolledByWheel;
  }
  if (has_scrolled_by_touch_) {
    scroll_info->manipulation_info |= kManipulationInfoHasScrolledByTouch;
  }
  if (has_scrolled_by_precisiontouchpad_) {
    scroll_info->manipulation_info |=
        kManipulationInfoHasScrolledByPrecisionTouchPad;
  }
  if (has_pinch_zoomed_) {
    scroll_info->manipulation_info |= kManipulationInfoHasPinchZoomed;
  }

  has_scrolled_by_wheel_ = has_scrolled_by_touch_ =
      has_scrolled_by_precisiontouchpad_ = has_pinch_zoomed_ = false;
  scroll_info->scroll_gesture_did_end = scroll_gesture_did_end_;

  // Record and reset overscroll delta.
  scroll_info->overscroll_delta = overscroll_delta_for_main_thread_;
  overscroll_delta_for_main_thread_ = gfx::Vector2dF();

  // Use the |last_latched_scroller_| rather than the |CurrentlyScrollingNode|
  // since the latter may be cleared by a GSE before we've committed these
  // values to the main thread.
  scroll_info->scroll_latched_element_id = last_latched_scroller_;
  if (scroll_gesture_did_end_)
    last_latched_scroller_ = ElementId();

  if (browser_controls_manager()) {
    scroll_info->browser_controls_constraint =
        browser_controls_manager()->PullConstraintForMainThread(
            &scroll_info->browser_controls_constraint_changed);
  }

  scroll_gesture_did_end_ = false;
  return scroll_info;
}

void LayerTreeHostImpl::SetFullViewportDamage() {
  // In non-Android-WebView cases, we expect GetDeviceViewport() to be the same
  // as internal_device_viewport(), so the full-viewport damage rect is just
  // the internal viewport rect. In the case of Android WebView,
  // GetDeviceViewport returns the external viewport, but we still want to use
  // the internal viewport's origin for setting the damage.
  // See https://chromium-review.googlesource.com/c/chromium/src/+/1257555.
  SetViewportDamage(gfx::Rect(active_tree_->internal_device_viewport().origin(),
                              active_tree_->GetDeviceViewport().size()));
}

bool LayerTreeHostImpl::AnimatePageScale(base::TimeTicks monotonic_time) {
  if (!page_scale_animation_)
    return false;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();

  if (!page_scale_animation_->IsAnimationStarted())
    page_scale_animation_->StartAnimation(monotonic_time);

  active_tree_->SetPageScaleOnActiveTree(
      page_scale_animation_->PageScaleFactorAtTime(monotonic_time));
  gfx::ScrollOffset next_scroll = gfx::ScrollOffset(
      page_scale_animation_->ScrollOffsetAtTime(monotonic_time));

  viewport().ScrollByInnerFirst(next_scroll.DeltaFrom(scroll_total));

  if (page_scale_animation_->IsAnimationCompleteAtTime(monotonic_time)) {
    page_scale_animation_ = nullptr;
    client_->SetNeedsCommitOnImplThread();
    client_->RenewTreePriority();
    client_->DidCompletePageScaleAnimationOnImplThread();
  } else {
    SetNeedsOneBeginImplFrame();
  }
  return true;
}

bool LayerTreeHostImpl::AnimateBrowserControls(base::TimeTicks time) {
  if (!browser_controls_offset_manager_->HasAnimation())
    return false;

  gfx::Vector2dF scroll_delta = browser_controls_offset_manager_->Animate(time);

  if (browser_controls_offset_manager_->HasAnimation())
    SetNeedsOneBeginImplFrame();

  if (active_tree_->TotalScrollOffset().y() == 0.f)
    return false;

  if (scroll_delta.IsZero())
    return false;

  viewport().ScrollBy(scroll_delta,
                      /*viewport_point=*/gfx::Point(),
                      /*is_wheel_scroll=*/false,
                      /*affect_browser_controls=*/false,
                      /*scroll_outer_viewport=*/true);
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
  return true;
}

bool LayerTreeHostImpl::AnimateScrollbars(base::TimeTicks monotonic_time) {
  bool animated = false;
  for (auto& pair : scrollbar_animation_controllers_)
    animated |= pair.second->Animate(monotonic_time);
  return animated;
}

bool LayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonic_time,
                                      bool is_active_tree) {
  const ScrollTree& scroll_tree =
      is_active_tree ? active_tree_->property_trees()->scroll_tree
                     : pending_tree_->property_trees()->scroll_tree;
  const bool animated = mutator_host_->TickAnimations(
      monotonic_time, scroll_tree, is_active_tree);

  // TODO(crbug.com/551134): Only do this if the animations are on the active
  // tree, or if they are on the pending tree waiting for some future time to
  // start.
  // TODO(crbug.com/551138): We currently have a single signal from the
  // animation_host, so on the last frame of an animation we will
  // still request an extra SetNeedsAnimate here.
  if (animated) {
    // TODO(crbug.com/1039750): If only scroll animations present, schedule a
    // frame only if scroll changes.
    SetNeedsOneBeginImplFrame();
    frame_trackers_.StartSequence(
        FrameSequenceTrackerType::kCompositorAnimation);
  } else {
    frame_trackers_.StopSequence(
        FrameSequenceTrackerType::kCompositorAnimation);
  }

  // TODO(crbug.com/551138): We could return true only if the animations are on
  // the active tree. There's no need to cause a draw to take place from
  // animations starting/ticking on the pending tree.
  return animated;
}

void LayerTreeHostImpl::UpdateAnimationState(bool start_ready_animations) {
  const bool has_active_animations = mutator_host_->UpdateAnimationState(
      start_ready_animations, mutator_events_.get());

  if (has_active_animations) {
    SetNeedsOneBeginImplFrame();
    if (!mutator_events_->IsEmpty())
      SetNeedsCommit();
  }
}

void LayerTreeHostImpl::ActivateAnimations() {
  const bool activated =
      mutator_host_->ActivateAnimations(mutator_events_.get());
  if (activated) {
    // Activating an animation changes layer draw properties, such as
    // screen_space_transform_is_animating. So when we see a new animation get
    // activated, we need to update the draw properties on the active tree.
    active_tree()->set_needs_update_draw_properties();
    // Request another frame to run the next tick of the animation.
    SetNeedsOneBeginImplFrame();
    if (!mutator_events_->IsEmpty())
      SetNeedsCommit();
  }
}

void LayerTreeHostImpl::RegisterScrollbarAnimationController(
    ElementId scroll_element_id,
    float scrollbar_opacity) {
  if (ScrollbarAnimationControllerForElementId(scroll_element_id))
    return;

  scrollbar_animation_controllers_[scroll_element_id] =
      active_tree_->CreateScrollbarAnimationController(scroll_element_id,
                                                       scrollbar_opacity);
}

void LayerTreeHostImpl::DidUnregisterScrollbarLayer(
    ElementId scroll_element_id) {
  scrollbar_animation_controllers_.erase(scroll_element_id);
  scrollbar_controller_->DidUnregisterScrollbar(scroll_element_id);
}

ScrollbarAnimationController*
LayerTreeHostImpl::ScrollbarAnimationControllerForElementId(
    ElementId scroll_element_id) const {
  // The viewport layers have only one set of scrollbars. On Android, these are
  // registered with the inner viewport, otherwise they're registered with the
  // outer viewport. If a controller for one exists, the other shouldn't.
  if (InnerViewportScrollNode()) {
    DCHECK(OuterViewportScrollNode());
    if (scroll_element_id == InnerViewportScrollNode()->element_id ||
        scroll_element_id == OuterViewportScrollNode()->element_id) {
      auto itr = scrollbar_animation_controllers_.find(
          InnerViewportScrollNode()->element_id);
      if (itr != scrollbar_animation_controllers_.end())
        return itr->second.get();

      itr = scrollbar_animation_controllers_.find(
          OuterViewportScrollNode()->element_id);
      if (itr != scrollbar_animation_controllers_.end())
        return itr->second.get();

      return nullptr;
    }
  }

  auto i = scrollbar_animation_controllers_.find(scroll_element_id);
  if (i == scrollbar_animation_controllers_.end())
    return nullptr;
  return i->second.get();
}

void LayerTreeHostImpl::FlashAllScrollbars(bool did_scroll) {
  for (auto& pair : scrollbar_animation_controllers_) {
    if (did_scroll)
      pair.second->DidScrollUpdate();
    else
      pair.second->WillUpdateScroll();
  }
}

void LayerTreeHostImpl::PostDelayedScrollbarAnimationTask(
    base::OnceClosure task,
    base::TimeDelta delay) {
  client_->PostDelayedAnimationTaskOnImplThread(std::move(task), delay);
}

// TODO(danakj): Make this a return value from the Animate() call instead of an
// interface on LTHI. (Also, crbug.com/551138.)
void LayerTreeHostImpl::SetNeedsAnimateForScrollbarAnimation() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::SetNeedsAnimateForScrollbarAnimation");
  SetNeedsOneBeginImplFrame();
}

// TODO(danakj): Make this a return value from the Animate() call instead of an
// interface on LTHI. (Also, crbug.com/551138.)
void LayerTreeHostImpl::SetNeedsRedrawForScrollbarAnimation() {
  SetNeedsRedraw();
}

ScrollbarSet LayerTreeHostImpl::ScrollbarsFor(ElementId id) const {
  return active_tree_->ScrollbarsFor(id);
}

void LayerTreeHostImpl::AddVideoFrameController(
    VideoFrameController* controller) {
  bool was_empty = video_frame_controllers_.empty();
  video_frame_controllers_.insert(controller);
  if (current_begin_frame_tracker_.DangerousMethodHasStarted() &&
      !current_begin_frame_tracker_.DangerousMethodHasFinished())
    controller->OnBeginFrame(current_begin_frame_tracker_.Current());
  if (was_empty)
    client_->SetVideoNeedsBeginFrames(true);
}

void LayerTreeHostImpl::RemoveVideoFrameController(
    VideoFrameController* controller) {
  video_frame_controllers_.erase(controller);
  if (video_frame_controllers_.empty())
    client_->SetVideoNeedsBeginFrames(false);
}

void LayerTreeHostImpl::SetTreePriority(TreePriority priority) {
  if (global_tile_state_.tree_priority == priority)
    return;
  global_tile_state_.tree_priority = priority;
  DidModifyTilePriorities();
}

TreePriority LayerTreeHostImpl::GetTreePriority() const {
  return global_tile_state_.tree_priority;
}

const viz::BeginFrameArgs& LayerTreeHostImpl::CurrentBeginFrameArgs() const {
  // TODO(mithro): Replace call with current_begin_frame_tracker_.Current()
  // once all calls which happens outside impl frames are fixed.
  return current_begin_frame_tracker_.DangerousMethodCurrentOrLast();
}

base::TimeDelta LayerTreeHostImpl::CurrentBeginFrameInterval() const {
  return current_begin_frame_tracker_.Interval();
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
LayerTreeHostImpl::AsValueWithFrame(FrameData* frame) const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  AsValueWithFrameInto(frame, state.get());
  return std::move(state);
}

void LayerTreeHostImpl::AsValueWithFrameInto(
    FrameData* frame,
    base::trace_event::TracedValue* state) const {
  if (this->pending_tree_) {
    state->BeginDictionary("activation_state");
    ActivationStateAsValueInto(state);
    state->EndDictionary();
  }
  MathUtil::AddToTracedValue("device_viewport_size",
                             active_tree_->GetDeviceViewport().size(), state);

  std::vector<PrioritizedTile> prioritized_tiles;
  active_tree_->GetAllPrioritizedTilesForTracing(&prioritized_tiles);
  if (pending_tree_)
    pending_tree_->GetAllPrioritizedTilesForTracing(&prioritized_tiles);

  state->BeginArray("active_tiles");
  for (const auto& prioritized_tile : prioritized_tiles) {
    state->BeginDictionary();
    prioritized_tile.AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();

  state->BeginDictionary("tile_manager_basic_state");
  tile_manager_.BasicStateAsValueInto(state);
  state->EndDictionary();

  state->BeginDictionary("active_tree");
  active_tree_->AsValueInto(state);
  state->EndDictionary();
  if (pending_tree_) {
    state->BeginDictionary("pending_tree");
    pending_tree_->AsValueInto(state);
    state->EndDictionary();
  }
  if (frame) {
    state->BeginDictionary("frame");
    frame->AsValueInto(state);
    state->EndDictionary();
  }
}

void LayerTreeHostImpl::ActivationStateAsValueInto(
    base::trace_event::TracedValue* state) const {
  viz::TracedValue::SetIDRef(this, state, "lthi");
  state->BeginDictionary("tile_manager");
  tile_manager_.BasicStateAsValueInto(state);
  state->EndDictionary();
}

void LayerTreeHostImpl::SetDebugState(
    const LayerTreeDebugState& new_debug_state) {
  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  SetFullViewportDamage();
}

void LayerTreeHostImpl::CreateUIResource(UIResourceId uid,
                                         const UIResourceBitmap& bitmap) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::CreateUIResource");
  DCHECK_GT(uid, 0);

  // Allow for multiple creation requests with the same UIResourceId.  The
  // previous resource is simply deleted.
  viz::ResourceId id = ResourceIdForUIResource(uid);
  if (id)
    DeleteUIResource(uid);

  if (!has_valid_layer_tree_frame_sink_) {
    evicted_ui_resources_.insert(uid);
    return;
  }

  viz::ResourceFormat format;
  switch (bitmap.GetFormat()) {
    case UIResourceBitmap::RGBA8:
      if (layer_tree_frame_sink_->context_provider()) {
        const gpu::Capabilities& caps =
            layer_tree_frame_sink_->context_provider()->ContextCapabilities();
        format = viz::PlatformColor::BestSupportedTextureFormat(caps);
      } else {
        format = viz::RGBA_8888;
      }
      break;
    case UIResourceBitmap::ALPHA_8:
      format = viz::ALPHA_8;
      break;
    case UIResourceBitmap::ETC1:
      format = viz::ETC1;
      break;
  }

  const gfx::Size source_size = bitmap.GetSize();
  gfx::Size upload_size = bitmap.GetSize();
  bool scaled = false;
  // UIResources are assumed to be rastered in SRGB.
  const gfx::ColorSpace& color_space = gfx::ColorSpace::CreateSRGB();

  if (source_size.width() > max_texture_size_ ||
      source_size.height() > max_texture_size_) {
    // Must resize the bitmap to fit within the max texture size.
    scaled = true;
    int edge = std::max(source_size.width(), source_size.height());
    float scale = static_cast<float>(max_texture_size_ - 1) / edge;
    DCHECK_LT(scale, 1.f);
    upload_size = gfx::ScaleToCeiledSize(source_size, scale, scale);
  }

  // For gpu compositing, a SharedImage mailbox will be allocated and the
  // UIResource will be uploaded into it.
  gpu::Mailbox mailbox;
  uint32_t shared_image_usage = gpu::SHARED_IMAGE_USAGE_DISPLAY;
  // For gpu compositing, we also calculate the GL texture target.
  // TODO(ericrk): Remove references to GL from this code.
  GLenum texture_target = GL_TEXTURE_2D;
  // For software compositing, shared memory will be allocated and the
  // UIResource will be copied into it.
  base::MappedReadOnlyRegion shm;
  viz::SharedBitmapId shared_bitmap_id;
  bool overlay_candidate = false;

  if (layer_tree_frame_sink_->context_provider()) {
    viz::ContextProvider* context_provider =
        layer_tree_frame_sink_->context_provider();
    const auto& caps = context_provider->ContextCapabilities();
    overlay_candidate =
        settings_.resource_settings.use_gpu_memory_buffer_resources &&
        caps.texture_storage_image &&
        viz::IsGpuMemoryBufferFormatSupported(format);
    if (overlay_candidate) {
      shared_image_usage |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
      texture_target = gpu::GetBufferTextureTarget(gfx::BufferUsage::SCANOUT,
                                                   BufferFormat(format), caps);
    }
  } else {
    shm = viz::bitmap_allocation::AllocateSharedBitmap(upload_size, format);
    shared_bitmap_id = viz::SharedBitmap::GenerateId();
  }

  if (!scaled) {
    // If not scaled, we can copy the pixels 1:1 from the source bitmap to our
    // destination backing of a texture or shared bitmap.
    if (layer_tree_frame_sink_->context_provider()) {
      viz::ContextProvider* context_provider =
          layer_tree_frame_sink_->context_provider();
      auto* sii = context_provider->SharedImageInterface();
      mailbox = sii->CreateSharedImage(
          format, upload_size, color_space, shared_image_usage,
          base::span<const uint8_t>(bitmap.GetPixels(), bitmap.SizeInBytes()));
    } else {
      DCHECK_EQ(bitmap.GetFormat(), UIResourceBitmap::RGBA8);
      SkImageInfo src_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(source_size));
      SkImageInfo dst_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(upload_size));

      sk_sp<SkSurface> surface = SkSurface::MakeRasterDirect(
          dst_info, shm.mapping.memory(), dst_info.minRowBytes());
      surface->getCanvas()->writePixels(
          src_info, const_cast<uint8_t*>(bitmap.GetPixels()),
          bitmap.row_bytes(), 0, 0);
    }
  } else {
    // Only support auto-resizing for N32 textures (since this is primarily for
    // scrollbars). Users of other types need to ensure they are not too big.
    DCHECK_EQ(bitmap.GetFormat(), UIResourceBitmap::RGBA8);

    float canvas_scale_x =
        upload_size.width() / static_cast<float>(source_size.width());
    float canvas_scale_y =
        upload_size.height() / static_cast<float>(source_size.height());

    // Uses N32Premul since that is what SkBitmap's allocN32Pixels makes, and we
    // only support the RGBA8 format here.
    SkImageInfo info =
        SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(source_size));

    SkBitmap source_bitmap;
    source_bitmap.setInfo(info, bitmap.row_bytes());
    source_bitmap.setPixels(const_cast<uint8_t*>(bitmap.GetPixels()));

    // This applies the scale to draw the |bitmap| into |scaled_surface|. For
    // gpu compositing, we scale into a software bitmap-backed SkSurface here,
    // then upload from there into a texture. For software compositing, we scale
    // directly into the shared memory backing.
    sk_sp<SkSurface> scaled_surface;
    if (layer_tree_frame_sink_->context_provider()) {
      scaled_surface = SkSurface::MakeRasterN32Premul(upload_size.width(),
                                                      upload_size.height());
    } else {
      SkImageInfo dst_info =
          SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(upload_size));
      scaled_surface = SkSurface::MakeRasterDirect(
          dst_info, shm.mapping.memory(), dst_info.minRowBytes());
    }
    SkCanvas* scaled_canvas = scaled_surface->getCanvas();
    scaled_canvas->scale(canvas_scale_x, canvas_scale_y);
    // The |canvas_scale_x| and |canvas_scale_y| may have some floating point
    // error for large enough values, causing pixels on the edge to be not
    // fully filled by drawBitmap(), so we ensure they start empty. (See
    // crbug.com/642011 for an example.)
    scaled_canvas->clear(SK_ColorTRANSPARENT);
    scaled_canvas->drawBitmap(source_bitmap, 0, 0);

    if (layer_tree_frame_sink_->context_provider()) {
      SkPixmap pixmap;
      scaled_surface->peekPixels(&pixmap);
      viz::ContextProvider* context_provider =
          layer_tree_frame_sink_->context_provider();
      auto* sii = context_provider->SharedImageInterface();
      mailbox = sii->CreateSharedImage(
          format, upload_size, color_space, shared_image_usage,
          base::span<const uint8_t>(
              reinterpret_cast<const uint8_t*>(pixmap.addr()),
              pixmap.computeByteSize()));
    }
  }

  // Once the backing has the UIResource inside it, we have to prepare it for
  // export to the display compositor via ImportResource(). For gpu compositing,
  // this requires a Mailbox+SyncToken as well. For software compositing, the
  // SharedBitmapId must be notified to the LayerTreeFrameSink. The
  // OnUIResourceReleased() method will be called once the resource is deleted
  // and the display compositor is no longer using it, to free the memory
  // allocated in this method above.
  viz::TransferableResource transferable;
  if (layer_tree_frame_sink_->context_provider()) {
    gpu::SyncToken sync_token = layer_tree_frame_sink_->context_provider()
                                    ->SharedImageInterface()
                                    ->GenUnverifiedSyncToken();

    transferable = viz::TransferableResource::MakeGL(
        mailbox, GL_LINEAR, texture_target, sync_token, upload_size,
        overlay_candidate);
    transferable.format = format;
  } else {
    layer_tree_frame_sink_->DidAllocateSharedBitmap(std::move(shm.region),
                                                    shared_bitmap_id);
    transferable = viz::TransferableResource::MakeSoftware(shared_bitmap_id,
                                                           upload_size, format);
  }
  transferable.color_space = color_space;
  id = resource_provider_.ImportResource(
      transferable,
      // The OnUIResourceReleased method is bound with a WeakPtr, but the
      // resource backing will be deleted when the LayerTreeFrameSink is
      // removed before shutdown, so nothing leaks if the WeakPtr is
      // invalidated.
      viz::SingleReleaseCallback::Create(base::BindOnce(
          &LayerTreeHostImpl::OnUIResourceReleased, AsWeakPtr(), uid)));

  UIResourceData data;
  data.opaque = bitmap.GetOpaque();
  data.format = format;
  data.shared_bitmap_id = shared_bitmap_id;
  data.shared_mapping = std::move(shm.mapping);
  data.mailbox = mailbox;
  data.resource_id_for_export = id;
  ui_resource_map_[uid] = std::move(data);

  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "LayerTreeHostImpl::DeleteUIResource");
  auto it = ui_resource_map_.find(uid);
  if (it != ui_resource_map_.end()) {
    UIResourceData& data = it->second;
    viz::ResourceId id = data.resource_id_for_export;
    // Move the |data| to |deleted_ui_resources_| before removing it from the
    // viz::ClientResourceProvider, so that the ReleaseCallback can see it
    // there.
    deleted_ui_resources_[uid] = std::move(data);
    ui_resource_map_.erase(it);

    resource_provider_.RemoveImportedResource(id);
  }
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResourceBacking(
    UIResourceData data,
    const gpu::SyncToken& sync_token) {
  // Resources are either software or gpu backed, not both.
  DCHECK(!(data.shared_mapping.IsValid() && !data.mailbox.IsZero()));
  if (data.shared_mapping.IsValid())
    layer_tree_frame_sink_->DidDeleteSharedBitmap(data.shared_bitmap_id);
  if (!data.mailbox.IsZero()) {
    auto* sii =
        layer_tree_frame_sink_->context_provider()->SharedImageInterface();
    sii->DestroySharedImage(sync_token, data.mailbox);
  }
  // |data| goes out of scope and deletes anything it owned.
}

void LayerTreeHostImpl::OnUIResourceReleased(UIResourceId uid,
                                             const gpu::SyncToken& sync_token,
                                             bool lost) {
  auto it = deleted_ui_resources_.find(uid);
  if (it == deleted_ui_resources_.end()) {
    // Backing was already deleted, eg if the context was lost.
    return;
  }
  UIResourceData& data = it->second;
  // We don't recycle backings here, so |lost| is not relevant, we always delete
  // them.
  DeleteUIResourceBacking(std::move(data), sync_token);
  deleted_ui_resources_.erase(it);
}

void LayerTreeHostImpl::ClearUIResources() {
  for (auto& pair : ui_resource_map_) {
    UIResourceId uid = pair.first;
    UIResourceData& data = pair.second;
    resource_provider_.RemoveImportedResource(data.resource_id_for_export);
    // Immediately drop the backing instead of waiting for the resource to be
    // returned from the ResourceProvider, as this is called in cases where the
    // ability to clean up the backings will go away (context loss, shutdown).
    DeleteUIResourceBacking(std::move(data), gpu::SyncToken());
    // This resource is not deleted, and its |uid| is still valid, so it moves
    // to the evicted list, not the |deleted_ui_resources_| set. Also, its
    // backing is gone, so it would not belong in |deleted_ui_resources_|.
    evicted_ui_resources_.insert(uid);
  }
  ui_resource_map_.clear();
  for (auto& pair : deleted_ui_resources_) {
    UIResourceData& data = pair.second;
    // Immediately drop the backing instead of waiting for the resource to be
    // returned from the ResourceProvider, as this is called in cases where the
    // ability to clean up the backings will go away (context loss, shutdown).
    DeleteUIResourceBacking(std::move(data), gpu::SyncToken());
  }
  deleted_ui_resources_.clear();
}

void LayerTreeHostImpl::EvictAllUIResources() {
  if (ui_resource_map_.empty())
    return;
  while (!ui_resource_map_.empty()) {
    UIResourceId uid = ui_resource_map_.begin()->first;
    DeleteUIResource(uid);
    evicted_ui_resources_.insert(uid);
  }
  client_->SetNeedsCommitOnImplThread();
  client_->OnCanDrawStateChanged(CanDraw());
  client_->RenewTreePriority();
}

viz::ResourceId LayerTreeHostImpl::ResourceIdForUIResource(
    UIResourceId uid) const {
  auto iter = ui_resource_map_.find(uid);
  if (iter != ui_resource_map_.end())
    return iter->second.resource_id_for_export;
  return viz::kInvalidResourceId;
}

bool LayerTreeHostImpl::IsUIResourceOpaque(UIResourceId uid) const {
  auto iter = ui_resource_map_.find(uid);
  DCHECK(iter != ui_resource_map_.end());
  return iter->second.opaque;
}

bool LayerTreeHostImpl::EvictedUIResourcesExist() const {
  return !evicted_ui_resources_.empty();
}

void LayerTreeHostImpl::MarkUIResourceNotEvicted(UIResourceId uid) {
  auto found_in_evicted = evicted_ui_resources_.find(uid);
  if (found_in_evicted == evicted_ui_resources_.end())
    return;
  evicted_ui_resources_.erase(found_in_evicted);
  if (evicted_ui_resources_.empty())
    client_->OnCanDrawStateChanged(CanDraw());
}

void LayerTreeHostImpl::ScheduleMicroBenchmark(
    std::unique_ptr<MicroBenchmarkImpl> benchmark) {
  micro_benchmark_controller_.ScheduleRun(std::move(benchmark));
}

void LayerTreeHostImpl::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHostImpl::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfSetNeedsRedraw() {
  auto it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsRedrawOnImpl();
}

void LayerTreeHostImpl::UpdateRootLayerStateForSynchronousInputHandler() {
  if (!input_handler_client_)
    return;
  input_handler_client_->UpdateRootLayerStateForSynchronousInputHandler(
      active_tree_->TotalScrollOffset(), active_tree_->TotalMaxScrollOffset(),
      active_tree_->ScrollableSize(), active_tree_->current_page_scale_factor(),
      active_tree_->min_page_scale_factor(),
      active_tree_->max_page_scale_factor());
}

bool LayerTreeHostImpl::ScrollAnimationUpdateTarget(
    const ScrollNode& scroll_node,
    const gfx::Vector2dF& scroll_delta,
    base::TimeDelta delayed_by) {
  // TODO(bokan): Remove |scroll_node| as a parameter and just use the value
  // coming from |mutator_host|.
  DCHECK_EQ(scroll_node.element_id,
            mutator_host_->ImplOnlyScrollAnimatingElement());

  float scale_factor = active_tree()->page_scale_factor_for_scroll();
  gfx::Vector2dF adjusted_delta =
      gfx::ScaleVector2d(scroll_delta, 1.f / scale_factor);
  adjusted_delta = UserScrollableDelta(scroll_node, adjusted_delta);

  bool animation_updated = mutator_host_->ImplOnlyScrollAnimationUpdateTarget(
      adjusted_delta,
      active_tree_->property_trees()->scroll_tree.MaxScrollOffset(
          scroll_node.id),
      CurrentBeginFrameArgs().frame_time, delayed_by);
  if (animation_updated) {
    // Because we updated the animation target, notify the SwapPromiseMonitor
    // to tell it that something happened that will cause a swap in the future.
    // This will happen within the scope of the dispatch of a gesture scroll
    // update input event. If we don't notify during the handling of the input
    // event, the LatencyInfo associated with the input event will not be
    // added as a swap promise and we won't get any swap results.
    NotifySwapPromiseMonitorsOfSetNeedsRedraw();
    events_metrics_manager_.SaveActiveEventMetrics();

    // The animation is no longer targeting a snap position. By clearing the
    // target, this will ensure that we attempt to resnap at the end of this
    // animation.
    scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();
  }

  return animation_updated;
}

bool LayerTreeHostImpl::IsElementInPropertyTrees(
    ElementId element_id,
    ElementListType list_type) const {
  if (list_type == ElementListType::ACTIVE)
    return active_tree() && active_tree()->IsElementInPropertyTree(element_id);

  return (pending_tree() &&
          pending_tree()->IsElementInPropertyTree(element_id)) ||
         (recycle_tree() &&
          recycle_tree()->IsElementInPropertyTree(element_id));
}

void LayerTreeHostImpl::SetMutatorsNeedCommit() {}

void LayerTreeHostImpl::SetMutatorsNeedRebuildPropertyTrees() {}

void LayerTreeHostImpl::SetTreeLayerScrollOffsetMutated(
    ElementId element_id,
    LayerTreeImpl* tree,
    const gfx::ScrollOffset& scroll_offset) {
  if (!tree)
    return;

  PropertyTrees* property_trees = tree->property_trees();
  DCHECK_EQ(1u,
            property_trees->element_id_to_scroll_node_index.count(element_id));
  const int scroll_node_index =
      property_trees->element_id_to_scroll_node_index[element_id];
  property_trees->scroll_tree.OnScrollOffsetAnimated(
      element_id, scroll_node_index, scroll_offset, tree);
}

void LayerTreeHostImpl::SetNeedUpdateGpuRasterizationStatus() {
  need_update_gpu_rasterization_status_ = true;
}

void LayerTreeHostImpl::SetElementFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& filters) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetFilterMutated(element_id, filters);
  } else {
    if (pending_tree())
      pending_tree()->SetFilterMutated(element_id, filters);
    if (recycle_tree())
      recycle_tree()->SetFilterMutated(element_id, filters);
  }
}

void LayerTreeHostImpl::OnCustomPropertyMutated(
    ElementId element_id,
    const std::string& custom_property_name,
    PaintWorkletInput::PropertyValue custom_property_value) {
  paint_worklet_tracker_.OnCustomPropertyMutated(
      element_id, custom_property_name, std::move(custom_property_value));
}

void LayerTreeHostImpl::SetElementBackdropFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& backdrop_filters) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetBackdropFilterMutated(element_id, backdrop_filters);
  } else {
    if (pending_tree())
      pending_tree()->SetBackdropFilterMutated(element_id, backdrop_filters);
    if (recycle_tree())
      recycle_tree()->SetBackdropFilterMutated(element_id, backdrop_filters);
  }
}

void LayerTreeHostImpl::SetElementOpacityMutated(ElementId element_id,
                                                 ElementListType list_type,
                                                 float opacity) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetOpacityMutated(element_id, opacity);
  } else {
    if (pending_tree())
      pending_tree()->SetOpacityMutated(element_id, opacity);
    if (recycle_tree())
      recycle_tree()->SetOpacityMutated(element_id, opacity);
  }
}

void LayerTreeHostImpl::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  if (list_type == ElementListType::ACTIVE) {
    active_tree()->SetTransformMutated(element_id, transform);
  } else {
    if (pending_tree())
      pending_tree()->SetTransformMutated(element_id, transform);
    if (recycle_tree())
      recycle_tree()->SetTransformMutated(element_id, transform);
  }
}

void LayerTreeHostImpl::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerScrollOffsetMutated(element_id, active_tree(), scroll_offset);
    ShowScrollbarsForImplScroll(element_id);
  } else {
    SetTreeLayerScrollOffsetMutated(element_id, pending_tree(), scroll_offset);
    SetTreeLayerScrollOffsetMutated(element_id, recycle_tree(), scroll_offset);
  }
}

void LayerTreeHostImpl::ElementIsAnimatingChanged(
    const PropertyToElementIdMap& element_id_map,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state) {
  LayerTreeImpl* tree =
      list_type == ElementListType::ACTIVE ? active_tree() : pending_tree();
  // TODO(wkorman): Explore enabling DCHECK in ElementIsAnimatingChanged()
  // below. Currently enabling causes batch of unit test failures.
  if (tree && tree->property_trees()->ElementIsAnimatingChanged(
                  element_id_map, mask, state, false))
    tree->set_needs_update_draw_properties();
}

void LayerTreeHostImpl::AnimationScalesChanged(ElementId element_id,
                                               ElementListType list_type,
                                               float maximum_scale,
                                               float starting_scale) {
  if (LayerTreeImpl* tree = list_type == ElementListType::ACTIVE
                                ? active_tree()
                                : pending_tree()) {
    tree->property_trees()->AnimationScalesChanged(element_id, maximum_scale,
                                                   starting_scale);
  }
}

void LayerTreeHostImpl::ScrollOffsetAnimationFinished() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollOffsetAnimationFinished");
  // ScrollOffsetAnimationFinished is called in two cases:
  //  1- smooth scrolling animation is over (IsAnimatingForSnap == false).
  //  2- snap scroll animation is over (IsAnimatingForSnap == true).
  //
  //  Only for case (1) we should check and run snap scroll animation if needed.
  if (!IsAnimatingForSnap() && SnapAtScrollEnd())
    return;

  // The end of a scroll offset animation means that the scrolling node is at
  // the target offset.
  ScrollNode* scroll_node = CurrentlyScrollingNode();
  if (scroll_node && scroll_node->snap_container_data.has_value()) {
    scroll_node->snap_container_data.value().SetTargetSnapAreaElementIds(
        scroll_animating_snap_target_ids_);
    updated_snapped_elements_.insert(scroll_node->element_id);
    client_->SetNeedsCommitOnImplThread();
  }
  scroll_animating_snap_target_ids_ = TargetSnapAreaElementIds();

  // Call scrollEnd with the deferred scroll end state when the scroll animation
  // completes after GSE arrival.
  if (deferred_scroll_end_) {
    ScrollEnd(/*should_snap=*/false);
    return;
  }
}

void LayerTreeHostImpl::NotifyAnimationWorkletStateChange(
    AnimationWorkletMutationState state,
    ElementListType tree_type) {
  client_->NotifyAnimationWorkletStateChange(state, tree_type);
  if (state != AnimationWorkletMutationState::CANCELED) {
    // We have at least one active worklet animation. We need to request a new
    // frame to keep the animation ticking.
    SetNeedsOneBeginImplFrame();
    if (state == AnimationWorkletMutationState::COMPLETED_WITH_UPDATE &&
        tree_type == ElementListType::ACTIVE) {
      SetNeedsRedraw();
    }
  }
}

gfx::ScrollOffset LayerTreeHostImpl::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  if (active_tree()) {
    return active_tree()->property_trees()->scroll_tree.current_scroll_offset(
        element_id);
  }

  return gfx::ScrollOffset();
}

bool LayerTreeHostImpl::SupportsImplScrolling() const {
  // Supported in threaded mode.
  return task_runner_provider_->HasImplThread();
}

bool LayerTreeHostImpl::CommitToActiveTree() const {
  return settings_.commit_to_active_tree;
}

void LayerTreeHostImpl::SetContextVisibility(bool is_visible) {
  if (!layer_tree_frame_sink_)
    return;

  // Update the compositor context. If we are already in the correct visibility
  // state, skip. This can happen if we transition invisible/visible rapidly,
  // before we get a chance to go invisible in NotifyAllTileTasksComplete.
  auto* compositor_context = layer_tree_frame_sink_->context_provider();
  if (compositor_context && is_visible != !!compositor_context_visibility_) {
    if (is_visible) {
      compositor_context_visibility_ =
          compositor_context->CacheController()->ClientBecameVisible();
    } else {
      compositor_context->CacheController()->ClientBecameNotVisible(
          std::move(compositor_context_visibility_));
    }
  }

  // Update the worker context. If we are already in the correct visibility
  // state, skip. This can happen if we transition invisible/visible rapidly,
  // before we get a chance to go invisible in NotifyAllTileTasksComplete.
  auto* worker_context = layer_tree_frame_sink_->worker_context_provider();
  if (worker_context && is_visible != !!worker_context_visibility_) {
    viz::RasterContextProvider::ScopedRasterContextLock hold(worker_context);
    if (is_visible) {
      worker_context_visibility_ =
          worker_context->CacheController()->ClientBecameVisible();
    } else {
      worker_context->CacheController()->ClientBecameNotVisible(
          std::move(worker_context_visibility_));
    }
  }
}

void LayerTreeHostImpl::UpdateScrollSourceInfo(const ScrollState& scroll_state,
                                               ui::ScrollInputType type) {
  if (type == ui::ScrollInputType::kWheel &&
      scroll_state.delta_granularity() ==
          ui::ScrollGranularity::kScrollByPrecisePixel) {
    has_scrolled_by_precisiontouchpad_ = true;
  } else if (type == ui::ScrollInputType::kWheel) {
    has_scrolled_by_wheel_ = true;
  } else if (type == ui::ScrollInputType::kTouchscreen) {
    has_scrolled_by_touch_ = true;
  }
}

void LayerTreeHostImpl::ShowScrollbarsForImplScroll(ElementId element_id) {
  if (settings_.scrollbar_flash_after_any_scroll_update) {
    FlashAllScrollbars(true);
    return;
  }
  if (!element_id)
    return;
  if (ScrollbarAnimationController* animation_controller =
          ScrollbarAnimationControllerForElementId(element_id))
    animation_controller->DidScrollUpdate();
}

void LayerTreeHostImpl::InitializeUkm(
    std::unique_ptr<ukm::UkmRecorder> recorder) {
  DCHECK(!ukm_manager_);
  ukm_manager_ = std::make_unique<UkmManager>(std::move(recorder));
  frame_trackers_.SetUkmManager(ukm_manager_.get());
  compositor_frame_reporting_controller_->SetUkmManager(ukm_manager_.get());
}

void LayerTreeHostImpl::SetActiveURL(const GURL& url, ukm::SourceId source_id) {
  tile_manager_.set_active_url(url);

  // The active tree might still be from content for the previous page when the
  // recorder is updated here, since new content will be pushed with the next
  // main frame. But we should only get a few impl frames wrong here in that
  // case. Also, since checkerboard stats are only recorded with user
  // interaction, it must be in progress when the navigation commits for this
  // case to occur.
  if (ukm_manager_) {
    // The source id has already been associated to the URL.
    ukm_manager_->SetSourceId(source_id);
  }
}

void LayerTreeHostImpl::AllocateLocalSurfaceId() {
  child_local_surface_id_allocator_.GenerateId();
}

void LayerTreeHostImpl::RequestBeginFrameForAnimatedImages() {
  SetNeedsOneBeginImplFrame();
}

void LayerTreeHostImpl::RequestInvalidationForAnimatedImages() {
  DCHECK_EQ(impl_thread_phase_, ImplThreadPhase::INSIDE_IMPL_FRAME);

  // If we are animating an image, we want at least one draw of the active tree
  // before a new tree is activated.
  bool needs_first_draw_on_activation = true;
  client_->NeedsImplSideInvalidation(needs_first_draw_on_activation);
}

EventMetricsSet LayerTreeHostImpl::TakeEventsMetrics() {
  return EventMetricsSet(active_tree()->TakeEventsMetrics(),
                         events_metrics_manager_.TakeSavedEventsMetrics());
}

base::WeakPtr<LayerTreeHostImpl> LayerTreeHostImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace cc
