// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <stddef.h>

#include <algorithm>
#include <deque>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/context_provider.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_switches.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

namespace ui {

Compositor::Compositor(const cc::FrameSinkId& frame_sink_id,
                       ui::ContextFactory* context_factory,
                       ui::ContextFactoryPrivate* context_factory_private,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       bool enable_surface_synchronization)
    : context_factory_(context_factory),
      context_factory_private_(context_factory_private),
      frame_sink_id_(frame_sink_id),
      task_runner_(task_runner),
      vsync_manager_(new CompositorVSyncManager()),
      layer_animator_collection_(this),
      scheduled_timeout_(base::TimeTicks()),
      allow_locks_to_extend_timeout_(false),
      weak_ptr_factory_(this),
      lock_timeout_weak_ptr_factory_(this) {
  if (context_factory_private) {
    context_factory_private->GetSurfaceManager()->RegisterFrameSinkId(
        frame_sink_id_);
  }
  root_web_layer_ = cc::Layer::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // This will ensure PictureLayers always can have LCD text, to match the
  // previous behaviour with ContentLayers, where LCD-not-allowed notifications
  // were ignored.
  settings.layers_always_allowed_lcd_text = true;
  // Use occlusion to allow more overlapping windows to take less memory.
  settings.use_occlusion_for_tile_prioritization = true;
  refresh_rate_ = context_factory_->GetRefreshRate();
  settings.main_frame_before_activation_enabled = false;

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
  settings.enable_surface_synchronization = enable_surface_synchronization;

  settings.use_zero_copy = IsUIZeroCopyEnabled();

  settings.use_layer_lists =
      command_line->HasSwitch(cc::switches::kUIEnableLayerLists);

  settings.enable_color_correct_rasterization =
      command_line->HasSwitch(switches::kEnableColorCorrectRendering);

  // UI compositor always uses partial raster if not using zero-copy. Zero copy
  // doesn't currently support partial raster.
  settings.use_partial_raster = !settings.use_zero_copy;

  if (command_line->HasSwitch(switches::kUIEnableRGBA4444Textures))
    settings.preferred_tile_format = cc::RGBA_4444;
  settings.resource_settings = context_factory_->GetResourceSettings();

  settings.gpu_memory_policy.bytes_limit_when_visible = 512 * 1024 * 1024;
  settings.gpu_memory_policy.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  settings.disallow_non_exact_resource_reuse =
      command_line->HasSwitch(cc::switches::kDisallowNonExactResourceReuse);

  base::TimeTicks before_create = base::TimeTicks::Now();

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  UMA_HISTOGRAM_TIMES("GPU.CreateBrowserCompositor",
                      base::TimeTicks::Now() - before_create);

  animation_timeline_ =
      cc::AnimationTimeline::Create(cc::AnimationIdProvider::NextTimelineId());
  animation_host_->AddAnimationTimeline(animation_timeline_.get());

  host_->SetRootLayer(root_web_layer_);
  host_->SetFrameSinkId(frame_sink_id_);
  host_->SetVisible(true);

  if (command_line->HasSwitch(switches::kUISlowAnimations)) {
    slow_animations_ = base::MakeUnique<ScopedAnimationDurationScaleMode>(
        ScopedAnimationDurationScaleMode::SLOW_DURATION);
  }
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

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
  if (context_factory_private_) {
    auto* manager = context_factory_private_->GetSurfaceManager();
    for (auto& client : child_frame_sinks_) {
      DCHECK(client.is_valid());
      manager->UnregisterFrameSinkHierarchy(frame_sink_id_, client);
    }
    manager->InvalidateFrameSinkId(frame_sink_id_);
  }
}

bool Compositor::IsForSubframe() {
  return false;
}

void Compositor::AddFrameSink(const cc::FrameSinkId& frame_sink_id) {
  if (!context_factory_private_)
    return;
  context_factory_private_->GetSurfaceManager()->RegisterFrameSinkHierarchy(
      frame_sink_id_, frame_sink_id);
  child_frame_sinks_.insert(frame_sink_id);
}

void Compositor::RemoveFrameSink(const cc::FrameSinkId& frame_sink_id) {
  if (!context_factory_private_)
    return;
  auto it = child_frame_sinks_.find(frame_sink_id);
  DCHECK(it != child_frame_sinks_.end());
  DCHECK(it->is_valid());
  context_factory_private_->GetSurfaceManager()->UnregisterFrameSinkHierarchy(
      frame_sink_id_, *it);
  child_frame_sinks_.erase(it);
}

void Compositor::SetLocalSurfaceId(const cc::LocalSurfaceId& local_surface_id) {
  host_->SetLocalSurfaceId(local_surface_id);
}

void Compositor::SetCompositorFrameSink(
    std::unique_ptr<cc::CompositorFrameSink> compositor_frame_sink) {
  compositor_frame_sink_requested_ = false;
  host_->SetCompositorFrameSink(std::move(compositor_frame_sink));
  // Display properties are reset when the output surface is lost, so update it
  // to match the Compositor's.
  if (context_factory_private_) {
    context_factory_private_->SetDisplayVisible(this, host_->IsVisible());
    context_factory_private_->SetDisplayColorSpace(this, blending_color_space_,
                                                   output_color_space_);
  }
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

void Compositor::SetHostHasTransparentBackground(
    bool host_has_transparent_background) {
  host_->set_has_transparent_background(host_has_transparent_background);
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedrawRect(gfx::Rect(host_->device_viewport_size()));
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

void Compositor::DisableSwapUntilResize() {
  context_factory_private_->ResizeDisplay(this, gfx::Size());
}

void Compositor::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  std::unique_ptr<cc::SwapPromise> swap_promise(
      new cc::LatencyInfoSwapPromise(latency_info));
  host_->QueueSwapPromise(std::move(swap_promise));
}

void Compositor::SetScaleAndSize(float scale, const gfx::Size& size_in_pixel) {
  DCHECK_GT(scale, 0);
  if (!size_in_pixel.IsEmpty()) {
    size_ = size_in_pixel;
    host_->SetViewportSize(size_in_pixel);
    root_web_layer_->SetBounds(size_in_pixel);
    // TODO(fsamuel): Get rid of ContextFactoryPrivate.
    if (context_factory_private_)
      context_factory_private_->ResizeDisplay(this, size_in_pixel);
  }
  if (device_scale_factor_ != scale) {
    device_scale_factor_ = scale;
    host_->SetDeviceScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetDisplayColorProfile(const gfx::ICCProfile& icc_profile) {
  blending_color_space_ = icc_profile.GetColorSpace();
  output_color_space_ = blending_color_space_;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableHDR)) {
    blending_color_space_ = gfx::ColorSpace::CreateExtendedSRGB();
    output_color_space_ = gfx::ColorSpace::CreateSCRGBLinear();
  }
  host_->SetRasterColorSpace(icc_profile.GetParametricColorSpace());
  // Color space is reset when the output surface is lost, so this must also be
  // updated then.
  // TODO(fsamuel): Get rid of this.
  if (context_factory_private_) {
    context_factory_private_->SetDisplayColorSpace(this, blending_color_space_,
                                                   output_color_space_);
  }
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
  // Visibility is reset when the output surface is lost, so this must also be
  // updated then.
  // TODO(fsamuel): Eliminate this call.
  if (context_factory_private_)
    context_factory_private_->SetDisplayVisible(this, visible);
}

bool Compositor::IsVisible() {
  return host_->IsVisible();
}

bool Compositor::ScrollLayerTo(int layer_id, const gfx::ScrollOffset& offset) {
  return host_->GetInputHandler()->ScrollLayerTo(layer_id, offset);
}

bool Compositor::GetScrollOffsetForLayer(int layer_id,
                                         gfx::ScrollOffset* offset) const {
  return host_->GetInputHandler()->GetScrollOffsetForLayer(layer_id, offset);
}

void Compositor::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  DCHECK_GT(interval.InMillisecondsF(), 0);
  refresh_rate_ =
      base::Time::kMillisecondsPerSecond / interval.InMillisecondsF();
  if (context_factory_private_)
    context_factory_private_->SetAuthoritativeVSyncInterval(this, interval);
  vsync_manager_->SetAuthoritativeVSyncInterval(interval);
}

void Compositor::SetDisplayVSyncParameters(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
  if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = cc::BeginFrameArgs::DefaultInterval();
  }
  DCHECK_GT(interval.InMillisecondsF(), 0);
  refresh_rate_ =
      base::Time::kMillisecondsPerSecond / interval.InMillisecondsF();

