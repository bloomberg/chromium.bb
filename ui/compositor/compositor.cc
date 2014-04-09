// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/latency_info_swap_promise.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/context_provider.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/frame_time.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 200.0;

bool g_compositor_initialized = false;
base::Thread* g_compositor_thread = NULL;
cc::SharedBitmapManager* g_shared_bitmap_manager;

ui::ContextFactory* g_context_factory = NULL;

const int kCompositorLockTimeoutMs = 67;

}  // namespace

namespace ui {

// static
ContextFactory* ContextFactory::GetInstance() {
  DCHECK(g_context_factory);
  return g_context_factory;
}

// static
void ContextFactory::SetInstance(ContextFactory* instance) {
  DCHECK_NE(!!g_context_factory, !!instance);
  g_context_factory = instance;
}

CompositorLock::CompositorLock(Compositor* compositor)
    : compositor_(compositor) {
  base::MessageLoop::current()->PostDelayedTask(
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

}  // namespace ui

namespace {

}  // namespace

namespace ui {

Compositor::Compositor(gfx::AcceleratedWidget widget)
    : root_layer_(NULL),
      widget_(widget),
      vsync_manager_(new CompositorVSyncManager()),
      device_scale_factor_(0.0f),
      last_started_frame_(0),
      last_ended_frame_(0),
      next_draw_is_resize_(false),
      disable_schedule_composite_(false),
      compositor_lock_(NULL),
      defer_draw_scheduling_(false),
      waiting_on_compositing_end_(false),
      draw_on_compositing_end_(false),
      swap_state_(SWAP_NONE),
      schedule_draw_factory_(this) {
  DCHECK(g_compositor_initialized)
      << "Compositor::Initialize must be called before creating a Compositor.";

  root_web_layer_ = cc::Layer::Create();
  root_web_layer_->SetAnchorPoint(gfx::PointF(0.f, 0.f));

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;
  settings.refresh_rate =
      ContextFactory::GetInstance()->DoesCreateTestContexts()
      ? kTestRefreshRate
      : kDefaultRefreshRate;
  settings.main_frame_before_draw_enabled = false;
  settings.main_frame_before_activation_enabled = false;
  settings.throttle_frame_production =
      !command_line->HasSwitch(switches::kDisableGpuVsync);
  settings.partial_swap_enabled =
      !command_line->HasSwitch(cc::switches::kUIDisablePartialSwap);
#if defined(OS_CHROMEOS)
  settings.per_tile_painting_enabled = true;
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
  settings.use_map_image = IsUIMapImageEnabled();

  base::TimeTicks before_create = base::TimeTicks::Now();
  if (!!g_compositor_thread) {
    host_ = cc::LayerTreeHost::CreateThreaded(
        this,
        g_shared_bitmap_manager,
        settings,
        g_compositor_thread->message_loop_proxy());
  } else {
    host_ = cc::LayerTreeHost::CreateSingleThreaded(
        this, this, g_shared_bitmap_manager, settings);
  }
  UMA_HISTOGRAM_TIMES("GPU.CreateBrowserCompositor",
                      base::TimeTicks::Now() - before_create);
  host_->SetRootLayer(root_web_layer_);
  host_->SetLayerTreeHostClientReady();
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

  DCHECK(g_compositor_initialized);

  CancelCompositorLock();
  DCHECK(!compositor_lock_);

  if (root_layer_)
    root_layer_->SetCompositor(NULL);

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  ContextFactory::GetInstance()->RemoveCompositor(this);
}

// static
void Compositor::Initialize() {
#if defined(OS_CHROMEOS)
  bool use_thread = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUIDisableThreadedCompositing);
#else
  bool use_thread = false;
#endif
  if (use_thread) {
    g_compositor_thread = new base::Thread("Browser Compositor");
    g_compositor_thread->Start();
  }

  DCHECK(!g_compositor_initialized) << "Compositor initialized twice.";
  g_compositor_initialized = true;
}

// static
bool Compositor::WasInitializedWithThread() {
  DCHECK(g_compositor_initialized);
  return !!g_compositor_thread;
}

// static
scoped_refptr<base::MessageLoopProxy> Compositor::GetCompositorMessageLoop() {
  scoped_refptr<base::MessageLoopProxy> proxy;
  if (g_compositor_thread)
    proxy = g_compositor_thread->message_loop_proxy();
  return proxy;
}

// static
void Compositor::Terminate() {
  if (g_compositor_thread) {
    g_compositor_thread->Stop();
    delete g_compositor_thread;
    g_compositor_thread = NULL;
  }

  DCHECK(g_compositor_initialized) << "Compositor::Initialize() didn't happen.";
  g_compositor_initialized = false;
}

// static
void Compositor::SetSharedBitmapManager(cc::SharedBitmapManager* manager) {
  g_shared_bitmap_manager = manager;
}

void Compositor::ScheduleDraw() {
  if (g_compositor_thread) {
    host_->Composite(gfx::FrameTime::Now());
  } else if (!defer_draw_scheduling_) {
    defer_draw_scheduling_ = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&Compositor::Draw, schedule_draw_factory_.GetWeakPtr()));
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
  DCHECK(!g_compositor_thread);

  defer_draw_scheduling_ = false;
  if (waiting_on_compositing_end_) {
    draw_on_compositing_end_ = true;
    return;
  }
  waiting_on_compositing_end_ = true;

  TRACE_EVENT_ASYNC_BEGIN0("ui", "Compositor::Draw", last_started_frame_ + 1);

  if (!root_layer_)
    return;

  DCHECK_NE(swap_state_, SWAP_POSTED);
  swap_state_ = SWAP_NONE;

  last_started_frame_++;
  if (!IsLocked()) {
    // TODO(nduca): Temporary while compositor calls
    // compositeImmediately() directly.
    Layout();
    host_->Composite(gfx::FrameTime::Now());

#if defined(OS_WIN)
    // While we resize, we are usually a few frames behind. By blocking
    // the UI thread here we minize the area that is mis-painted, specially
    // in the non-client area. See RenderWidgetHostViewAura::SetBounds for
    // more details and bug 177115.
    if (next_draw_is_resize_ && (last_ended_frame_ > 1)) {
      next_draw_is_resize_ = false;
      host_->FinishAllRendering();
    }
#endif

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

    next_draw_is_resize_ = true;
  }
  if (device_scale_factor_ != scale) {
    device_scale_factor_ = scale;
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
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

bool Compositor::HasObserver(CompositorObserver* observer) {
  return observer_list_.HasObserver(observer);
}

void Compositor::Layout() {
  // We're sending damage that will be addressed during this composite
  // cycle, so we don't need to schedule another composite to address it.
  disable_schedule_composite_ = true;
  if (root_layer_)
    root_layer_->SendDamagedRects();
  disable_schedule_composite_ = false;
}

scoped_ptr<cc::OutputSurface> Compositor::CreateOutputSurface(bool fallback) {
  return ContextFactory::GetInstance()->CreateOutputSurface(this, fallback);
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
  if (g_compositor_thread) {
    NotifyEnd();
  } else {
    DCHECK_EQ(swap_state_, SWAP_POSTED);
    NotifyEnd();
    swap_state_ = SWAP_COMPLETED;
  }
}

scoped_refptr<cc::ContextProvider> Compositor::OffscreenContextProvider() {
  return ContextFactory::GetInstance()->OffscreenCompositorContextProvider();
}

void Compositor::ScheduleComposite() {
  if (!disable_schedule_composite_)
    ScheduleDraw();
}

void Compositor::ScheduleAnimation() {
  ScheduleComposite();
}

void Compositor::DidPostSwapBuffers() {
  DCHECK(!g_compositor_thread);
  DCHECK_EQ(swap_state_, SWAP_NONE);
  swap_state_ = SWAP_POSTED;
}

void Compositor::DidAbortSwapBuffers() {
  if (!g_compositor_thread) {
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

scoped_refptr<CompositorLock> Compositor::GetCompositorLock() {
  if (!compositor_lock_) {
    compositor_lock_ = new CompositorLock(this);
    if (g_compositor_thread)
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
  if (g_compositor_thread)
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
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingEnded(this));
}

}  // namespace ui
