// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/histograms.h"
#include "cc/base/switches.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/ukm_manager.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/metrics/public/mojom/single_sample_metrics.mojom.h"
#include "components/metrics/single_sample_metrics.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/switches.h"
#include "content/child/runtime_features.h"
#include "content/common/buildflags.h"
#include "content/common/content_constants_internal.h"
#include "content/common/partition_alloc_support.h"
#include "content/common/process_visibility_tracker.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/agent_scheduling_group.h"
#include "content/renderer/browser_exposed_renderer_interfaces.h"
#include "content/renderer/categorized_worker_pool.h"
#include "content/renderer/effective_connection_type_helper.h"
#include "content/renderer/media/gpu/gpu_video_accelerator_factories_impl.h"
#include "content/renderer/media/media_factory.h"
#include "content/renderer/media/media_interface_factory.h"
#include "content/renderer/media/render_media_client.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/variations_render_thread_observer.h"
#include "content/renderer/worker/embedded_shared_worker_stub.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "content/services/shared_storage_worklet/shared_storage_worklet_service_impl.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "gin/public/debug.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/decoder_factory.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_image_generator.h"
#include "third_party/blink/public/platform/web_memory_pressure_listener.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_scoped_page_pauser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_render_theme.h"
#include "third_party/blink/public/web/web_script_controller.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/display/display_switches.h"
#include "v8/include/v8-extension.h"

#if BUILDFLAG(IS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/media/android/stream_texture_factory.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "content/renderer/theme_helper_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <objbase.h>
#include <windows.h>
#include "content/renderer/media/win/dcomp_texture_factory.h"
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#if defined(ENABLE_IPC_FUZZER)
#include "content/common/external_ipc_dumper.h"
#include "mojo/public/cpp/bindings/message_dumper.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

namespace content {

namespace {

using ::base::PassKey;
using ::blink::WebDocument;
using ::blink::WebFrame;
using ::blink::WebNetworkStateNotifier;
using ::blink::WebRuntimeFeatures;
using ::blink::WebScriptController;
using ::blink::WebSecurityPolicy;
using ::blink::WebString;
using ::blink::WebView;

// An implementation of mojom::RenderMessageFilter which can be mocked out
// for tests which may indirectly send messages over this interface.
mojom::RenderMessageFilter* g_render_message_filter_for_testing;

// An implementation of RendererBlinkPlatformImpl which can be mocked out
// for tests.
RendererBlinkPlatformImpl* g_current_blink_platform_impl_for_testing;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl>>::DestructorAtExit
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner>>::
    DestructorAtExit g_main_task_runner = LAZY_INSTANCE_INITIALIZER;

// v8::MemoryPressureLevel should correspond to base::MemoryPressureListener.
static_assert(static_cast<v8::MemoryPressureLevel>(
                  base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) ==
                  v8::MemoryPressureLevel::kNone,
              "none level not align");
static_assert(
    static_cast<v8::MemoryPressureLevel>(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) ==
        v8::MemoryPressureLevel::kModerate,
    "moderate level not align");
static_assert(
    static_cast<v8::MemoryPressureLevel>(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) ==
        v8::MemoryPressureLevel::kCritical,
    "critical level not align");

void AddCrashKey(v8::CrashKeyId id, const std::string& value) {
  using base::debug::AllocateCrashKeyString;
  using base::debug::CrashKeySize;
  using base::debug::SetCrashKeyString;

  switch (id) {
    case v8::CrashKeyId::kIsolateAddress:
      static auto* const isolate_address =
          AllocateCrashKeyString("v8_isolate_address", CrashKeySize::Size32);
      SetCrashKeyString(isolate_address, value);
      break;
    case v8::CrashKeyId::kReadonlySpaceFirstPageAddress:
      static auto* const ro_space_firstpage_address = AllocateCrashKeyString(
          "v8_ro_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(ro_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kMapSpaceFirstPageAddress:
      static auto* const map_space_firstpage_address = AllocateCrashKeyString(
          "v8_map_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(map_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kCodeSpaceFirstPageAddress:
      static auto* const code_space_firstpage_address = AllocateCrashKeyString(
          "v8_code_space_firstpage_address", CrashKeySize::Size32);
      SetCrashKeyString(code_space_firstpage_address, value);
      break;
    case v8::CrashKeyId::kDumpType:
      static auto* const dump_type =
          AllocateCrashKeyString("dump-type", CrashKeySize::Size32);
      SetCrashKeyString(dump_type, value);
      break;
    default:
      // Doing nothing for new keys is a valid option. Having this case allows
      // to introduce new CrashKeyId's without triggering a build break.
      break;
  }
}

scoped_refptr<viz::ContextProviderCommandBuffer> CreateOffscreenContext(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const gpu::SharedMemoryLimits& limits,
    bool support_locking,
    bool support_gles2_interface,
    bool support_raster_interface,
    bool support_oop_rasterization,
    bool support_grcontext,
    bool automatic_flushes,
    viz::command_buffer_metrics::ContextType type,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority) {
  DCHECK(gpu_channel_host);
  // This is used to create a few different offscreen contexts:
  // - The shared main thread context, used by blink for 2D Canvas.
  // - The compositor worker context, used for GPU raster.
  // - The media context, used for accelerated video decoding.
  // This is for an offscreen context, so the default framebuffer doesn't need
  // alpha, depth, stencil, antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.enable_gles2_interface = support_gles2_interface;
  attributes.enable_raster_interface = support_raster_interface;
  attributes.enable_grcontext = support_grcontext;
  // Using RasterDecoder for OOP-R backend, so we need support_raster_interface
  // and !support_gles2_interface.
  attributes.enable_oop_rasterization = support_oop_rasterization &&
                                        support_raster_interface &&
                                        !support_gles2_interface;
  return base::MakeRefCounted<viz::ContextProviderCommandBuffer>(
      std::move(gpu_channel_host), gpu_memory_buffer_manager, stream_id,
      stream_priority, gpu::kNullSurfaceHandle,
      GURL("chrome://gpu/RenderThreadImpl::CreateOffscreenContext/" +
           viz::command_buffer_metrics::ContextTypeToString(type)),
      automatic_flushes, support_locking, support_grcontext, limits, attributes,
      type);
}

// Hook that allows single-sample metric code from //components/metrics to
// connect from the renderer process to the browser process.
void CreateSingleSampleMetricsProvider(
    mojo::SharedRemote<mojom::ChildProcessHost> process_host,
    mojo::PendingReceiver<metrics::mojom::SingleSampleMetricsProvider>
        receiver) {
  process_host->BindHostReceiver(std::move(receiver));
}

static bool IsSingleProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

// A thread for running shared storage worklet operations. It hosts a worklet
// environment belonging to one Document. The object owns itself, cleaning up
// when the worklet has shut down.
class SelfOwnedSharedStorageWorkletThread {
 public:
  SelfOwnedSharedStorageWorkletThread(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      mojo::PendingReceiver<
          shared_storage_worklet::mojom::SharedStorageWorkletService> receiver)
      : main_thread_runner_(std::move(main_thread_runner)) {
    DCHECK(main_thread_runner_->BelongsToCurrentThread());

    auto disconnect_handler = base::BindPostTask(
        main_thread_runner_,
        base::BindOnce(&SelfOwnedSharedStorageWorkletThread::
                           OnSharedStorageWorkletServiceDestroyed,
                       weak_factory_.GetWeakPtr()));

    auto task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);

    // Initialize the worklet service in a new thread.
    worklet_thread_ = base::SequenceBound<
        shared_storage_worklet::SharedStorageWorkletServiceImpl>(
        task_runner, std::move(receiver), std::move(disconnect_handler));
  }

 private:
  void OnSharedStorageWorkletServiceDestroyed() {
    DCHECK(main_thread_runner_->BelongsToCurrentThread());
    worklet_thread_.Reset();
    delete this;
  }

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  base::SequenceBound<shared_storage_worklet::SharedStorageWorkletServiceImpl>
      worklet_thread_;

  base::WeakPtrFactory<SelfOwnedSharedStorageWorkletThread> weak_factory_{this};
};

}  // namespace

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryExternalFragmentationTotal");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalUsed");
  custom_histograms_.insert("V8.MemoryHeapUsed");
  custom_histograms_.insert("V8.MemoryHeapCommitted");
}

