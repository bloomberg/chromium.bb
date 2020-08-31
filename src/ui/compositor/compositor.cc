// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_settings.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/compositor/overscroll/scroll_input_handler.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#endif

namespace ui {

// Used to hold on to IssueExternalBeginFrame arguments if
// |external_begin_frame_controller_| isn't ready yet.
struct PendingBeginFrameArgs {
  PendingBeginFrameArgs(
      const viz::BeginFrameArgs& args,
      bool force,
      base::OnceCallback<void(const viz::BeginFrameAck&)> callback)
      : args(args), force(force), callback(std::move(callback)) {}

  viz::BeginFrameArgs args;
  bool force;
  base::OnceCallback<void(const viz::BeginFrameAck&)> callback;
};

Compositor::Compositor(const viz::FrameSinkId& frame_sink_id,
                       ui::ContextFactory* context_factory,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       bool enable_pixel_canvas,
                       bool use_external_begin_frame_control,
                       bool force_software_compositor)
    : context_factory_(context_factory),
      frame_sink_id_(frame_sink_id),
      task_runner_(task_runner),
      use_external_begin_frame_control_(use_external_begin_frame_control),
      force_software_compositor_(force_software_compositor),
      layer_animator_collection_(this),
      is_pixel_canvas_(enable_pixel_canvas),
      lock_manager_(task_runner) {
  DCHECK(context_factory_);
  auto* host_frame_sink_manager = context_factory_->GetHostFrameSinkManager();
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  host_frame_sink_manager->SetFrameSinkDebugLabel(frame_sink_id_, "Compositor");
  root_web_layer_ = cc::Layer::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // This will ensure PictureLayers always can have LCD text, to match the
  // previous behaviour with ContentLayers, where LCD-not-allowed notifications
  // were ignored.
  settings.layers_always_allowed_lcd_text = true;
  // Use occlusion to allow more overlapping windows to take less memory.
  settings.use_occlusion_for_tile_prioritization = true;
  settings.main_frame_before_activation_enabled = false;

  // Disable edge anti-aliasing in order to increase support for HW overlays.
  settings.enable_edge_anti_aliasing = false;

  // GPU rasterization in the UI compositor is controlled by a feature.
  settings.gpu_rasterization_disabled =
      !features::IsUiGpuRasterizationEnabled();

  if (command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders)) {
    std::string layer_borders_string = command_line->GetSwitchValueASCII(
        cc::switches::kUIShowCompositedLayerBorders);
    std::vector<base::StringPiece> entries = base::SplitStringPiece(
        layer_borders_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (entries.empty()) {
      settings.initial_debug_state.show_debug_borders.set();
    } else {
      for (const auto& entry : entries) {
        const struct {
          const char* name;
          cc::DebugBorderType type;
        } kBorders[] = {{cc::switches::kCompositedRenderPassBorders,
                         cc::DebugBorderType::RENDERPASS},
                        {cc::switches::kCompositedSurfaceBorders,
                         cc::DebugBorderType::SURFACE},
                        {cc::switches::kCompositedLayerBorders,
                         cc::DebugBorderType::LAYER}};
        for (const auto& border : kBorders) {
          if (border.name == entry) {
            settings.initial_debug_state.show_debug_borders.set(border.type);
            break;
          }
        }
      }
    }
  }
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      command_line->HasSwitch(cc::switches::kUIShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      command_line->HasSwitch(switches::kUIShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      command_line->HasSwitch(cc::switches::kUIShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      command_line->HasSwitch(cc::switches::kUIShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowScreenSpaceRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  settings.use_zero_copy = IsUIZeroCopyEnabled();

  settings.use_layer_lists =
      command_line->HasSwitch(cc::switches::kUIEnableLayerLists);

  // UI compositor always uses partial raster if not using zero-copy. Zero copy
  // doesn't currently support partial raster.
  settings.use_partial_raster = !settings.use_zero_copy;

  settings.use_rgba_4444 =
      command_line->HasSwitch(switches::kUIEnableRGBA4444Textures);

#if defined(OS_MACOSX)
  // Using CoreAnimation to composite requires using GpuMemoryBuffers, which
  // require zero copy.
  settings.resource_settings.use_gpu_memory_buffer_resources =
      settings.use_zero_copy;
  settings.enable_elastic_overscroll = true;
#endif

  settings.memory_policy.bytes_limit_when_visible = 512 * 1024 * 1024;

  // Used to configure ui compositor memory limit for chromeos devices.
  // See crbug.com/923141.
  if (command_line->HasSwitch(
          switches::kUiCompositorMemoryLimitWhenVisibleMB)) {
    std::string value_str = command_line->GetSwitchValueASCII(
        switches::kUiCompositorMemoryLimitWhenVisibleMB);
    unsigned value_in_mb;
    if (base::StringToUint(value_str, &value_in_mb)) {
      settings.memory_policy.bytes_limit_when_visible =
          1024 * 1024 * value_in_mb;
    }
  }

  settings.memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  settings.disallow_non_exact_resource_reuse =
      command_line->HasSwitch(switches::kDisallowNonExactResourceReuse);

  settings.enable_impl_latency_recovery =
      features::IsImplLatencyRecoveryEnabled();
  settings.enable_main_latency_recovery =
      features::IsMainLatencyRecoveryEnabled();

  if (command_line->HasSwitch(switches::kRunAllCompositorStagesBeforeDraw)) {
    settings.wait_for_all_pipeline_stages_before_draw = true;
    settings.enable_impl_latency_recovery = false;
    settings.enable_main_latency_recovery = false;
  }

  if (base::FeatureList::IsEnabled(
          features::kCompositorThreadedScrollbarScrolling)) {
    settings.compositor_threaded_scrollbar_scrolling = true;
  }

  if (base::FeatureList::IsEnabled(features::kPercentBasedScrolling)) {
    settings.percent_based_scrolling = true;
  }

#if DCHECK_IS_ON()
  if (command_line->HasSwitch(cc::switches::kLogOnUIDoubleBackgroundBlur))
    settings.log_on_ui_double_background_blur = true;
#endif

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));

  if (base::FeatureList::IsEnabled(features::kUiCompositorScrollWithLayers) &&
      host_->GetInputHandler()) {
    scroll_input_handler_.reset(
        new ScrollInputHandler(host_->GetInputHandler()));
  }

  animation_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  animation_host_->AddAnimationTimeline(animation_timeline_.get());

  host_->SetRootLayer(root_web_layer_);

  // This shouldn't be done in the constructor in order to match Widget.
  // See: http://crbug.com/956264.
  host_->SetVisible(true);

  if (command_line->HasSwitch(switches::kUISlowAnimations)) {
    slow_animations_ = std::make_unique<ScopedAnimationDurationScaleMode>(
        ScopedAnimationDurationScaleMode::SLOW_DURATION);
  }
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown,viz", "Compositor::destructor");

  for (auto& observer : observer_list_)
    observer.OnCompositingShuttingDown(this);

  for (auto& observer : animation_observer_list_)
    observer.OnCompositingShuttingDown(this);

  if (root_layer_)
    root_layer_->ResetCompositor();

  if (animation_timeline_)
    animation_host_->RemoveAnimationTimeline(animation_timeline_.get());

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
  auto* host_frame_sink_manager = context_factory_->GetHostFrameSinkManager();
  for (auto& client : child_frame_sinks_) {
    DCHECK(client.is_valid());
    host_frame_sink_manager->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                          client);
  }
  host_frame_sink_manager->InvalidateFrameSinkId(frame_sink_id_);
}

void Compositor::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  context_factory_->GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
      frame_sink_id_, frame_sink_id);