  if (context_factory_private_) {
    context_factory_private_->SetDisplayVSyncParameters(this, timebase,
                                                        interval);
  }
  vsync_manager_->UpdateVSyncParameters(timebase, interval);
}

void Compositor::SetAcceleratedWidget(gfx::AcceleratedWidget widget) {
  // This function should only get called once.
  DCHECK(!widget_valid_);
  widget_ = widget;
  widget_valid_ = true;
  if (compositor_frame_sink_requested_)
    context_factory_->CreateCompositorFrameSink(weak_ptr_factory_.GetWeakPtr());
}

gfx::AcceleratedWidget Compositor::ReleaseAcceleratedWidget() {
  DCHECK(!IsVisible());
  host_->ReleaseCompositorFrameSink();
  context_factory_->RemoveCompositor(this);
  widget_valid_ = false;
  gfx::AcceleratedWidget widget = widget_;
  widget_ = gfx::kNullAcceleratedWidget;
  return widget;
}

gfx::AcceleratedWidget Compositor::widget() const {
  DCHECK(widget_valid_);
  return widget_;
}

scoped_refptr<CompositorVSyncManager> Compositor::vsync_manager() const {
  return vsync_manager_;
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

void Compositor::BeginMainFrame(const cc::BeginFrameArgs& args) {
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

void Compositor::RequestNewCompositorFrameSink() {
  DCHECK(!compositor_frame_sink_requested_);
  compositor_frame_sink_requested_ = true;
  if (widget_valid_)
    context_factory_->CreateCompositorFrameSink(weak_ptr_factory_.GetWeakPtr());
}

void Compositor::DidFailToInitializeCompositorFrameSink() {
  // The CompositorFrameSink should already be bound/initialized before being
  // given to
  // the Compositor.
  NOTREACHED();
}

void Compositor::DidCommit() {
  DCHECK(!IsLocked());
  for (auto& observer : observer_list_)
    observer.OnCompositingDidCommit(this);
}

void Compositor::DidReceiveCompositorFrameAck() {
  ++activated_frame_count_;
  for (auto& observer : observer_list_)
    observer.OnCompositingEnded(this);
}

void Compositor::DidSubmitCompositorFrame() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (auto& observer : observer_list_)
    observer.OnCompositingStarted(this, start_time);
}

void Compositor::SetOutputIsSecure(bool output_is_secure) {
  if (context_factory_private_)
    context_factory_private_->SetOutputIsSecure(this, output_is_secure);
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->GetDebugState();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

std::unique_ptr<CompositorLock> Compositor::GetCompositorLock(
    CompositorLockClient* client,
    base::TimeDelta timeout) {
  // This uses the main WeakPtrFactory to break the connection from the lock to
  // the Compositor when the Compositor is destroyed.
  auto lock =
      base::MakeUnique<CompositorLock>(client, weak_ptr_factory_.GetWeakPtr());
  bool was_empty = active_locks_.empty();
  active_locks_.push_back(lock.get());

  bool should_extend_timeout = false;
  if ((was_empty || allow_locks_to_extend_timeout_) && !timeout.is_zero()) {
    const base::TimeTicks time_to_timeout = base::TimeTicks::Now() + timeout;
    // For the first lock, scheduled_timeout.is_null is true,
    // |time_to_timeout| will always larger than |scheduled_timeout_|. And it
    // is ok to invalidate the weakptr of |lock_timeout_weak_ptr_factory_|.
    if (time_to_timeout > scheduled_timeout_) {
      scheduled_timeout_ = time_to_timeout;
      should_extend_timeout = true;
      lock_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
    }
  }

  if (was_empty) {
    host_->SetDeferCommits(true);
    for (auto& observer : observer_list_)
      observer.OnCompositingLockStateChanged(this);
  }

  if (should_extend_timeout) {
    // The timeout task uses an independent WeakPtrFactory that is invalidated
    // when all locks are ended to prevent the timeout from leaking into
    // another lock that should have its own timeout.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Compositor::TimeoutLocks,
                   lock_timeout_weak_ptr_factory_.GetWeakPtr()),
        timeout);
  }
  return lock;
}

void Compositor::RemoveCompositorLock(CompositorLock* lock) {
  base::Erase(active_locks_, lock);
  if (active_locks_.empty()) {
    host_->SetDeferCommits(false);
    for (auto& observer : observer_list_)
      observer.OnCompositingLockStateChanged(this);
    lock_timeout_weak_ptr_factory_.InvalidateWeakPtrs();
    scheduled_timeout_ = base::TimeTicks();
  }
}

void Compositor::TimeoutLocks() {
  // Make a copy, we're going to cause |active_locks_| to become
  // empty.
  std::vector<CompositorLock*> locks = active_locks_;
  for (auto* lock : locks)
    lock->TimeoutLock();
  DCHECK(active_locks_.empty());
}

}  // namespace ui
