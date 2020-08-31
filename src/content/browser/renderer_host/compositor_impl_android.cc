// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_impl_android.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <stdint.h>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/host/host_display_client.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/swap_buffers_flags.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"
#include "ui/display/display_transform.h"
#include "ui/display/screen.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/swap_result.h"
#include "ui/latency/latency_tracker.h"

namespace content {

namespace {

static const char* kBrowser = "Browser";

// NOINLINE to make sure crashes use this for magic signature.
NOINLINE void FatalSurfaceFailure() {
  LOG(FATAL) << "Fatal surface initialization failure";
}

gpu::SharedMemoryLimits GetCompositorContextSharedMemoryLimits(
    gfx::NativeWindow window) {
  const gfx::Size screen_size = display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(window)
                                    .GetSizeInPixel();
  return gpu::SharedMemoryLimits::ForDisplayCompositor(screen_size);
}

gpu::ContextCreationAttribs GetCompositorContextAttributes(
    const gfx::ColorSpace& display_color_space,
    bool requires_alpha_channel) {
  // This is used for the browser compositor (offscreen) and for the display
  // compositor (onscreen), so ask for capabilities needed by either one.
  // The default framebuffer for an offscreen context is not used, so it does
  // not need alpha, stencil, depth, antialiasing. The display compositor does
  // not use these things either, except for alpha when it has a transparent
  // background.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.stencil_size = 0;
  attributes.depth_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  if (display_color_space == gfx::ColorSpace::CreateSRGB()) {
    attributes.color_space = gpu::COLOR_SPACE_SRGB;
  } else if (display_color_space == gfx::ColorSpace::CreateDisplayP3D65()) {
    attributes.color_space = gpu::COLOR_SPACE_DISPLAY_P3;
  } else {
    attributes.color_space = gpu::COLOR_SPACE_UNSPECIFIED;
    DLOG(ERROR) << "Android color space is neither sRGB nor P3, output color "
                   "will be incorrect.";
  }

  if (requires_alpha_channel) {
    attributes.alpha_size = 8;
  } else if (viz::PreferRGB565ResourcesForDisplay()) {
    // In this case we prefer to use RGB565 format instead of RGBA8888 if
    // possible.
    // TODO(danakj): CommandBufferStub constructor checks for alpha == 0
    // in order to enable 565, but it should avoid using 565 when -1s are
    // specified
    // (IOW check that a <= 0 && rgb > 0 && rgb <= 565) then alpha should be
    // -1.
    // TODO(liberato): This condition is memorized in ComositorView.java, to
    // avoid using two surfaces temporarily during alpha <-> no alpha
    // transitions.  If these mismatch, then we risk a power regression if the
    // SurfaceView is not marked as eOpaque (FORMAT_OPAQUE), and we have an
    // EGL surface with an alpha channel.  SurfaceFlinger needs at least one of
    // those hints to optimize out alpha blending.
    attributes.alpha_size = 0;
    attributes.red_size = 5;
    attributes.green_size = 6;
    attributes.blue_size = 5;
  }

  attributes.enable_swap_timestamps_if_supported = true;

  return attributes;
}

void CreateContextProviderAfterGpuChannelEstablished(
    gpu::SurfaceHandle handle,
    gpu::ContextCreationAttribs attributes,
    gpu::SharedMemoryLimits shared_memory_limits,
    Compositor::ContextProviderCallback callback,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  if (!gpu_channel_host) {
    std::move(callback).Run(nullptr);
    return;
  }

  gpu::GpuChannelEstablishFactory* factory =
      BrowserGpuChannelHostFactory::instance();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool automatic_flushes = false;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = false;

  auto context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, handle,
          GURL(std::string("chrome://gpu/Compositor::CreateContextProvider")),
          automatic_flushes, support_locking, support_grcontext,
          shared_memory_limits, attributes,
          viz::command_buffer_metrics::ContextType::UNKNOWN);
  std::move(callback).Run(std::move(context_provider));
}

static bool g_initialized = false;

}  // anonymous namespace

// An implementation of HostDisplayClient which handles swap callbacks.
class CompositorImpl::AndroidHostDisplayClient : public viz::HostDisplayClient {
 public:
  explicit AndroidHostDisplayClient(CompositorImpl* compositor)
      : HostDisplayClient(gfx::kNullAcceleratedWidget),
        compositor_(compositor) {}