  child_frame_sinks_.insert(frame_sink_id);
}

void Compositor::RemoveChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  auto it = child_frame_sinks_.find(frame_sink_id);
  DCHECK(it != child_frame_sinks_.end());
  DCHECK(it->is_valid());
  context_factory_->GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
      frame_sink_id_, *it);
  child_frame_sinks_.erase(it);
}

void Compositor::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink,
    viz::mojom::DisplayPrivate* display_private) {
  layer_tree_frame_sink_requested_ = false;
  display_private_ = display_private;
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
  // Display properties are reset when the output surface is lost, so update it
  // to match the Compositor's.
  if (display_private_) {
    disabled_swap_until_resize_ = false;
    display_private_->Resize(size());
    display_private_->SetDisplayVisible(host_->IsVisible());
    display_private_->SetDisplayColorSpaces(display_color_spaces_);
    display_private_->SetDisplayColorMatrix(
        gfx::Transform(display_color_matrix_));
    display_private_->SetOutputIsSecure(output_is_secure_);
    if (has_vsync_params_)
      display_private_->SetDisplayVSyncParameters(vsync_timebase_,
                                                  vsync_interval_);
  }
}

void Compositor::SetExternalBeginFrameController(
    viz::mojom::ExternalBeginFrameController* external_begin_frame_controller) {
  DCHECK(use_external_begin_frame_control());
  external_begin_frame_controller_ = external_begin_frame_controller;
  if (pending_begin_frame_args_) {
    external_begin_frame_controller_->IssueExternalBeginFrame(
        pending_begin_frame_args_->args, pending_begin_frame_args_->force,
        std::move(pending_begin_frame_args_->callback));
    pending_begin_frame_args_.reset();
  }
}