RenderThreadImpl::HistogramCustomizer::~HistogramCustomizer() {}

void RenderThreadImpl::HistogramCustomizer::RenderViewNavigatedToHost(
    const std::string& host,
    size_t view_count) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHistogramCustomizer)) {
    return;
  }
  // Check if all RenderViews are displaying a page from the same host. If there
  // is only one RenderView, the common host is this view's host. If there are
  // many, check if this one shares the common host of the other
  // RenderViews. It's ok to not detect some cases where the RenderViews share a
  // common host. This information is only used for producing custom histograms.
  if (view_count == 1)
    SetCommonHost(host);
  else if (host != common_host_)
    SetCommonHost(std::string());
}

std::string RenderThreadImpl::HistogramCustomizer::ConvertToCustomHistogramName(
    const char* histogram_name) const {
  std::string name(histogram_name);
  if (!common_host_histogram_suffix_.empty() &&
      custom_histograms_.find(name) != custom_histograms_.end())
    name += common_host_histogram_suffix_;
  return name;
}

void RenderThreadImpl::HistogramCustomizer::SetCommonHost(
    const std::string& host) {
  if (host != common_host_) {
    common_host_ = host;
    common_host_histogram_suffix_ = HostToCustomHistogramSuffix(host);
  }
}

std::string RenderThreadImpl::HistogramCustomizer::HostToCustomHistogramSuffix(
    const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  if (host == "inbox.google.com")
    return ".inbox";
  if (host == "calendar.google.com")
    return ".calendar";
  if (host == "www.youtube.com")
    return ".youtube";
  if (IsAlexaTop10NonGoogleSite(host))
    return ".top10";

  return std::string();
}

bool RenderThreadImpl::HistogramCustomizer::IsAlexaTop10NonGoogleSite(
    const std::string& host) {
  // The Top10 sites have different TLD and/or subdomains depending on the
  // localization.
  if (host == "sina.com.cn")
    return true;

  std::string sanitized_host =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (sanitized_host == "facebook.com")
    return true;
  if (sanitized_host == "baidu.com")
    return true;
  if (sanitized_host == "qq.com")
    return true;
  if (sanitized_host == "twitter.com")
    return true;
  if (sanitized_host == "taobao.com")
    return true;
  if (sanitized_host == "live.com")
    return true;

  if (!sanitized_host.empty()) {
    std::vector<base::StringPiece> host_tokens = base::SplitStringPiece(
        sanitized_host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (host_tokens.size() >= 2) {
      if ((host_tokens[0] == "yahoo") || (host_tokens[0] == "amazon") ||
          (host_tokens[0] == "wikipedia")) {
        return true;
      }
    }
  }
  return false;
}

// static
RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

// static
mojom::RenderMessageFilter* RenderThreadImpl::current_render_message_filter() {
  if (g_render_message_filter_for_testing)
    return g_render_message_filter_for_testing;
  DCHECK(current());
  return current()->render_message_filter();
}

// static
RendererBlinkPlatformImpl* RenderThreadImpl::current_blink_platform_impl() {
  if (g_current_blink_platform_impl_for_testing)
    return g_current_blink_platform_impl_for_testing;
  DCHECK(current());
  return current()->blink_platform_impl();
}

// static
void RenderThreadImpl::SetRenderMessageFilterForTesting(
    mojom::RenderMessageFilter* render_message_filter) {
  g_render_message_filter_for_testing = render_message_filter;
}

// static
void RenderThreadImpl::SetRendererBlinkPlatformImplForTesting(
    RendererBlinkPlatformImpl* blink_platform_impl) {
  g_current_blink_platform_impl_for_testing = blink_platform_impl;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::DeprecatedGetMainTaskRunner() {
  return g_main_task_runner.Get();
}

// In single-process mode used for debugging, we don't pass a renderer client
// ID via command line because RenderThreadImpl lives in the same process as
// the browser
RenderThreadImpl::RenderThreadImpl(
    const InProcessChildThreadParams& params,
    int32_t client_id,
    std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler)
    : ChildThreadImpl(
          base::DoNothing(),
          Options::Builder()
              .InBrowserProcess(params)
              .ConnectToBrowser(true)
              .IPCTaskRunner(scheduler->DeprecatedDefaultTaskRunner())
              .ExposesInterfacesToBrowser()
              .Build()),
      main_thread_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      client_id_(client_id) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  Init();
}

namespace {
int32_t GetClientIdFromCommandLine() {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRendererClientId));
  int32_t client_id;
  base::StringToInt(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        switches::kRendererClientId),
                    &client_id);
  return client_id;
}
}  // anonymous namespace

// Multi-process mode.
RenderThreadImpl::RenderThreadImpl(
    base::RepeatingClosure quit_closure,
    std::unique_ptr<blink::scheduler::WebThreadScheduler> scheduler)
    : ChildThreadImpl(
          std::move(quit_closure),
          Options::Builder()
              .ConnectToBrowser(true)
              .IPCTaskRunner(scheduler->DeprecatedDefaultTaskRunner())
              .ExposesInterfacesToBrowser()
              .Build()),
      main_thread_scheduler_(std::move(scheduler)),
      categorized_worker_pool_(new CategorizedWorkerPool()),
      client_id_(GetClientIdFromCommandLine()) {
  TRACE_EVENT0("startup", "RenderThreadImpl::Create");
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT0("startup", "RenderThreadImpl::Init");

  SCOPED_UMA_HISTOGRAM_TIMER("Renderer.RenderThreadImpl.Init");

  GetContentClient()->renderer()->PostIOThreadCreated(GetIOTaskRunner().get());

  base::trace_event::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  // On Mac and Android Java UI, the select popups are rendered by the browser.
#if BUILDFLAG(IS_MAC)
  // When UseCommonSelectPopup is enabled, the internal popup menu should be
  // used.
  if (!features::IsUseCommonSelectPopupEnabled())
#endif
    blink::WebView::SetUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);
  g_main_task_runner.Get() = base::ThreadTaskRunnerHandle::Get();

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  metrics::InitializeSingleSampleMetricsFactory(base::BindRepeating(
      &CreateSingleSampleMetricsProvider, child_process_host()));

  mojo::PendingRemote<viz::mojom::Gpu> remote_gpu;
  BindHostReceiver(remote_gpu.InitWithNewPipeAndPassReceiver());
  gpu_ = viz::Gpu::Create(std::move(remote_gpu), GetIOTaskRunner());

  // Establish the GPU channel now, so its ready when needed and we don't have
  // to wait on a sync call.
  if (base::FeatureList::IsEnabled(features::kEarlyEstablishGpuChannel)) {
    gpu_->EstablishGpuChannel(
        base::BindOnce([](scoped_refptr<gpu::GpuChannelHost> host) {
          if (!host)
            return;
          GetContentClient()->SetGpuInfo(host->gpu_info());
          const bool create_compositor_worker_context =
              base::GetFieldTrialParamByFeatureAsBool(
                  features::kEarlyEstablishGpuChannel,
                  "CreateCompositorWorkerContext", false);
          if (create_compositor_worker_context) {
            // Similarly, establish the SharedCompositorWorkerContextProvider as
            // it involves a sync call. PostTask() is used as this may be called
            // from within SharedCompositorWorkerContextProvider(), in which
            // case we don't want to trigger reeentrancy.
            g_main_task_runner.Get()->PostTask(
                FROM_HERE, base::BindOnce([] {
                  RenderThreadImpl::current()
                      ->SharedCompositorWorkerContextProvider();
                }));
          }
        }));
  }

  // NOTE: Do not add interfaces to |binders| within this method. Instead,
  // modify the definition of |ExposeRendererInterfacesToBrowser()| to ensure
  // security review coverage.
  mojo::BinderMap binders;
  InitializeWebKit(&binders);

  vc_manager_ = std::make_unique<blink::WebVideoCaptureImplManager>();

  GetContentClient()->renderer()->RenderThreadStarted();
  ExposeRendererInterfacesToBrowser(weak_factory_.GetWeakPtr(), &binders);
  ExposeInterfacesToBrowser(std::move(binders));

  url_loader_throttle_provider_ =
      GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
          blink::URLLoaderThrottleProviderType::kFrame);

  GetAssociatedInterfaceRegistry()->AddInterface(base::BindRepeating(
      &RenderThreadImpl::OnRendererInterfaceReceiver, base::Unretained(this)));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#if defined(ENABLE_IPC_FUZZER)
  if (command_line.HasSwitch(switches::kIpcDumpDirectory)) {
    base::FilePath dump_directory =
        command_line.GetSwitchValuePath(switches::kIpcDumpDirectory);
    IPC::ChannelProxy::OutgoingMessageFilter* filter =
        LoadExternalIPCDumper(dump_directory);
    GetChannel()->set_outgoing_message_filter(filter);
    mojo::MessageDumper::SetMessageDumpDirectory(dump_directory);
  }