  // viz::mojom::DisplayClient implementation:
  void DidCompleteSwapWithSize(const gfx::Size& pixel_size) override {
    compositor_->DidSwapBuffers(pixel_size);
  }
  void OnContextCreationResult(gpu::ContextResult context_result) override {
    compositor_->OnContextCreationResult(context_result);
  }
  void SetWideColorEnabled(bool enabled) override {
    // TODO(cblume): Add support for multiple compositors.
    // If one goes wide, all should go wide.
    if (compositor_->root_window_)
      compositor_->root_window_->SetWideColorEnabled(enabled);
  }
  void SetPreferredRefreshRate(float refresh_rate) override {
    if (compositor_->root_window_)
      compositor_->root_window_->SetPreferredRefreshRate(refresh_rate);
  }

 private:
  CompositorImpl* compositor_;
};

class CompositorImpl::ScopedCachedBackBuffer {
 public:
  ScopedCachedBackBuffer(const viz::FrameSinkId& root_sink_id) {
    cache_id_ =
        GetHostFrameSinkManager()->CacheBackBufferForRootSink(root_sink_id);
  }
  ~ScopedCachedBackBuffer() {
    GetHostFrameSinkManager()->EvictCachedBackBuffer(cache_id_);
  }

 private:
  uint32_t cache_id_;
};

class CompositorImpl::ReadbackRefImpl
    : public ui::WindowAndroidCompositor::ReadbackRef {
 public:
  explicit ReadbackRefImpl(base::WeakPtr<CompositorImpl> weakptr);
  ~ReadbackRefImpl() override;

 private:
  base::WeakPtr<CompositorImpl> compositor_weakptr_;
};

CompositorImpl::ReadbackRefImpl::ReadbackRefImpl(
    base::WeakPtr<CompositorImpl> weakptr)
    : compositor_weakptr_(weakptr) {}

CompositorImpl::ReadbackRefImpl::~ReadbackRefImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (compositor_weakptr_)
    compositor_weakptr_->DecrementPendingReadbacks();
}

// static
Compositor* Compositor::Create(CompositorClient* client,
                               gfx::NativeWindow root_window) {
  return client ? new CompositorImpl(client, root_window) : NULL;
}

// static
void Compositor::Initialize() {
  DCHECK(!CompositorImpl::IsInitialized());
  g_initialized = true;
}

// static
void Compositor::CreateContextProvider(
    gpu::SurfaceHandle handle,
    gpu::ContextCreationAttribs attributes,
    gpu::SharedMemoryLimits shared_memory_limits,
    ContextProviderCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(
      base::BindOnce(&CreateContextProviderAfterGpuChannelEstablished, handle,
                     attributes, shared_memory_limits, std::move(callback)));
}

// static
bool CompositorImpl::IsInitialized() {
  return g_initialized;
}

CompositorImpl::CompositorImpl(CompositorClient* client,
                               gfx::NativeWindow root_window)
    : frame_sink_id_(AllocateFrameSinkId()),
      resource_manager_(root_window),
      window_(NULL),
      surface_handle_(gpu::kNullSurfaceHandle),
      client_(client),
      needs_animate_(false),
      pending_frames_(0U),
      layer_tree_frame_sink_request_pending_(false) {
  DCHECK(client);

  SetRootWindow(root_window);

  // Listen to display density change events and update painted device scale
  // factor accordingly.
  display::Screen::GetScreen()->AddObserver(this);
}

CompositorImpl::~CompositorImpl() {
  display::Screen::GetScreen()->RemoveObserver(this);
  DetachRootWindow();
  // Clean-up any surface references.
  SetSurface(nullptr, false);

  BrowserGpuChannelHostFactory::instance()->MaybeCloseChannel();
}

void CompositorImpl::DetachRootWindow() {
  root_window_->DetachCompositor();
  root_window_->SetLayer(nullptr);
}

ui::UIResourceProvider& CompositorImpl::GetUIResourceProvider() {
  return *this;
}

ui::ResourceManager& CompositorImpl::GetResourceManager() {
  return resource_manager_;
}