void Compositor::OnChildResizing() {
  for (auto& observer : observer_list_)
    observer.OnCompositingChildResizing(this);
}

void Compositor::ScheduleDraw() {
  host_->SetNeedsCommit();
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->ResetCompositor();
  root_layer_ = root_layer;
  root_web_layer_->RemoveAllChildren();
  if (root_layer_)
    root_layer_->SetCompositor(this, root_web_layer_);
}

cc::AnimationTimeline* Compositor::GetAnimationTimeline() const {
  return animation_timeline_.get();
}

void Compositor::SetDisplayColorMatrix(const SkMatrix44& matrix) {
  display_color_matrix_ = matrix;
  if (display_private_)
    display_private_->SetDisplayColorMatrix(gfx::Transform(matrix));
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedrawRect(host_->device_viewport_rect());
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

#if defined(OS_WIN)
void Compositor::SetShouldDisableSwapUntilResize(bool should) {
  should_disable_swap_until_resize_ = should;
}

void Compositor::DisableSwapUntilResize() {
  if (should_disable_swap_until_resize_ && display_private_) {
    // Browser needs to block for Viz to receive and process this message.
    // Otherwise when we return from WM_WINDOWPOSCHANGING message handler and
    // receive a WM_WINDOWPOSCHANGED the resize is finalized and any swaps of
    // wrong size by Viz can cause the swapped content to get scaled.
    // TODO(crbug.com/859168): Investigate nonblocking ways for solving.
    TRACE_EVENT0("viz", "Blocked UI for DisableSwapUntilResize");
    mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow_sync_call;
    display_private_->DisableSwapUntilResize();
    disabled_swap_until_resize_ = true;
  }
}

void Compositor::ReenableSwap() {
  if (should_disable_swap_until_resize_ && display_private_)
    display_private_->Resize(size_);
}
#endif

void Compositor::SetScaleAndSize(
    float scale,
    const gfx::Size& size_in_pixel,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  DCHECK_GT(scale, 0);
  bool device_scale_factor_changed = device_scale_factor_ != scale;
  device_scale_factor_ = scale;

#if DCHECK_IS_ON()
  if (size_ != size_in_pixel && local_surface_id_allocation.IsValid()) {
    // A new LocalSurfaceId must be set when the compositor size changes.
    DCHECK_NE(
        local_surface_id_allocation.local_surface_id(),
        host_->local_surface_id_allocation_from_parent().local_surface_id());
    DCHECK_NE(local_surface_id_allocation,
              host_->local_surface_id_allocation_from_parent());
  }
#endif  // DECHECK_IS_ON()

  if (!size_in_pixel.IsEmpty()) {
    bool size_changed = size_ != size_in_pixel;
    size_ = size_in_pixel;
    host_->SetViewportRectAndScale(gfx::Rect(size_in_pixel), scale,
                                   local_surface_id_allocation);
    root_web_layer_->SetBounds(size_in_pixel);
    if (display_private_ && (size_changed || disabled_swap_until_resize_)) {
      display_private_->Resize(size_in_pixel);
      disabled_swap_until_resize_ = false;
    }
  }
  if (device_scale_factor_changed) {
    if (is_pixel_canvas())
      host_->SetRecordingScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  if (display_color_spaces_ == display_color_spaces)
    return;
  display_color_spaces_ = display_color_spaces;
  // TODO(crbug.com/1012846): Remove this flag and provision when HDR is fully
  // supported on ChromeOS.
#if defined(OS_CHROMEOS)
  if (display_color_spaces_.SupportsHDR() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUseHDRTransferFunction)) {
    display_color_spaces_ =
        gfx::DisplayColorSpaces(gfx::ColorSpace::CreateSRGB());
  }
#endif

  host_->SetRasterColorSpace(display_color_spaces_.GetRasterColorSpace());
  // Always force the ui::Compositor to re-draw all layers, because damage
  // tracking bugs result in black flashes.
  // https://crbug.com/804430
  // TODO(ccameron): Remove this when the above bug is fixed.
  host_->SetNeedsDisplayOnAllLayers();

  // Color space is reset when the output surface is lost, so this must also be
  // updated then.
  if (display_private_)
    display_private_->SetDisplayColorSpaces(display_color_spaces_);
}

void Compositor::SetDisplayTransformHint(gfx::OverlayTransform hint) {
  host_->set_display_transform_hint(hint);
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
  // Visibility is reset when the output surface is lost, so this must also be
  // updated then.
  if (display_private_)
    display_private_->SetDisplayVisible(visible);
}

bool Compositor::IsVisible() {
  return host_->IsVisible();
}

bool Compositor::ScrollLayerTo(cc::ElementId element_id,
                               const gfx::ScrollOffset& offset) {
  auto input_handler = host_->GetInputHandler();
  return input_handler && input_handler->ScrollLayerTo(element_id, offset);
}

bool Compositor::GetScrollOffsetForLayer(cc::ElementId element_id,
                                         gfx::ScrollOffset* offset) const {
  auto input_handler = host_->GetInputHandler();
  return input_handler &&
         input_handler->GetScrollOffsetForLayer(element_id, offset);
}

void Compositor::SetDisplayVSyncParameters(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
  static bool is_frame_rate_limit_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFrameRateLimit);
  if (is_frame_rate_limit_disabled)
    return;

  if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = viz::BeginFrameArgs::DefaultInterval();
  }
  DCHECK_GT(interval.InMillisecondsF(), 0);

  // This is called at high frequenty on macOS, so early-out of redundant
  // updates here.
  if (vsync_timebase_ == timebase && vsync_interval_ == interval)
    return;

  if (interval != vsync_interval_)
    has_vsync_params_ = true;

  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
  if (display_private_)
    display_private_->SetDisplayVSyncParameters(timebase, interval);
}

