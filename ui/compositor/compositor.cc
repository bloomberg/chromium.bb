// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/context_provider.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/gfx/frame_time.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 200.0;

const int kCompositorLockTimeoutMs = 67;

}  // namespace

namespace ui {

CompositorLock::CompositorLock(Compositor* compositor)
    : compositor_(compositor) {
  compositor_->task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CompositorLock::CancelLock, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
}

CompositorLock::~CompositorLock() {
  CancelLock();
}

void CompositorLock::CancelLock() {
  if (!compositor_)
    return;
  compositor_->UnlockCompositor();
  compositor_ = NULL;
}

Compositor::Compositor(gfx::AcceleratedWidget widget,
                       ui::ContextFactory* context_factory,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_factory_(context_factory),
      root_layer_(NULL),
      widget_(widget),
      surface_id_allocator_(context_factory->CreateSurfaceIdAllocator()),
      compositor_thread_loop_(context_factory->GetCompositorMessageLoop()),
      task_runner_(task_runner),
      vsync_manager_(new CompositorVSyncManager()),
      device_scale_factor_(0.0f),
      last_started_frame_(0),
      last_ended_frame_(0),
      num_failed_recreate_attempts_(0),
      disable_schedule_composite_(false),
      compositor_lock_(NULL),
      defer_draw_scheduling_(false),
      waiting_on_compositing_end_(false),
      draw_on_compositing_end_(false),
      swap_state_(SWAP_NONE),
      layer_animator_collection_(this),
      weak_ptr_factory_(this) {
  root_web_layer_ = cc::Layer::Create();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;
  // When impl-side painting is enabled, this will ensure PictureLayers always
  // can have LCD text, to match the previous behaviour with ContentLayers,
  // where LCD-not-allowed notifications were ignored.
  settings.layers_always_allowed_lcd_text = true;
  settings.renderer_settings.refresh_rate =
      context_factory_->DoesCreateTestContexts() ? kTestRefreshRate
                                                 : kDefaultRefreshRate;
  settings.main_frame_before_activation_enabled = false;
  settings.throttle_frame_production =
      !command_line->HasSwitch(switches::kDisableGpuVsync);
#if !defined(OS_MACOSX)
  settings.renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(cc::switches::kUIDisablePartialSwap);
#endif
#if defined(OS_CHROMEOS)
  settings.per_tile_painting_enabled = true;
#endif
#if defined(OS_WIN)
  settings.disable_hi_res_timer_tasks_on_battery = true;
  settings.renderer_settings.finish_rendering_on_resize = true;
#endif

  // These flags should be mirrored by renderer versions in content/renderer/.
  settings.initial_debug_state.show_debug_borders =
      command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders);
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
  settings.initial_debug_state.show_replica_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowReplicaScreenSpaceRects);
  settings.initial_debug_state.show_occluding_rects =
      command_line->HasSwitch(cc::switches::kUIShowOccludingRects);
  settings.initial_debug_state.show_non_occluding_rects =
      command_line->HasSwitch(cc::switches::kUIShowNonOccludingRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  settings.impl_side_painting = IsUIImplSidePaintingEnabled();
  settings.use_zero_copy = IsUIZeroCopyEnabled();
  settings.single_thread_proxy_scheduler = false;

  base::TimeTicks before_create = base::TimeTicks::Now();
  if (compositor_thread_loop_.get()) {
    host_ = cc::LayerTreeHost::CreateThreaded(
        this,
        context_factory_->GetSharedBitmapManager(),
        context_factory_->GetGpuMemoryBufferManager(),
        settings,
        task_runner_,
        compositor_thread_loop_,
        nullptr);
  } else {
    host_ = cc::LayerTreeHost::CreateSingleThreaded(
        this,
        this,
        context_factory_->GetSharedBitmapManager(),
        context_factory_->GetGpuMemoryBufferManager(),
        settings,
        task_runner_,
        nullptr);
  }
  UMA_HISTOGRAM_TIMES("GPU.CreateBrowserCompositor",
                      base::TimeTicks::Now() - before_create);
  host_->SetRootLayer(root_web_layer_);
  host_->set_surface_id_namespace(surface_id_allocator_->id_namespace());
  host_->SetLayerTreeHostClientReady();
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

  CancelCompositorLock();
  DCHECK(!compositor_lock_);

  if (root_layer_)
    root_layer_->SetCompositor(NULL);

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
}

void Compositor::SetOutputSurface(
    scoped_ptr<cc::OutputSurface> output_surface) {
  host_->SetOutputSurface(output_surface.Pass());
}

void Compositor::ScheduleDraw() {
  if (compositor_thread_loop_.get()) {
    host_->SetNeedsCommit();
  } else if (!defer_draw_scheduling_) {
    defer_draw_scheduling_ = true;
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Compositor::Draw, weak_ptr_factory_.GetWeakPtr()));
  }
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->SetCompositor(NULL);
  root_layer_ = root_layer;
  if (root_layer_ && !root_layer_->GetCompositor())
    root_layer_->SetCompositor(this);
  root_web_layer_->RemoveAllChildren();
  if (root_layer_)
    root_web_layer_->AddChild(root_layer_->cc_layer());
}