#endif

  cc::SetClientNameForMetrics("Renderer");

  is_threaded_animation_enabled_ =
      !command_line.HasSwitch(cc::switches::kDisableThreadedAnimation);

  is_elastic_overscroll_enabled_ = switches::IsElasticOverscrollEnabled();

  is_zoom_for_dsf_enabled_ = content::IsUseZoomForDSFEnabled();

  if (command_line.HasSwitch(switches::kDisableLCDText)) {
    is_lcd_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableLCDText)) {
    is_lcd_text_enabled_ = true;
  } else {
#if BUILDFLAG(IS_ANDROID)
    is_lcd_text_enabled_ = false;
#elif BUILDFLAG(IS_MAC)
    is_lcd_text_enabled_ = IsSubpixelAntialiasingAvailable();
#else
    is_lcd_text_enabled_ = true;
#endif
  }

  if (command_line.HasSwitch(switches::kDisableGpuCompositing))
    is_gpu_compositing_disabled_ = true;

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  media::InitializeMediaLibrary();

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&RenderThreadImpl::OnMemoryPressure,
                          base::Unretained(this)),
      base::BindRepeating(&RenderThreadImpl::OnSyncMemoryPressure,
                          base::Unretained(this)));

  int num_raster_threads = 0;
  std::string string_value =
      command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
  bool parsed_num_raster_threads =
      base::StringToInt(string_value, &num_raster_threads);
  DCHECK(parsed_num_raster_threads) << string_value;
  DCHECK_GT(num_raster_threads, 0);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  categorized_worker_pool_->SetBackgroundingCallback(
      main_thread_scheduler_->DefaultTaskRunner(),
      base::BindOnce(
          [](base::WeakPtr<RenderThreadImpl> render_thread,
             base::PlatformThreadId thread_id) {
            if (!render_thread)
              return;
            render_thread->render_message_filter()->SetThreadPriority(
                thread_id, base::ThreadPriority::BACKGROUND);
          },
          weak_factory_.GetWeakPtr()));
#endif
  categorized_worker_pool_->Start(num_raster_threads);

  discardable_memory_allocator_ = CreateDiscardableMemoryAllocator();

  // TODO(boliu): In single process, browser main loop should set up the
  // discardable memory manager, and should skip this if kSingleProcess.
  // See crbug.com/503724.
  base::DiscardableMemoryAllocator::SetInstance(
      discardable_memory_allocator_.get());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          blink::features::kBlinkCompositorUseDisplayThreadPriority)) {
    render_message_filter()->SetThreadPriority(
        ChildProcess::current()->io_thread_id(), base::ThreadPriority::DISPLAY);
  }
#endif

  process_foregrounded_count_ = 0;

  if (!is_gpu_compositing_disabled_) {
    BindHostReceiver(compositing_mode_reporter_.BindNewPipeAndPassReceiver());

    compositing_mode_reporter_->AddCompositingModeWatcher(
        compositing_mode_watcher_receiver_.BindNewPipeAndPassRemote(
            main_thread_scheduler_->CompositorTaskRunner()));
  }

  variations_observer_ = std::make_unique<VariationsRenderThreadObserver>();
  AddObserver(variations_observer_.get());

  if (base::FeatureList::IsEnabled(features::kFontManagerEarlyInit)) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindOnce([] { SkFontMgr::RefDefault(); }));
  }
}

RenderThreadImpl::~RenderThreadImpl() {
  g_main_task_runner.Get() = nullptr;

  // Need to make sure this reference is removed on the correct task runner;
  if (video_frame_compositor_task_runner_ &&
      video_frame_compositor_context_provider_) {
    video_frame_compositor_task_runner_->ReleaseSoon(
        FROM_HERE, std::move(video_frame_compositor_context_provider_));
  }
}

void RenderThreadImpl::Shutdown() {
  ChildThreadImpl::Shutdown();
  // In a multi-process mode, we immediately exit the renderer.
  // Historically we had a graceful shutdown sequence here but it was
  // 1) a waste of performance and 2) a source of lots of complicated
  // crashes caused by shutdown ordering. Immediate exit eliminates
  // those problems.

  blink::LogStatsDuringShutdown();

  // In a single-process mode, we cannot call _exit(0) in Shutdown() because
  // it will exit the process before the browser side is ready to exit.
  if (!IsSingleProcess())
    base::Process::TerminateCurrentProcessImmediately(0);
}