void Compositor::AddVSyncParameterObserver(
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  if (display_private_)
    display_private_->AddVSyncParameterObserver(std::move(observer));
}

void Compositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  // This function should only get called once.
  DCHECK(!widget_valid_);
  widget_ = widget;
  widget_valid_ = true;
  if (layer_tree_frame_sink_requested_) {
    context_factory_->CreateLayerTreeFrameSink(
        context_creation_weak_ptr_factory_.GetWeakPtr());
  }
}

gfx::AcceleratedWidget Compositor::ReleaseAcceleratedWidget() {
  DCHECK(!IsVisible());
  host_->ReleaseLayerTreeFrameSink();
  display_private_ = nullptr;
  external_begin_frame_controller_ = nullptr;
  context_factory_->RemoveCompositor(this);
  context_creation_weak_ptr_factory_.InvalidateWeakPtrs();
  widget_valid_ = false;
  gfx::AcceleratedWidget widget = widget_;
  widget_ = gfx::kNullAcceleratedWidget;
  return widget;
}

gfx::AcceleratedWidget Compositor::widget() const {
  DCHECK(widget_valid_);
  return widget_;
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool Compositor::HasObserver(const CompositorObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Compositor::AddAnimationObserver(CompositorAnimationObserver* observer) {
  animation_observer_list_.AddObserver(observer);
  host_->SetNeedsAnimate();
}

void Compositor::RemoveAnimationObserver(
    CompositorAnimationObserver* observer) {
  animation_observer_list_.RemoveObserver(observer);
}

bool Compositor::HasAnimationObserver(
    const CompositorAnimationObserver* observer) const {
  return animation_observer_list_.HasObserver(observer);
}

void Compositor::IssueExternalBeginFrame(
    const viz::BeginFrameArgs& args,
    bool force,
    base::OnceCallback<void(const viz::BeginFrameAck&)> callback) {
  if (!external_begin_frame_controller_) {
    // IssueExternalBeginFrame() shouldn't be called again before the previous
    // begin frame is acknowledged.
    DCHECK(!pending_begin_frame_args_);
    pending_begin_frame_args_ = std::make_unique<PendingBeginFrameArgs>(
        args, force, std::move(callback));
    return;
  }
  external_begin_frame_controller_->IssueExternalBeginFrame(
      args, force, std::move(callback));
}

ThroughputTracker Compositor::RequestNewThroughputTracker() {
  return ThroughputTracker(next_throughput_tracker_id_++, this);
}

void Compositor::DidUpdateLayers() {
  // Dump property trees and layers if run with:
  //   --vmodule=*ui/compositor*=3
  VLOG(3) << "After updating layers:\n"
          << "property trees:\n"
          << host_->property_trees()->ToString() << "\n"
          << "cc::Layers:\n"
          << host_->LayersAsString();
}

void Compositor::BeginMainFrame(const viz::BeginFrameArgs& args) {
  DCHECK(!IsLocked());
  for (auto& observer : animation_observer_list_)
    observer.OnAnimationStep(args.frame_time);
  if (animation_observer_list_.might_have_observers())
    host_->SetNeedsAnimate();
}

void Compositor::BeginMainFrameNotExpectedSoon() {
}

void Compositor::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {}

static void SendDamagedRectsRecursive(ui::Layer* layer) {
  layer->SendDamagedRects();
  for (auto* child : layer->children())
    SendDamagedRectsRecursive(child);
}

void Compositor::UpdateLayerTreeHost() {
  if (!root_layer())
    return;
  SendDamagedRectsRecursive(root_layer());
}

void Compositor::RequestNewLayerTreeFrameSink() {
  DCHECK(!layer_tree_frame_sink_requested_);
  layer_tree_frame_sink_requested_ = true;
  if (widget_valid_) {
    context_factory_->CreateLayerTreeFrameSink(
        context_creation_weak_ptr_factory_.GetWeakPtr());
  }
}

void Compositor::DidFailToInitializeLayerTreeFrameSink() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Compositor::RequestNewLayerTreeFrameSink,
                     context_creation_weak_ptr_factory_.GetWeakPtr()));
}