void CompositorImpl::SetRootWindow(gfx::NativeWindow root_window) {
  DCHECK(root_window);
  DCHECK(!root_window->GetLayer());

  // TODO(mthiesse): Right now we only support swapping the root window without
  // a surface. If we want to support swapping with a surface we need to
  // handle visibility, swapping begin frame sources, etc.
  // These checks ensure we have no begin frame source, and that we don't need
  // to register one on the new window.
  DCHECK(!window_);

  scoped_refptr<cc::Layer> root_layer;
  if (root_window_) {
    root_layer = root_window_->GetLayer();
    DetachRootWindow();
  }

  root_window_ = root_window;
  if (base::FeatureList::IsEnabled(features::kForce60HzRefreshRate))
    root_window_->SetForce60HzRefreshRate();
  root_window_->SetLayer(root_layer ? root_layer : cc::Layer::Create());
  root_window_->GetLayer()->SetBounds(size_);
  root_window->AttachCompositor(this);
  if (!host_) {
    CreateLayerTreeHost();
    resource_manager_.Init(host_->GetUIResourceManager());
  }
  host_->SetRootLayer(root_window_->GetLayer());
  host_->SetViewportRectAndScale(gfx::Rect(size_), root_window_->GetDipScale(),
                                 GenerateLocalSurfaceId());
}

void CompositorImpl::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (subroot_layer_.get()) {
    subroot_layer_->RemoveFromParent();
    subroot_layer_ = nullptr;
  }
  if (root_layer.get() && root_window_->GetLayer()) {
    subroot_layer_ = root_window_->GetLayer();
    subroot_layer_->AddChild(root_layer);
  }
}

void CompositorImpl::SetSurface(jobject surface,
                                bool can_be_used_with_surface_control) {
  can_be_used_with_surface_control &=
      !root_window_->ApplyDisableSurfaceControlWorkaround();

  JNIEnv* env = base::android::AttachCurrentThread();
  gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();

  if (window_) {
    // Shut down GL context before unregistering surface.
    SetVisible(false);
    tracker->RemoveSurface(surface_handle_);
    ANativeWindow_release(window_);
    window_ = NULL;
    surface_handle_ = gpu::kNullSurfaceHandle;
  }

  ANativeWindow* window = NULL;
  if (surface) {
    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window = ANativeWindow_fromSurface(env, surface);
  }

  if (window) {
    window_ = window;
    ANativeWindow_acquire(window);
    // Register first, SetVisible() might create a LayerTreeFrameSink.
    surface_handle_ = tracker->AddSurfaceForNativeWidget(
        gpu::GpuSurfaceTracker::SurfaceRecord(
            window, surface, can_be_used_with_surface_control));
    SetVisible(true);
    ANativeWindow_release(window);
  }
}

void CompositorImpl::SetBackgroundColor(int color) {
  DCHECK(host_);
  host_->set_background_color(color);
}

void CompositorImpl::CreateLayerTreeHost() {
  DCHECK(!host_);

  cc::LayerTreeSettings settings;
  settings.use_zero_copy = true;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  if (command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders))
    settings.initial_debug_state.show_debug_borders.set();
  settings.single_thread_proxy_scheduler = true;
  settings.use_painted_device_scale_factor = true;
  // TODO(crbug.com/933846): LatencyRecovery is causing jank on Android. Disable
  // for now, with a plan to disable more widely once viz launches.
  settings.enable_impl_latency_recovery = false;
  settings.enable_main_latency_recovery = false;

  animation_host_ = cc::AnimationHost::CreateMainInstance();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner =
      CompositorDependenciesAndroid::Get().GetTaskGraphRunner();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.settings = &settings;
  params.mutator_host = animation_host_.get();
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, std::move(params));
  DCHECK(!host_->IsVisible());
  host_->SetViewportRectAndScale(gfx::Rect(size_), root_window_->GetDipScale(),
                                 GenerateLocalSurfaceId());
  const auto& display_props =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);
  host_->set_display_transform_hint(
      display::DisplayRotationToOverlayTransform(display_props.rotation()));

  if (needs_animate_)
    host_->SetNeedsAnimate();
}