bool RenderThreadImpl::ShouldBeDestroyed() {
  DCHECK(IsSingleProcess());
  // In a single-process mode, it is unsafe to destruct this renderer thread
  // because we haven't run the shutdown sequence. Hence we leak the render
  // thread.
  //
  // In this case, we also need to disable at-exit callbacks because some of
  // the at-exit callbacks are expected to run after the renderer thread
  // has been destructed.
  base::AtExitManager::DisableAllAtExitManagers();
  return false;
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

IPC::SyncMessageFilter* RenderThreadImpl::GetSyncMessageFilter() {
  return sync_message_filter();
}

void RenderThreadImpl::AddRoute(int32_t routing_id, IPC::Listener* listener) {
  ChildThreadImpl::GetRouter()->AddRoute(routing_id, listener);
}

void RenderThreadImpl::AttachTaskRunnerToRoute(
    int32_t routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  GetChannel()->AddListenerTaskRunner(routing_id, std::move(task_runner));
}

void RenderThreadImpl::RemoveRoute(int32_t routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
  GetChannel()->RemoveListenerTaskRunner(routing_id);
  pending_frames_.erase(routing_id);
}

mojom::RendererHost* RenderThreadImpl::GetRendererHost() {
  if (!renderer_host_) {
    DCHECK(GetChannel());
    GetChannel()->GetRemoteAssociatedInterface(&renderer_host_);
  }
  return renderer_host_.get();
}

int RenderThreadImpl::GenerateRoutingID() {
  int32_t routing_id = MSG_ROUTING_NONE;
  render_message_filter()->GenerateRoutingID(&routing_id);
  return routing_id;
}

bool RenderThreadImpl::GenerateFrameRoutingID(
    int32_t& routing_id,
    blink::LocalFrameToken& frame_token,
    base::UnguessableToken& devtools_frame_token) {
  return render_message_filter()->GenerateFrameRoutingID(
      &routing_id, &frame_token, &devtools_frame_token);
}

void RenderThreadImpl::AddFilter(IPC::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThreadImpl::RemoveFilter(IPC::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThreadImpl::AddObserver(RenderThreadObserver* observer) {
  observers_.AddObserver(observer);
  observer->RegisterMojoInterfaces(&associated_interfaces_);
}

void RenderThreadImpl::RemoveObserver(RenderThreadObserver* observer) {
  observer->UnregisterMojoInterfaces(&associated_interfaces_);
  observers_.RemoveObserver(observer);
}

void RenderThreadImpl::SetResourceRequestSenderDelegate(
    blink::WebResourceRequestSenderDelegate* delegate) {
  resource_request_sender_delegate_ = delegate;
}

void RenderThreadImpl::InitializeCompositorThread() {
  blink_platform_impl_->CreateAndSetCompositorThread();
  compositor_task_runner_ = blink_platform_impl_->CompositorThreadTaskRunner();

  compositor_task_runner_->PostTask(FROM_HERE,
                                    base::BindOnce(&base::DisallowBlocking));
  GetContentClient()->renderer()->PostCompositorThreadCreated(
      compositor_task_runner_.get());
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::CreateVideoFrameCompositorTaskRunner() {
  if (!video_frame_compositor_task_runner_) {
    // All of Chromium's GPU code must know which thread it's running on, and
    // be the same thread on which the rendering context was initialized. This
    // is why this must be a SingleThreadTaskRunner instead of a
    // SequencedTaskRunner.
    video_frame_compositor_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }

  return video_frame_compositor_task_runner_;
}

void RenderThreadImpl::CreateSharedStorageWorkletService(
    mojo::PendingReceiver<
        shared_storage_worklet::mojom::SharedStorageWorkletService> receiver) {
  new SelfOwnedSharedStorageWorkletThread(
      GetWebMainThreadScheduler()->DefaultTaskRunner(), std::move(receiver));
}

void RenderThreadImpl::InitializeWebKit(mojo::BinderMap* binders) {
  DCHECK(!blink_platform_impl_);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    gin::Debug::SetJitCodeEventHandler(vTune::GetVtuneCodeEventHandler());
#endif

  blink_platform_impl_ =
      std::make_unique<RendererBlinkPlatformImpl>(main_thread_scheduler_.get());
  // This, among other things, enables any feature marked "test" in
  // runtime_enabled_features. It is run before
  // SetRuntimeFeaturesDefaultsAndUpdateFromArgs() so that command line
  // arguments take precedence over (and can disable) "test" features.
  GetContentClient()
      ->renderer()
      ->SetRuntimeFeaturesDefaultsBeforeBlinkInitialization();
  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);

  blink::Initialize(blink_platform_impl_.get(), binders,
                    main_thread_scheduler_.get());

  v8::Isolate* isolate = blink::MainThreadIsolate();
  isolate->SetAddCrashKeyCallback(AddCrashKey);

  if (!command_line.HasSwitch(switches::kDisableThreadedCompositing))
    InitializeCompositorThread();

  RenderThreadImpl::RegisterSchemes();

  RenderMediaClient::Initialize();

  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    // If we do not track widget visibility, then assume conservatively that
    // the isolate is in background. This reduces memory usage.
    isolate->IsolateInBackgroundNotification();
  }

  // Hook up blink's codecs so skia can call them. Since only the renderer
  // processes should be doing image decoding, this is not done in the common
  // skia initialization code for the GPU.
  SkGraphics::SetImageGeneratorFromEncodedDataFactory(
      blink::WebImageGenerator::CreateAsSkImageGenerator);
}

void RenderThreadImpl::RegisterSchemes() {
  // chrome:
  WebString chrome_scheme(WebString::FromASCII(kChromeUIScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(chrome_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsWebUI(chrome_scheme);

  // chrome-untrusted:
  WebString chrome_untrusted_scheme(
      WebString::FromASCII(kChromeUIUntrustedScheme));
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      chrome_untrusted_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(
      chrome_untrusted_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsAllowingWasmEvalCSP(
      chrome_untrusted_scheme);

  if (base::FeatureList::IsEnabled(features::kWebUICodeCache)) {
    WebSecurityPolicy::RegisterURLSchemeAsCodeCacheWithHashing(chrome_scheme);
    WebSecurityPolicy::RegisterURLSchemeAsCodeCacheWithHashing(
        chrome_untrusted_scheme);
  }

  // devtools:
  WebString devtools_scheme(WebString::FromASCII(kChromeDevToolsScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(devtools_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsSupportingFetchAPI(devtools_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(
      devtools_scheme);

  // view-source:
  WebString view_source_scheme(WebString::FromASCII(kViewSourceScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(view_source_scheme);

  // chrome-error:
  WebString error_scheme(WebString::FromASCII(kChromeErrorScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(error_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsNotAllowingJavascriptURLs(error_scheme);
  WebSecurityPolicy::RegisterURLSchemeAsError(error_scheme);

  // googlechrome:
  WebString google_chrome_scheme(WebString::FromASCII(kGoogleChromeScheme));
  WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(google_chrome_scheme);
}

void RenderThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  GetRendererHost()->RecordUserMetricsAction(action.str_);
}

void RenderThreadImpl::RecordComputedAction(const std::string& action) {
  GetRendererHost()->RecordUserMetricsAction(action);
}

void RenderThreadImpl::RegisterExtension(
    std::unique_ptr<v8::Extension> extension) {
  WebScriptController::RegisterExtension(std::move(extension));
}

int RenderThreadImpl::PostTaskToAllWebWorkers(base::RepeatingClosure closure) {
  return WorkerThreadRegistry::Instance()->PostTaskToAllThreads(
      std::move(closure));
}

media::GpuVideoAcceleratorFactories* RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  if (!gpu_factories_.empty()) {
    if (!gpu_factories_.back()->CheckContextProviderLostOnMainThread())
      return gpu_factories_.back().get();

    GetMediaThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuVideoAcceleratorFactoriesImpl::DestroyContext,
                       base::Unretained(gpu_factories_.back().get())));
  }

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host)
    return nullptr;
  // Currently, VideoResourceUpdater can't convert hardware resources to
  // software resources in software compositing mode.  So, fall back to software
  // video decoding if gpu compositing is off.
  if (is_gpu_compositing_disabled_)
    return nullptr;
  // This context is only used to create textures and mailbox them, so
  // use lower limits than the default.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();
  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = false;
  bool support_oop_rasterization = false;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  scoped_refptr<viz::ContextProviderCommandBuffer> media_context_provider =
      CreateOffscreenContext(
          gpu_channel_host, GetGpuMemoryBufferManager(), limits,
          support_locking, support_gles2_interface, support_raster_interface,
          support_oop_rasterization, support_grcontext, automatic_flushes,
          viz::command_buffer_metrics::ContextType::MEDIA, kGpuStreamIdMedia,
          kGpuStreamPriorityMedia);

  const bool enable_video_decode_accelerator =

#if BUILDFLAG(IS_LINUX)
      base::FeatureList::IsEnabled(media::kVaapiVideoDecodeLinux) &&
#else
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode) &&
#endif  // BUILDFLAG(IS_LINUX)
      (gpu_channel_host->gpu_feature_info()
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] ==
       gpu::kGpuFeatureStatusEnabled);

  const bool enable_video_encode_accelerator =

#if BUILDFLAG(IS_LINUX)
      base::FeatureList::IsEnabled(media::kVaapiVideoEncodeLinux) &&
#else
      !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoEncode) &&
#endif  // BUILDFLAG(IS_LINUX)
      (gpu_channel_host->gpu_feature_info()
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE] ==
       gpu::kGpuFeatureStatusEnabled);

  const bool enable_gpu_memory_buffers =
      !is_gpu_compositing_disabled_ &&
#if !BUILDFLAG(IS_ANDROID)
      !cmd_line->HasSwitch(switches::kDisableGpuMemoryBufferVideoFrames);
#else
      cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames);
#endif
  const bool enable_media_stream_gpu_memory_buffers =
      enable_gpu_memory_buffers &&
      base::FeatureList::IsEnabled(
          features::kWebRtcUseGpuMemoryBufferVideoFrames);
  bool enable_video_gpu_memory_buffers = enable_gpu_memory_buffers;
#if BUILDFLAG(IS_WIN)
  enable_video_gpu_memory_buffers =
      enable_video_gpu_memory_buffers &&
      (cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames) ||
       gpu_channel_host->gpu_info().overlay_info.supports_overlays);
#endif  // BUILDFLAG(IS_WIN)

  mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory;
  BindHostReceiver(interface_factory.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
      vea_provider;
  gpu_->CreateVideoEncodeAcceleratorProvider(
      vea_provider.InitWithNewPipeAndPassReceiver());

  gpu_factories_.push_back(GpuVideoAcceleratorFactoriesImpl::Create(
      std::move(gpu_channel_host), base::ThreadTaskRunnerHandle::Get(),
      GetMediaThreadTaskRunner(), std::move(media_context_provider),
      enable_video_gpu_memory_buffers, enable_media_stream_gpu_memory_buffers,
      enable_video_decode_accelerator, enable_video_encode_accelerator,
      std::move(interface_factory), std::move(vea_provider)));
  gpu_factories_.back()->SetRenderingColorSpace(rendering_color_space_);
  return gpu_factories_.back().get();
}

media::DecoderFactory* RenderThreadImpl::GetMediaDecoderFactory() {
  DCHECK(IsMainThread());

  // Note that we don't reset this, ever. We instantiate it once and never reset
  // it, even if the gpu process restarts.
  if (media_decoder_factory_)
    return media_decoder_factory_.get();

  // MediaInterfaceFactory guarantees that the media::InterfaceFactory is
  // accessed from the current (main) thread.
  mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory;
  BindHostReceiver(interface_factory.InitWithNewPipeAndPassReceiver());
  media_interface_factory_ = std::make_unique<MediaInterfaceFactory>(
      base::ThreadTaskRunnerHandle::Get(), std::move(interface_factory));
  // TODO(liberato): Should destruction of `media_decoder_factory_` be posted
  // to the media thread?  I don't think it's needed, since it's owned by us
  // rather than tied to a particular frame.  Calls into it might happen on
  // the media thread, but we own the media thread.
  media_decoder_factory_ =
      MediaFactory::CreateDecoderFactory(media_interface_factory_.get());

  return media_decoder_factory_.get();
}

scoped_refptr<viz::RasterContextProvider>
RenderThreadImpl::GetVideoFrameCompositorContextProvider(
    scoped_refptr<viz::RasterContextProvider> unwanted_context_provider) {
  DCHECK(video_frame_compositor_task_runner_);
  if (video_frame_compositor_context_provider_ &&
      video_frame_compositor_context_provider_ != unwanted_context_provider) {
    return video_frame_compositor_context_provider_;
  }

  // Need to make sure these references are removed on the correct task runner;
  if (video_frame_compositor_context_provider_) {
    video_frame_compositor_task_runner_->ReleaseSoon(
        FROM_HERE, std::move(video_frame_compositor_context_provider_));
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
      EstablishGpuChannelSync();
  if (!gpu_channel_host)
    return nullptr;

  // This context is only used to create textures and mailbox them, so
  // use lower limits than the default.
  gpu::SharedMemoryLimits limits = gpu::SharedMemoryLimits::ForMailboxContext();

  bool support_locking = false;
  bool support_gles2_interface = true;
  bool support_raster_interface = true;
  bool support_oop_rasterization = false;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  video_frame_compositor_context_provider_ = CreateOffscreenContext(
      gpu_channel_host, GetGpuMemoryBufferManager(), limits, support_locking,
      support_gles2_interface, support_raster_interface,
      support_oop_rasterization, support_grcontext, automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDER_COMPOSITOR,
      kGpuStreamIdMedia, kGpuStreamPriorityMedia);
  return video_frame_compositor_context_provider_;
}

scoped_refptr<viz::ContextProviderCommandBuffer>
RenderThreadImpl::SharedMainThreadContextProvider() {
  DCHECK(IsMainThread());
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->RasterInterface()
              ->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
    return shared_main_thread_contexts_;

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_main_thread_contexts_ = nullptr;
    return nullptr;
  }

  bool support_locking = false;
  bool support_raster_interface = true;
  bool support_oop_rasterization =
      gpu_channel_host->gpu_feature_info()
          .status_values[gpu::GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;
  bool support_gles2_interface = false;
  bool support_grcontext = !support_oop_rasterization;
  // Enable automatic flushes to improve canvas throughput.
  // See https://crbug.com/880901
  bool automatic_flushes = true;
  shared_main_thread_contexts_ = CreateOffscreenContext(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
      gpu::SharedMemoryLimits(), support_locking, support_gles2_interface,
      support_raster_interface, support_oop_rasterization, support_grcontext,
      automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDERER_MAIN_THREAD,
      kGpuStreamIdDefault, kGpuStreamPriorityDefault);
  auto result = shared_main_thread_contexts_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess)
    shared_main_thread_contexts_ = nullptr;
  return shared_main_thread_contexts_;
}

#if BUILDFLAG(IS_ANDROID)
scoped_refptr<StreamTextureFactory> RenderThreadImpl::GetStreamTexureFactory() {
  DCHECK(IsMainThread());
  if (!stream_texture_factory_ || stream_texture_factory_->IsLost()) {
    scoped_refptr<gpu::GpuChannelHost> channel = EstablishGpuChannelSync();
    if (!channel) {
      stream_texture_factory_ = nullptr;
      return nullptr;
    }
    stream_texture_factory_ = StreamTextureFactory::Create(std::move(channel));
  }
  return stream_texture_factory_;
}

bool RenderThreadImpl::EnableStreamTextureCopy() {
  return GetContentClient()->UsingSynchronousCompositing();
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
scoped_refptr<DCOMPTextureFactory> RenderThreadImpl::GetDCOMPTextureFactory() {
  DCHECK(IsMainThread());
  if (!dcomp_texture_factory_.get() || dcomp_texture_factory_->IsLost()) {
    scoped_refptr<gpu::GpuChannelHost> channel = EstablishGpuChannelSync();
    if (!channel) {
      dcomp_texture_factory_ = nullptr;
      return nullptr;
    }
    dcomp_texture_factory_ = DCOMPTextureFactory::Create(
        std::move(channel), GetMediaThreadTaskRunner());
  }
  return dcomp_texture_factory_;
}
#endif  // BUILDFLAG(IS_WIN)

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

int32_t RenderThreadImpl::GetClientId() {
  return client_id_;
}

void RenderThreadImpl::SetRendererProcessType(
    blink::scheduler::WebRendererProcessType type) {
  main_thread_scheduler_->SetRendererProcessType(type);
}

blink::WebString RenderThreadImpl::GetUserAgent() {
  DCHECK(!user_agent_.IsNull());

  return user_agent_;
}

blink::WebString RenderThreadImpl::GetReducedUserAgent() {
  DCHECK(!reduced_user_agent_.IsNull());

  return reduced_user_agent_;
}

const blink::UserAgentMetadata& RenderThreadImpl::GetUserAgentMetadata() {
  return user_agent_metadata_;
}

bool RenderThreadImpl::IsUseZoomForDSF() {
  return is_zoom_for_dsf_enabled_;
}

void RenderThreadImpl::WriteIntoTrace(
    perfetto::TracedProto<perfetto::protos::pbzero::RenderProcessHost> proto) {
  int id = GetClientId();
  proto->set_id(id);
}

void RenderThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (!associated_interfaces_.TryBindInterface(name, &handle))
    ChildThreadImpl::OnAssociatedInterfaceRequest(name, std::move(handle));
}

bool RenderThreadImpl::IsLcdTextEnabled() {
  return is_lcd_text_enabled_;
}

bool RenderThreadImpl::IsElasticOverscrollEnabled() {
  return is_elastic_overscroll_enabled_;
}

gpu::GpuMemoryBufferManager* RenderThreadImpl::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
}

blink::scheduler::WebThreadScheduler*
RenderThreadImpl::GetWebMainThreadScheduler() {
  return main_thread_scheduler_.get();
}

cc::TaskGraphRunner* RenderThreadImpl::GetTaskGraphRunner() {
  return categorized_worker_pool_->GetTaskGraphRunner();
}

bool RenderThreadImpl::IsThreadedAnimationEnabled() {
  return is_threaded_animation_enabled_;
}

bool RenderThreadImpl::IsScrollAnimatorEnabled() {
  return is_scroll_animator_enabled_;
}

void RenderThreadImpl::SetScrollAnimatorEnabled(
    bool enable_scroll_animator,
    base::PassKey<AgentSchedulingGroup>) {
  is_scroll_animator_enabled_ = enable_scroll_animator;
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

void RenderThreadImpl::OnChannelError() {
  // In single-process mode, the renderer can't be restarted after shutdown.
  // So, if we get a channel error, crash the whole process right now to get a
  // more informative stack, since we will otherwise just crash later when we
  // try to restart it.
  CHECK(!IsSingleProcess());
  ChildThreadImpl::OnChannelError();
}

void RenderThreadImpl::OnProcessFinalRelease() {
  // Do not shutdown the process. The browser process is the only one
  // responsible for renderer shutdown.
  //
  // Renderer process used to request self shutdown. It has been removed. It
  // caused race conditions, where the browser process was reusing renderer
  // processes that were shutting down.
  // See https://crbug.com/535246 or https://crbug.com/873541/#c8.
  NOTREACHED();
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  for (auto& observer : observers_) {
    if (observer.OnControlMessageReceived(msg))
      return true;
  }

  return false;
}

void RenderThreadImpl::SetProcessState(
    mojom::RenderProcessBackgroundState background_state,
    mojom::RenderProcessVisibleState visible_state) {
  DCHECK(background_state_ != background_state ||
         visible_state_ != visible_state);

  if (background_state != background_state_) {
    if (background_state == mojom::RenderProcessBackgroundState::kForegrounded)
      OnRendererForegrounded();
    else
      OnRendererBackgrounded();
  }

  if (visible_state != visible_state_) {
    bool is_visible =
        visible_state == mojom::RenderProcessVisibleState::kVisible;

    if (!IsInBrowserProcess()) {
      ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(
          is_visible);
    }

    if (is_visible)
      OnRendererVisible();
    else
      OnRendererHidden();
  }

  background_state_ = background_state;
  visible_state_ = visible_state;
}

void RenderThreadImpl::SetIsLockedToSite() {
  DCHECK(blink_platform_impl_);
  blink_platform_impl_->SetIsLockedToSite();
}

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void RenderThreadImpl::WriteClangProfilingProfile(
    WriteClangProfilingProfileCallback callback) {
  // This will write the profiling profile to the file that has been opened and
  // passed to this renderer by the browser.
  base::WriteClangProfilingProfile();
  std::move(callback).Run();
}
#endif

void RenderThreadImpl::SetIsCrossOriginIsolated(bool value) {
  blink::SetIsCrossOriginIsolated(value);
}

void RenderThreadImpl::SetIsDirectSocketEnabled(bool value) {
  blink::SetIsDirectSocketEnabled(value);
}

void RenderThreadImpl::EnableBlinkRuntimeFeatures(
    const std::vector<std::string>& features) {
  for (const auto& feature : features) {
    blink::WebRuntimeFeatures::EnableFeatureFromString(feature, true);
  }
}

bool RenderThreadImpl::GetRendererMemoryMetrics(
    RendererMemoryMetrics* memory_metrics) const {
  DCHECK(memory_metrics);

  // Cache this result, as it can change while this code is running, and is used
  // as a divisor below.
  size_t web_view_count = blink::WebView::GetWebViewCount();

  // If there are no web views it doesn't make sense to calculate metrics
  // right now.
  if (web_view_count == 0)
    return false;

  blink::WebMemoryStatistics blink_stats = blink::WebMemoryStatistics::Get();
  memory_metrics->partition_alloc_kb =
      blink_stats.partition_alloc_total_allocated_bytes / 1024;
  memory_metrics->blink_gc_kb =
      blink_stats.blink_gc_total_allocated_bytes / 1024;
  std::unique_ptr<base::ProcessMetrics> metric(
      base::ProcessMetrics::CreateCurrentProcessMetrics());
  size_t malloc_usage = metric->GetMallocUsage();
  memory_metrics->malloc_mb = malloc_usage / 1024 / 1024;

  size_t discardable_usage = discardable_memory_allocator_->GetBytesAllocated();
  memory_metrics->discardable_kb = discardable_usage / 1024;

  size_t v8_usage = 0;
  if (v8::Isolate* isolate = blink::MainThreadIsolate()) {
    v8::HeapStatistics v8_heap_statistics;
    isolate->GetHeapStatistics(&v8_heap_statistics);
    v8_usage = v8_heap_statistics.total_heap_size();
  }
  // TODO(tasak): Currently only memory usage of mainThreadIsolate() is
  // reported. We should collect memory usages of all isolates using
  // memory-infra.
  memory_metrics->v8_main_thread_isolate_mb = v8_usage / 1024 / 1024;
  size_t total_allocated = blink_stats.partition_alloc_total_allocated_bytes +
                           blink_stats.blink_gc_total_allocated_bytes +
                           malloc_usage + v8_usage + discardable_usage;
  memory_metrics->total_allocated_mb = total_allocated / 1024 / 1024;
  memory_metrics->non_discardable_total_allocated_mb =
      (total_allocated - discardable_usage) / 1024 / 1024;
  memory_metrics->total_allocated_per_render_view_mb =
      total_allocated / web_view_count / 1024 / 1024;

  return true;
}

static void RecordMemoryUsageAfterBackgroundedMB(const char* basename,
                                                 const char* suffix,
                                                 int memory_usage) {
  std::string histogram_name = base::StringPrintf("%s.%s", basename, suffix);
  base::UmaHistogramMemoryLargeMB(histogram_name, memory_usage);
}

void RenderThreadImpl::RecordMemoryUsageAfterBackgrounded(
    const char* suffix,
    int foregrounded_count) {
  // If this renderer is resumed, we should not update UMA.
  if (!RendererIsHidden())
    return;
  // If this renderer was not kept backgrounded for 5/10/15 minutes,
  // we should not record current memory usage.
  if (foregrounded_count != process_foregrounded_count_)
    return;

  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.PartitionAlloc.AfterBackgrounded", suffix,
      memory_metrics.partition_alloc_kb / 1024);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.BlinkGC.AfterBackgrounded", suffix,
      memory_metrics.blink_gc_kb / 1024);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.Malloc.AfterBackgrounded", suffix,
      memory_metrics.malloc_mb);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.Discardable.AfterBackgrounded", suffix,
      memory_metrics.discardable_kb / 1024);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.V8MainThreaIsolate.AfterBackgrounded",
      suffix, memory_metrics.v8_main_thread_isolate_mb);
  RecordMemoryUsageAfterBackgroundedMB(
      "Memory.Experimental.Renderer.TotalAllocated.AfterBackgrounded", suffix,
      memory_metrics.total_allocated_mb);
}