void Compositor::DidCommit(base::TimeTicks) {
  DCHECK(!IsLocked());
  for (auto& observer : observer_list_)
    observer.OnCompositingDidCommit(this);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
Compositor::GetBeginMainFrameMetrics() {
  return nullptr;
}

void Compositor::NotifyThroughputTrackerResults(
    cc::CustomTrackerResults results) {
  for (auto& pair : results)
    ReportThroughputForTracker(pair.first, std::move(pair.second));
}

void Compositor::DidReceiveCompositorFrameAck() {
  ++activated_frame_count_;
  for (auto& observer : observer_list_)
    observer.OnCompositingEnded(this);
}

void Compositor::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("cc,benchmark", "FramePresented",
                                   feedback.timestamp, "environment",
                                   "browser");
}

void Compositor::DidSubmitCompositorFrame() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (auto& observer : observer_list_)
    observer.OnCompositingStarted(this, start_time);
}

void Compositor::FrameIntervalUpdated(base::TimeDelta interval) {
  refresh_rate_ =
      base::Time::kMicrosecondsPerSecond / interval.InMicrosecondsF();
}

void Compositor::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
  NOTREACHED();
}

void Compositor::OnFrameTokenChanged(uint32_t frame_token) {
  // TODO(yiyix, fsamuel): Implement frame token propagation for Compositor.
  NOTREACHED();
}

void Compositor::StartThroughputTracker(
    TrackerId tracker_id,
    ThroughputTrackerHost::ReportCallback callback) {
  DCHECK(!base::Contains(throughput_tracker_map_, tracker_id));
  throughput_tracker_map_[tracker_id] = std::move(callback);
  animation_host_->StartThroughputTracking(tracker_id);
}

void Compositor::StopThroughtputTracker(TrackerId tracker_id) {
  DCHECK(base::Contains(throughput_tracker_map_, tracker_id));
  animation_host_->StopThroughputTracking(tracker_id);
}

void Compositor::CancelThroughtputTracker(TrackerId tracker_id) {
  DCHECK(base::Contains(throughput_tracker_map_, tracker_id));
  StopThroughtputTracker(tracker_id);
  throughput_tracker_map_.erase(tracker_id);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void Compositor::OnCompleteSwapWithNewSize(const gfx::Size& size) {
  for (auto& observer : observer_list_)
    observer.OnCompositingCompleteSwapWithNewSize(this, size);
}
#endif

void Compositor::SetOutputIsSecure(bool output_is_secure) {
  output_is_secure_ = output_is_secure;
  if (display_private_)
    display_private_->SetOutputIsSecure(output_is_secure);
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->GetDebugState();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

void Compositor::RequestPresentationTimeForNextFrame(
    PresentationTimeCallback callback) {
  host_->RequestPresentationTimeForNextFrame(std::move(callback));
}

void Compositor::ReportThroughputForTracker(
    int tracker_id,
    cc::FrameSequenceMetrics::ThroughputData throughput) {
  auto it = throughput_tracker_map_.find(tracker_id);
  if (it == throughput_tracker_map_.end())
    return;

  std::move(it->second).Run(std::move(throughput));
  throughput_tracker_map_.erase(it);
}

}  // namespace ui