void Compositor::SetHostHasTransparentBackground(
    bool host_has_transparent_background) {
  host_->set_has_transparent_background(host_has_transparent_background);
}

void Compositor::Draw() {
  DCHECK(!compositor_thread_loop_.get());

  defer_draw_scheduling_ = false;
  if (waiting_on_compositing_end_) {
    draw_on_compositing_end_ = true;
    return;
  }
  if (!root_layer_)
    return;

  TRACE_EVENT_ASYNC_BEGIN0("ui", "Compositor::Draw", last_started_frame_ + 1);

  DCHECK_NE(swap_state_, SWAP_POSTED);
  swap_state_ = SWAP_NONE;

  waiting_on_compositing_end_ = true;
  last_started_frame_++;
  if (!IsLocked()) {
    // TODO(nduca): Temporary while compositor calls
    // compositeImmediately() directly.
    cc::BeginFrameArgs args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, gfx::FrameTime::Now(), base::TimeTicks(),
        cc::BeginFrameArgs::DefaultInterval(), cc::BeginFrameArgs::SYNCHRONOUS);
    BeginMainFrame(args);
    host_->Composite(args.frame_time);
  }
  if (swap_state_ == SWAP_NONE)
    NotifyEnd();
}

void Compositor::ScheduleFullRedraw() {
  host_->SetNeedsRedraw();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  host_->SetNeedsRedrawRect(damage_rect);
}

void Compositor::DisableSwapUntilResize() {
  host_->FinishAllRendering();
  context_factory_->ResizeDisplay(this, gfx::Size());
}

void Compositor::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  scoped_ptr<cc::SwapPromise> swap_promise(
      new cc::LatencyInfoSwapPromise(latency_info));
  host_->QueueSwapPromise(swap_promise.Pass());
}

void Compositor::SetScaleAndSize(float scale, const gfx::Size& size_in_pixel) {
  DCHECK_GT(scale, 0);
  if (!size_in_pixel.IsEmpty()) {
    size_ = size_in_pixel;
    host_->SetViewportSize(size_in_pixel);
    root_web_layer_->SetBounds(size_in_pixel);
    context_factory_->ResizeDisplay(this, size_in_pixel);
  }
  if (device_scale_factor_ != scale) {
    device_scale_factor_ = scale;
    host_->SetDeviceScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
}

scoped_refptr<CompositorVSyncManager> Compositor::vsync_manager() const {
  return vsync_manager_;
}

void Compositor::AddObserver(CompositorObserver* observer) {
#if defined(OS_MACOSX)
  // Debugging instrumentation for crbug.com/401630.
  // TODO(ccameron): remove this.
  CHECK(observer);
  if (!observer_list_.HasObserver(observer))
    observer->observing_count_ += 1;
#endif

  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
#if defined(OS_MACOSX)
  // Debugging instrumentation for crbug.com/401630.
  // TODO(ccameron): remove this.
  if (observer_list_.HasObserver(observer))
    observer->observing_count_ -= 1;
#endif

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
  FOR_EACH_OBSERVER(CompositorAnimationObserver,
                    animation_observer_list_,
                    OnAnimationStep(args.frame_time));
  if (animation_observer_list_.might_have_observers())
    host_->SetNeedsAnimate();
}

void Compositor::Layout() {
  // We're sending damage that will be addressed during this composite
  // cycle, so we don't need to schedule another composite to address it.
  disable_schedule_composite_ = true;
  if (root_layer_)
    root_layer_->SendDamagedRects();
  disable_schedule_composite_ = false;
}

void Compositor::RequestNewOutputSurface() {
  bool fallback =
      num_failed_recreate_attempts_ >= OUTPUT_SURFACE_RETRIES_BEFORE_FALLBACK;
  context_factory_->CreateOutputSurface(weak_ptr_factory_.GetWeakPtr(),
                                        fallback);
}

void Compositor::DidInitializeOutputSurface() {
  num_failed_recreate_attempts_ = 0;
}

void Compositor::DidFailToInitializeOutputSurface() {
  num_failed_recreate_attempts_++;

  // Tolerate a certain number of recreation failures to work around races
  // in the output-surface-lost machinery.
  if (num_failed_recreate_attempts_ >= MAX_OUTPUT_SURFACE_RETRIES)
    LOG(FATAL) << "Failed to create a fallback OutputSurface.";

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Compositor::RequestNewOutputSurface,
                            weak_ptr_factory_.GetWeakPtr()));
}