#define GET_MEMORY_GROWTH(current, previous, allocator) \
  (current.allocator > previous.allocator               \
       ? current.allocator - previous.allocator         \
       : 0)

static void RecordBackgroundedRenderPurgeMemoryGrowthKB(const char* basename,
                                                        const char* suffix,
                                                        int memory_usage) {
  std::string histogram_name = base::StringPrintf("%s.%s", basename, suffix);
  base::UmaHistogramMemoryKB(histogram_name, memory_usage);
}

void RenderThreadImpl::OnRecordMetricsForBackgroundedRendererPurgeTimerExpired(
    const char* suffix,
    int foregrounded_count_when_purged) {
  // If this renderer is resumed, we should not update UMA.
  if (!RendererIsHidden())
    return;
  if (foregrounded_count_when_purged != process_foregrounded_count_)
    return;

  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;

  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.PartitionAllocKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        partition_alloc_kb));
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.BlinkGCKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        blink_gc_kb));
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.MallocKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        malloc_mb) *
          1024);
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.DiscardableKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        discardable_kb));
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.V8MainThreadIsolateKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        v8_main_thread_isolate_mb) *
          1024);
  RecordBackgroundedRenderPurgeMemoryGrowthKB(
      "PurgeAndSuspend.Experimental.MemoryGrowth.TotalAllocatedKB", suffix,
      GET_MEMORY_GROWTH(memory_metrics, purge_and_suspend_memory_metrics_,
                        total_allocated_mb) *
          1024);
}