void CompositorImpl::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "CompositorImpl::SetVisible", "visible", visible);

  if (!visible) {
    DCHECK(host_->IsVisible());
    // Tear down the display first, synchronously completing any pending
    // draws/readbacks if poosible.
    TearDownDisplayAndUnregisterRootFrameSink();
    // Hide the LayerTreeHost and release its frame sink.
    host_->SetVisible(false);
    host_->ReleaseLayerTreeFrameSink();
    pending_frames_ = 0;

    // Notify CompositorDependenciesAndroid of visibility changes last, to
    // ensure that we don't disable the GPU watchdog until sync IPCs above are
    // completed.
    CompositorDependenciesAndroid::Get().OnCompositorHidden(this);
  } else {
    DCHECK(!host_->IsVisible());
    CompositorDependenciesAndroid::Get().OnCompositorVisible(this);
    RegisterRootFrameSink();
    host_->SetVisible(true);
    has_submitted_frame_since_became_visible_ = false;
    if (layer_tree_frame_sink_request_pending_)
      HandlePendingLayerTreeFrameSinkRequest();
  }
}

void CompositorImpl::TearDownDisplayAndUnregisterRootFrameSink() {
  // Make a best effort to try to complete pending readbacks.
  // TODO(crbug.com/637035): Consider doing this in a better way,
  // ideally with the guarantee of readbacks completing.
  if (display_private_ && pending_readbacks_) {
    // Note that while this is not a Sync IPC, the call to
    // InvalidateFrameSinkId below will end up triggering a sync call to
    // FrameSinkManager::DestroyCompositorFrameSink, as this is the root
    // frame sink. Because |display_private_| is an associated remote to
    // FrameSinkManager, this subsequent sync call will ensure ordered
    // execution of this call.
    display_private_->ForceImmediateDrawAndSwapIfPossible();
  }
  GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);
  display_private_.reset();
}

void CompositorImpl::RegisterRootFrameSink() {
  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "CompositorImpl");
  for (auto& frame_sink_id : pending_child_frame_sink_ids_)
    AddChildFrameSink(frame_sink_id);
  pending_child_frame_sink_ids_.clear();
}

void CompositorImpl::SetWindowBounds(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  if (host_) {
    // TODO(ccameron): Ensure a valid LocalSurfaceId here.
    host_->SetViewportRectAndScale(gfx::Rect(size_),
                                   root_window_->GetDipScale(),
                                   GenerateLocalSurfaceId());
  }

  if (display_private_)
    display_private_->Resize(size);

  root_window_->GetLayer()->SetBounds(size);
}

void CompositorImpl::SetRequiresAlphaChannel(bool flag) {
  requires_alpha_channel_ = flag;
}

void CompositorImpl::SetNeedsComposite() {
  if (!host_->IsVisible())
    return;
  TRACE_EVENT0("compositor", "Compositor::SetNeedsComposite");
  host_->SetNeedsAnimate();
}

void CompositorImpl::SetNeedsRedraw() {
  host_->SetNeedsRedrawRect(host_->device_viewport_rect());
}

void CompositorImpl::DidUpdateLayers() {
  // Dump property trees and layers if run with:
  //   --vmodule=compositor_impl_android=3
  VLOG(3) << "After updating layers:\n"
          << "property trees:\n"
          << host_->property_trees()->ToString() << "\n"
          << "cc::Layers:\n"
          << host_->LayersAsString();
}

void CompositorImpl::BeginMainFrame(const viz::BeginFrameArgs& args) {
  latest_frame_time_ = args.frame_time;
}

void CompositorImpl::UpdateLayerTreeHost() {
  DCHECK(!latest_frame_time_.is_null());
  client_->UpdateLayerTreeHost();
  if (needs_animate_) {
    needs_animate_ = false;
    root_window_->Animate(latest_frame_time_);
  }
}

void CompositorImpl::RequestNewLayerTreeFrameSink() {
  DCHECK(!layer_tree_frame_sink_request_pending_)
      << "LayerTreeFrameSink request is already pending?";

  layer_tree_frame_sink_request_pending_ = true;
  HandlePendingLayerTreeFrameSinkRequest();
}

void CompositorImpl::DidInitializeLayerTreeFrameSink() {
  layer_tree_frame_sink_request_pending_ = false;
}