void Compositor::DidCommit() {
  DCHECK(!IsLocked());
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingDidCommit(this));
}

void Compositor::DidCommitAndDrawFrame() {
  base::TimeTicks start_time = gfx::FrameTime::Now();
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingStarted(this, start_time));
}

void Compositor::DidCompleteSwapBuffers() {
  if (compositor_thread_loop_.get()) {
    NotifyEnd();
  } else {
    DCHECK_EQ(swap_state_, SWAP_POSTED);
    NotifyEnd();
    swap_state_ = SWAP_COMPLETED;
  }
}

void Compositor::ScheduleComposite() {
  if (!disable_schedule_composite_)
    ScheduleDraw();
}

void Compositor::ScheduleAnimation() {
  ScheduleComposite();
}

void Compositor::DidPostSwapBuffers() {
  DCHECK(!compositor_thread_loop_.get());
  DCHECK_EQ(swap_state_, SWAP_NONE);
  swap_state_ = SWAP_POSTED;
}

void Compositor::DidAbortSwapBuffers() {
  if (!compositor_thread_loop_.get()) {
    if (swap_state_ == SWAP_POSTED) {
      NotifyEnd();
      swap_state_ = SWAP_COMPLETED;
    }
  }

  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingAborted(this));
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->debug_state();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

const cc::RendererSettings& Compositor::GetRendererSettings() const {
  return host_->settings().renderer_settings;
}

scoped_refptr<CompositorLock> Compositor::GetCompositorLock() {
  if (!compositor_lock_) {
    compositor_lock_ = new CompositorLock(this);
    if (compositor_thread_loop_.get())
      host_->SetDeferCommits(true);
    FOR_EACH_OBSERVER(CompositorObserver,
                      observer_list_,
                      OnCompositingLockStateChanged(this));
  }
  return compositor_lock_;
}

void Compositor::UnlockCompositor() {
  DCHECK(compositor_lock_);
  compositor_lock_ = NULL;
  if (compositor_thread_loop_.get())
    host_->SetDeferCommits(false);
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingLockStateChanged(this));
}

void Compositor::CancelCompositorLock() {
  if (compositor_lock_)
    compositor_lock_->CancelLock();
}

void Compositor::NotifyEnd() {
  last_ended_frame_++;
  TRACE_EVENT_ASYNC_END0("ui", "Compositor::Draw", last_ended_frame_);
  waiting_on_compositing_end_ = false;
  if (draw_on_compositing_end_) {
    draw_on_compositing_end_ = false;

    // Call ScheduleDraw() instead of Draw() in order to allow other
    // CompositorObservers to be notified before starting another
    // draw cycle.
    ScheduleDraw();
  }
  FOR_EACH_OBSERVER(
      CompositorObserver, observer_list_, OnCompositingEnded(this));
}

}  // namespace ui