void RenderThreadImpl::RecordMetricsForBackgroundedRendererPurge() {
  RendererMemoryMetrics memory_metrics;
  if (!GetRendererMemoryMetrics(&memory_metrics))
    return;

  purge_and_suspend_memory_metrics_ = memory_metrics;
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::
              OnRecordMetricsForBackgroundedRendererPurgeTimerExpired,
          base::Unretained(this), "30min", process_foregrounded_count_),
      base::Minutes(30));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::
              OnRecordMetricsForBackgroundedRendererPurgeTimerExpired,
          base::Unretained(this), "60min", process_foregrounded_count_),
      base::Minutes(60));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RenderThreadImpl::
              OnRecordMetricsForBackgroundedRendererPurgeTimerExpired,
          base::Unretained(this), "90min", process_foregrounded_count_),
      base::Minutes(90));
}

void RenderThreadImpl::CompositingModeFallbackToSoftware() {
  gpu_->LoseChannel();
  is_gpu_compositing_disabled_ = true;
}

scoped_refptr<gpu::GpuChannelHost> RenderThreadImpl::EstablishGpuChannelSync() {
  TRACE_EVENT0("gpu", "RenderThreadImpl::EstablishGpuChannelSync");

  scoped_refptr<gpu::GpuChannelHost> gpu_channel =
      gpu_->EstablishGpuChannelSync();
  if (gpu_channel)
    GetContentClient()->SetGpuInfo(gpu_channel->gpu_info());
  return gpu_channel;
}