void CompositorImpl::DidFailToInitializeLayerTreeFrameSink() {
  layer_tree_frame_sink_request_pending_ = false;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CompositorImpl::RequestNewLayerTreeFrameSink,
                                weak_factory_.GetWeakPtr()));
}

void CompositorImpl::HandlePendingLayerTreeFrameSinkRequest() {
  DCHECK(layer_tree_frame_sink_request_pending_);

  // We might have been made invisible now.
  if (!host_->IsVisible())
    return;

  DCHECK(surface_handle_ != gpu::kNullSurfaceHandle);
  BrowserGpuChannelHostFactory::instance()->EstablishGpuChannel(base::BindOnce(
      &CompositorImpl::OnGpuChannelEstablished, weak_factory_.GetWeakPtr()));
}

void CompositorImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  // At this point we know we have a valid GPU process, establish our viz
  // connection if needed.
  CompositorDependenciesAndroid::Get().TryEstablishVizConnectionIfNeeded();

  // We might end up queing multiple GpuChannel requests for the same
  // LayerTreeFrameSink request as the visibility of the compositor changes, so
  // the LayerTreeFrameSink request could have been handled already.
  if (!layer_tree_frame_sink_request_pending_)
    return;

  if (!gpu_channel_host) {
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  // We don't need the context anymore if we are invisible.
  if (!host_->IsVisible()) {
    return;
  }

  DCHECK(window_);
  DCHECK_NE(surface_handle_, gpu::kNullSurfaceHandle);

  gpu::GpuChannelEstablishFactory* factory =
      BrowserGpuChannelHostFactory::instance();

  int32_t stream_id = kGpuStreamIdDefault;
  gpu::SchedulingPriority stream_priority = kGpuStreamPriorityUI;

  constexpr bool support_locking = false;
  constexpr bool automatic_flushes = false;
  constexpr bool support_grcontext = true;
  display_color_spaces_ = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(root_window_)
                              .color_spaces();

  auto context_provider =
      base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
          std::move(gpu_channel_host), factory->GetGpuMemoryBufferManager(),
          stream_id, stream_priority, gpu::kNullSurfaceHandle,
          GURL(std::string("chrome://gpu/CompositorImpl::") +
               std::string("CompositorContextProvider")),
          automatic_flushes, support_locking, support_grcontext,
          GetCompositorContextSharedMemoryLimits(root_window_),
          GetCompositorContextAttributes(
              display_color_spaces_.GetRasterColorSpace(),
              requires_alpha_channel_),
          viz::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);
  auto result = context_provider->BindToCurrentThread();

  if (result == gpu::ContextResult::kFatalFailure) {
    LOG(FATAL) << "Fatal failure in creating offscreen context";
  }

  if (result != gpu::ContextResult::kSuccess) {
    HandlePendingLayerTreeFrameSinkRequest();
    return;
  }

  InitializeVizLayerTreeFrameSink(std::move(context_provider));
}

void CompositorImpl::DidSwapBuffers(const gfx::Size& swap_size) {
  client_->DidSwapBuffers(swap_size);

  if (swap_completed_with_size_for_testing_)
    swap_completed_with_size_for_testing_.Run(swap_size);
}

cc::UIResourceId CompositorImpl::CreateUIResource(
    cc::UIResourceClient* client) {
  TRACE_EVENT0("compositor", "CompositorImpl::CreateUIResource");
  return host_->GetUIResourceManager()->CreateUIResource(client);
}

void CompositorImpl::DeleteUIResource(cc::UIResourceId resource_id) {
  TRACE_EVENT0("compositor", "CompositorImpl::DeleteUIResource");
  host_->GetUIResourceManager()->DeleteUIResource(resource_id);
}

bool CompositorImpl::SupportsETC1NonPowerOfTwo() const {
  return gpu_capabilities_.texture_format_etc1_npot;
}

void CompositorImpl::DidSubmitCompositorFrame() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidSubmitCompositorFrame");
  pending_frames_++;
  has_submitted_frame_since_became_visible_ = true;
}

void CompositorImpl::DidReceiveCompositorFrameAck() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidReceiveCompositorFrameAck");
  DCHECK_GT(pending_frames_, 0U);
  pending_frames_--;
  client_->DidSwapFrame(pending_frames_);
}

void CompositorImpl::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("compositor", "CompositorImpl::DidLoseLayerTreeFrameSink");
  client_->DidSwapFrame(0);
}

void CompositorImpl::DidCommit(base::TimeTicks) {
  root_window_->OnCompositingDidCommit();
}

std::unique_ptr<cc::BeginMainFrameMetrics>
CompositorImpl::GetBeginMainFrameMetrics() {
  return nullptr;
}

std::unique_ptr<ui::WindowAndroidCompositor::ReadbackRef>
CompositorImpl::TakeReadbackRef() {
  ++pending_readbacks_;
  return std::make_unique<ReadbackRefImpl>(weak_factory_.GetWeakPtr());
}

void CompositorImpl::RequestCopyOfOutputOnRootLayer(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  root_window_->GetLayer()->RequestCopyOfOutput(std::move(request));
}

void CompositorImpl::SetNeedsAnimate() {
  needs_animate_ = true;
  if (!host_->IsVisible())
    return;

  TRACE_EVENT0("compositor", "Compositor::SetNeedsAnimate");
  host_->SetNeedsAnimate();
}

viz::FrameSinkId CompositorImpl::GetFrameSinkId() {
  return frame_sink_id_;
}

void CompositorImpl::AddChildFrameSink(const viz::FrameSinkId& frame_sink_id) {
  if (GetHostFrameSinkManager()->IsFrameSinkIdRegistered(frame_sink_id_)) {
    bool result = GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
        frame_sink_id_, frame_sink_id);
    DCHECK(result);
  } else {
    pending_child_frame_sink_ids_.insert(frame_sink_id);
  }
}

void CompositorImpl::RemoveChildFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = pending_child_frame_sink_ids_.find(frame_sink_id);
  if (it != pending_child_frame_sink_ids_.end()) {
    pending_child_frame_sink_ids_.erase(it);
    return;
  }
  GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                          frame_sink_id);
}

void CompositorImpl::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  if (display.id() != display::Screen::GetScreen()
                          ->GetDisplayNearestWindow(root_window_)
                          .id()) {
    return;
  }

  if (changed_metrics & display::DisplayObserver::DisplayMetric::
                            DISPLAY_METRIC_DEVICE_SCALE_FACTOR) {
    // TODO(ccameron): This is transiently incorrect -- |size_| must be
    // recalculated here as well. Is the call in SetWindowBounds sufficient?
    host_->SetViewportRectAndScale(gfx::Rect(size_),
                                   root_window_->GetDipScale(),
                                   GenerateLocalSurfaceId());
  }

  if (changed_metrics &
      display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_ROTATION) {
    host_->set_display_transform_hint(
        display::DisplayRotationToOverlayTransform(display.rotation()));
  }
}

bool CompositorImpl::IsDrawingFirstVisibleFrame() const {
  return !has_submitted_frame_since_became_visible_;
}

void CompositorImpl::SetVSyncPaused(bool paused) {
  if (vsync_paused_ == paused)
    return;

  vsync_paused_ = paused;
  if (display_private_)
    display_private_->SetVSyncPaused(paused);
}

void CompositorImpl::OnUpdateRefreshRate(float refresh_rate) {
  if (display_private_)
    display_private_->UpdateRefreshRate(refresh_rate);
}

void CompositorImpl::OnUpdateSupportedRefreshRates(
    const std::vector<float>& supported_refresh_rates) {
  if (display_private_)
    display_private_->SetSupportedRefreshRates(supported_refresh_rates);
}