blink::AssociatedInterfaceRegistry*
RenderThreadImpl::GetAssociatedInterfaceRegistry() {
  return &associated_interfaces_;
}

mojom::RenderMessageFilter* RenderThreadImpl::render_message_filter() {
  if (!render_message_filter_)
    GetChannel()->GetRemoteAssociatedInterface(&render_message_filter_);
  return render_message_filter_.get();
}

gpu::GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  return gpu_->GetGpuChannel().get();
}

base::PlatformThreadId RenderThreadImpl::GetIOPlatformThreadId() const {
  return ChildProcess::current()->io_thread_id();
}

void RenderThreadImpl::CreateAgentSchedulingGroup(
    mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> bootstrap,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote) {
  agent_scheduling_groups_.emplace(std::make_unique<AgentSchedulingGroup>(
      *this, std::move(bootstrap), std::move(broker_remote)));
}

void RenderThreadImpl::CreateAssociatedAgentSchedulingGroup(
    mojo::PendingAssociatedReceiver<mojom::AgentSchedulingGroup>
        agent_scheduling_group,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote) {
  agent_scheduling_groups_.emplace(std::make_unique<AgentSchedulingGroup>(
      *this, std::move(agent_scheduling_group), std::move(broker_remote)));
}

void RenderThreadImpl::OnNetworkConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type,
    double max_bandwidth_mbps) {
  bool online_status = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::SetOnLine(online_status);
  WebNetworkStateNotifier::SetWebConnection(
      NetConnectionTypeToWebConnectionType(type), max_bandwidth_mbps);
  if (url_loader_throttle_provider_)
    url_loader_throttle_provider_->SetOnline(online_status);
}

void RenderThreadImpl::OnNetworkQualityChanged(
    net::EffectiveConnectionType type,
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    double downlink_throughput_kbps) {
  LOCAL_HISTOGRAM_BOOLEAN("NQE.RenderThreadNotified", true);
  WebNetworkStateNotifier::SetNetworkQuality(
      EffectiveConnectionTypeToWebEffectiveConnectionType(type), http_rtt,
      transport_rtt, downlink_throughput_kbps);
}