void CompositorImpl::InitializeVizLayerTreeFrameSink(
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider) {
  DCHECK(root_window_);

  pending_frames_ = 0;
  gpu_capabilities_ = context_provider->ContextCapabilities();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  auto root_params = viz::mojom::RootCompositorFrameSinkParams::New();

  // Android requires swap size notifications.
  root_params->send_swap_size_notifications = true;

  // Create interfaces for a root CompositorFrameSink.
  mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink> sink_remote;
  root_params->compositor_frame_sink =
      sink_remote.InitWithNewEndpointAndPassReceiver();
  mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient> client_receiver =
      root_params->compositor_frame_sink_client
          .InitWithNewPipeAndPassReceiver();
  display_private_.reset();
  root_params->display_private =
      display_private_.BindNewEndpointAndPassReceiver();
  display_client_ = std::make_unique<AndroidHostDisplayClient>(this);
  root_params->display_client = display_client_->GetBoundRemote(task_runner);

  const auto& display_props =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);

  viz::RendererSettings renderer_settings;
  renderer_settings.partial_swap_enabled = true;
  renderer_settings.allow_antialiasing = false;
  renderer_settings.highp_threshold_min = 2048;
  renderer_settings.requires_alpha_channel = requires_alpha_channel_;
  renderer_settings.initial_screen_size = display_props.GetSizeInPixel();
  renderer_settings.use_skia_renderer = features::IsUsingSkiaRenderer();
  renderer_settings.color_space = display_color_spaces_.GetOutputColorSpace(
      gfx::ContentColorUsage::kHDR, requires_alpha_channel_);

  root_params->frame_sink_id = frame_sink_id_;
  root_params->widget = surface_handle_;
  root_params->gpu_compositing = true;
  root_params->renderer_settings = renderer_settings;
  root_params->refresh_rate = root_window_->GetRefreshRate();

  GetHostFrameSinkManager()->CreateRootCompositorFrameSink(
      std::move(root_params));

  // Create LayerTreeFrameSink with the browser end of CompositorFrameSink.
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = task_runner;
  params.gpu_memory_buffer_manager =
      BrowserGpuChannelHostFactory::instance()->GetGpuMemoryBufferManager();
  params.pipes.compositor_frame_sink_associated_remote = std::move(sink_remote);
  params.pipes.client_receiver = std::move(client_receiver);
  params.client_name = kBrowser;
  auto layer_tree_frame_sink =
      std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
          std::move(context_provider), nullptr, &params);
  host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
  display_private_->SetDisplayVisible(true);
  display_private_->Resize(size_);
  display_private_->SetDisplayColorSpaces(display_color_spaces_);
  display_private_->SetVSyncPaused(vsync_paused_);
  display_private_->SetSupportedRefreshRates(
      root_window_->GetSupportedRefreshRates());
}

viz::LocalSurfaceIdAllocation CompositorImpl::GenerateLocalSurfaceId() {
  local_surface_id_allocator_.GenerateId();
  return local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
}

void CompositorImpl::OnContextCreationResult(
    gpu::ContextResult context_result) {
  if (!gpu::IsFatalOrSurfaceFailure(context_result)) {
    num_of_consecutive_surface_failures_ = 0u;
    return;
  }

  OnFatalOrSurfaceContextCreationFailure(context_result);
}

void CompositorImpl::OnFatalOrSurfaceContextCreationFailure(
    gpu::ContextResult context_result) {
  DCHECK(gpu::IsFatalOrSurfaceFailure(context_result));
  LOG_IF(FATAL, context_result == gpu::ContextResult::kFatalFailure)
      << "Fatal error making Gpu context";

  constexpr size_t kMaxConsecutiveSurfaceFailures = 10u;
  if (++num_of_consecutive_surface_failures_ > kMaxConsecutiveSurfaceFailures)
    FatalSurfaceFailure();

  if (context_result == gpu::ContextResult::kSurfaceFailure) {
    SetSurface(nullptr, false);
    client_->RecreateSurface();
  }
}

void CompositorImpl::OnFirstSurfaceActivation(const viz::SurfaceInfo& info) {
  NOTREACHED();
}

void CompositorImpl::CacheBackBufferForCurrentSurface() {
  if (window_ && display_private_) {
    cached_back_buffer_ =
        std::make_unique<ScopedCachedBackBuffer>(frame_sink_id_);
  }
}

void CompositorImpl::EvictCachedBackBuffer() {
  cached_back_buffer_.reset();
}

void CompositorImpl::RequestPresentationTimeForNextFrame(
    PresentationTimeCallback callback) {
  host_->RequestPresentationTimeForNextFrame(std::move(callback));
}

void CompositorImpl::DecrementPendingReadbacks() {
  DCHECK_GT(pending_readbacks_, 0u);
  --pending_readbacks_;
}

}  // namespace content