void RenderThreadImpl::SetWebKitSharedTimersSuspended(bool suspend) {
#if BUILDFLAG(IS_ANDROID)
  if (suspend) {
    main_thread_scheduler_->PauseTimersForAndroidWebView();
  } else {
    main_thread_scheduler_->ResumeTimersForAndroidWebView();
  }
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::SetUserAgent(const std::string& user_agent) {
  DCHECK(user_agent_.IsNull());
  user_agent_ = WebString::FromUTF8(user_agent);
  GetContentClient()->renderer()->DidSetUserAgent(user_agent);
}

void RenderThreadImpl::SetReducedUserAgent(const std::string& user_agent) {
  DCHECK(reduced_user_agent_.IsNull());
  reduced_user_agent_ = WebString::FromUTF8(user_agent);
}

void RenderThreadImpl::SetUserAgentMetadata(
    const blink::UserAgentMetadata& user_agent_metadata) {
  user_agent_metadata_ = user_agent_metadata;
}

void RenderThreadImpl::SetCorsExemptHeaderList(
    const std::vector<std::string>& list) {
  cors_exempt_header_list_ = list;
}

void RenderThreadImpl::UpdateScrollbarTheme(
    mojom::UpdateScrollbarThemeParamsPtr params) {
#if BUILDFLAG(IS_MAC)
  blink::WebScrollbarTheme::UpdateScrollbarsWithNSDefaults(
      params->has_initial_button_delay
          ? absl::make_optional(params->initial_button_delay)
          : absl::nullopt,
      params->has_autoscroll_button_delay
          ? absl::make_optional(params->autoscroll_button_delay)
          : absl::nullopt,
      params->preferred_scroller_style, params->redraw,
      params->jump_on_track_click);

  is_elastic_overscroll_enabled_ = params->scroll_view_rubber_banding;
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::OnSystemColorsChanged(
    int32_t aqua_color_variant,
    const std::string& highlight_text_color,
    const std::string& highlight_color) {
#if BUILDFLAG(IS_MAC)
  if (!IsSingleProcess()) {
    // The purpose of this function is to send an IPC to the renderer notifying
    // it that the browser received an NSNotificationCenter notification, which
    // the renderer needs to repost in its own process so that AppKit-side state
    // in its process can be updated. This makes no sense in single-process
    // mode, since that state is already updated, and in fact is actively
    // harmful: that IPC is received on a different thread, then the
    // notification is reposted (again) from a different thread.
    // See https://crbug.com/1162066
    SystemColorsDidChange(aqua_color_variant, highlight_text_color,
                          highlight_color);
  }
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::UpdateSystemColorInfo(
    mojom::UpdateSystemColorInfoParamsPtr params) {
  if (blink_platform_impl_->ThemeEngine()->UpdateColorProviders(
          params->light_colors, params->dark_colors)) {
    // Notify blink that the global ColorProvider instances for this renderer
    // have changed. These color providers are only used to paint native
    // controls and only require us to invalidate paint for local frames in this
    // renderer.
    blink::ColorProvidersChanged();
  }

  bool did_system_color_info_change =
      ui::NativeTheme::GetInstanceForWeb()->UpdateSystemColorInfo(
          params->is_dark_mode, params->forced_colors, params->colors);

  if (did_system_color_info_change) {
    // Notify blink of system color info changes. These give blink the
    // opportunity to update internal state to reflect the NativeTheme's color
    // scheme. These will also prompt blink to invalidate and recalculate styles
    // for elements that rely on system colors, such as those leveraging the
    // forced colors media feature. These can affect CSS styles and thus require
    // action beyond simply invalidating paint on local frames.
    blink::SystemColorsChanged();
    blink::ColorSchemeChanged();
  }
}

void RenderThreadImpl::PurgePluginListCache(bool reload_pages) {
#if BUILDFLAG(ENABLE_PLUGINS)
  blink::ResetPluginCache(reload_pages);

  for (auto& observer : observers_)
    observer.PluginListChanged();
#else
  NOTREACHED();
#endif
}

void RenderThreadImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  TRACE_EVENT("memory", "RenderThreadImpl::OnMemoryPressure",
              [&](perfetto::EventContext ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                auto* data = event->set_chrome_memory_pressure_notification();
                data->set_level(
                    base::trace_event::MemoryPressureLevelToTraceEnum(
                        memory_pressure_level));
              });
  if (blink_platform_impl_)
    blink::WebMemoryPressureListener::OnMemoryPressure(memory_pressure_level);
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    ReleaseFreeMemory();
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetMediaThreadTaskRunner() {
  DCHECK(main_thread_runner()->BelongsToCurrentThread());
  if (!media_thread_) {
    media_thread_ = std::make_unique<base::Thread>("Media");
#if BUILDFLAG(IS_FUCHSIA)
    // Start IO thread on Fuchsia to make that thread usable for FIDL.
    base::Thread::Options options(base::MessagePumpType::IO, 0);
#else
    base::Thread::Options options;
#endif
    media_thread_->StartWithOptions(std::move(options));
  }
  return media_thread_->task_runner();
}

base::TaskRunner* RenderThreadImpl::GetWorkerTaskRunner() {
  return categorized_worker_pool_.get();
}

scoped_refptr<viz::RasterContextProvider>
RenderThreadImpl::SharedCompositorWorkerContextProvider() {
  DCHECK(IsMainThread());
  // Try to reuse existing shared worker context provider.
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        shared_worker_context_provider_.get());
    if (lock.RasterInterface()->GetGraphicsResetStatusKHR() == GL_NO_ERROR)
      return shared_worker_context_provider_;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    shared_worker_context_provider_ = nullptr;
    return shared_worker_context_provider_;
  }

  bool support_locking = true;
  bool support_gpu_rasterization =
      gpu_channel_host->gpu_feature_info()
          .status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] ==
      gpu::kGpuFeatureStatusEnabled;

  bool support_gles2_interface = false;
  bool support_raster_interface = true;
  bool support_grcontext = false;
  bool automatic_flushes = false;
  auto shared_memory_limits =
      support_gpu_rasterization ? gpu::SharedMemoryLimits::ForOOPRasterContext()
                                : gpu::SharedMemoryLimits();
  shared_worker_context_provider_ = CreateOffscreenContext(
      std::move(gpu_channel_host), GetGpuMemoryBufferManager(),
      shared_memory_limits, support_locking, support_gles2_interface,
      support_raster_interface, support_gpu_rasterization, support_grcontext,
      automatic_flushes,
      viz::command_buffer_metrics::ContextType::RENDER_WORKER,
      kGpuStreamIdWorker, kGpuStreamPriorityWorker);
  auto result = shared_worker_context_provider_->BindToCurrentThread();
  if (result != gpu::ContextResult::kSuccess) {
    shared_worker_context_provider_ = nullptr;
    return nullptr;
  }

  return shared_worker_context_provider_;
}

bool RenderThreadImpl::RendererIsHidden() const {
  return visible_state_ == mojom::RenderProcessVisibleState::kHidden;
}

void RenderThreadImpl::OnRendererHidden() {
  blink::MainThreadIsolate()->IsolateInBackgroundNotification();
  // TODO(rmcilroy): Remove IdleHandler and replace it with an IdleTask
  // scheduled by the RendererScheduler - http://crbug.com/469210.
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  main_thread_scheduler_->SetRendererHidden(true);
}

void RenderThreadImpl::OnRendererVisible() {
  blink::MainThreadIsolate()->IsolateInForegroundNotification();
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  main_thread_scheduler_->SetRendererHidden(false);
}

bool RenderThreadImpl::RendererIsBackgrounded() const {
  return background_state_ ==
         mojom::RenderProcessBackgroundState::kBackgrounded;
}

void RenderThreadImpl::OnRendererBackgrounded() {
  main_thread_scheduler_->SetRendererBackgrounded(true);
  discardable_memory_allocator_->OnBackgrounded();
  internal::PartitionAllocSupport::Get()->OnBackgrounded();

  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadImpl::RecordMemoryUsageAfterBackgrounded,
                     base::Unretained(this), "5min",
                     process_foregrounded_count_),
      base::Minutes(5));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadImpl::RecordMemoryUsageAfterBackgrounded,
                     base::Unretained(this), "10min",
                     process_foregrounded_count_),
      base::Minutes(10));
  GetWebMainThreadScheduler()->DefaultTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadImpl::RecordMemoryUsageAfterBackgrounded,
                     base::Unretained(this), "15min",
                     process_foregrounded_count_),
      base::Minutes(15));
}

void RenderThreadImpl::OnRendererForegrounded() {
  main_thread_scheduler_->SetRendererBackgrounded(false);
  discardable_memory_allocator_->OnForegrounded();
  internal::PartitionAllocSupport::Get()->OnForegrounded();
  process_foregrounded_count_++;
}

void RenderThreadImpl::ReleaseFreeMemory() {
  TRACE_EVENT0("blink", "RenderThreadImpl::ReleaseFreeMemory()");
  base::allocator::ReleaseFreeMemory();
  discardable_memory_allocator_->ReleaseFreeMemory();

  // Do not call into blink if it is not initialized.
  if (blink_platform_impl_) {
    // Purge Skia font cache, resource cache, and image filter.
    SkGraphics::PurgeAllCaches();
    blink::WebMemoryPressureListener::OnPurgeMemory();
  }
}

void RenderThreadImpl::OnSyncMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (!blink::MainThreadIsolate())
    return;

  v8::MemoryPressureLevel v8_memory_pressure_level =
      static_cast<v8::MemoryPressureLevel>(memory_pressure_level);

#if !BUILDFLAG(ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)
  // In order to reduce performance impact, translate critical level to
  // moderate level for foreground renderer.
  if (!RendererIsHidden() &&
      v8_memory_pressure_level == v8::MemoryPressureLevel::kCritical)
    v8_memory_pressure_level = v8::MemoryPressureLevel::kModerate;
#endif  // !BUILDFLAG(ALLOW_CRITICAL_MEMORY_PRESSURE_HANDLING_IN_FOREGROUND)

  blink::MainThreadIsolate()->MemoryPressureNotification(
      v8_memory_pressure_level);
  blink::MemoryPressureNotificationToWorkerThreadIsolates(
      v8_memory_pressure_level);
}

void RenderThreadImpl::OnRendererInterfaceReceiver(
    mojo::PendingAssociatedReceiver<mojom::Renderer> receiver) {
  DCHECK(!renderer_receiver_.is_bound());
  renderer_receiver_.Bind(
      std::move(receiver),
      GetWebMainThreadScheduler()->DeprecatedDefaultTaskRunner());
}

void RenderThreadImpl::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {
  DCHECK(IsMainThread());
  rendering_color_space_ = color_space;

  for (const auto& factories : gpu_factories_) {
    if (factories)
      factories->SetRenderingColorSpace(color_space);
  }
}

gfx::ColorSpace RenderThreadImpl::GetRenderingColorSpace() {
  DCHECK(IsMainThread());
  return rendering_color_space_;
}

}  // namespace content
