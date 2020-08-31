// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/common/child_process.mojom.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/latency/latency_info.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/window.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#include "ui/gfx/win/rendering_window_manager.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_switches.h"  // nogncheck
#endif

#if defined(OS_MACOSX) || defined(OS_ANDROID)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/gpu/ca_transaction_gpu_coordinator.h"
#endif

namespace content {

base::subtle::Atomic32 GpuProcessHost::gpu_crash_count_ = 0;
bool GpuProcessHost::crashed_before_ = false;
int GpuProcessHost::recent_crash_count_ = 0;
gpu::GpuMode GpuProcessHost::last_crash_mode_ = gpu::GpuMode::UNKNOWN;

namespace {

// UMA histogram names.
constexpr char kProcessLifetimeEventsHardwareAccelerated[] =
    "GPU.ProcessLifetimeEvents.HardwareAccelerated";
constexpr char kProcessLifetimeEventsSwiftShader[] =
    "GPU.ProcessLifetimeEvents.SwiftShader";
constexpr char kProcessLifetimeEventsDisplayCompositor[] =
    "GPU.ProcessLifetimeEvents.DisplayCompositor";

// Returns the UMA histogram name for the given GPU mode.
const char* GetProcessLifetimeUmaName(gpu::GpuMode gpu_mode) {
  switch (gpu_mode) {
    // TODO(sgilhuly): Add separate histograms for the different hardware modes.
    case gpu::GpuMode::HARDWARE_GL:
    case gpu::GpuMode::HARDWARE_METAL:
    case gpu::GpuMode::HARDWARE_VULKAN:
      return kProcessLifetimeEventsHardwareAccelerated;
    case gpu::GpuMode::SWIFTSHADER:
      return kProcessLifetimeEventsSwiftShader;
    case gpu::GpuMode::DISPLAY_COMPOSITOR:
      return kProcessLifetimeEventsDisplayCompositor;
    default:
      NOTREACHED();
      return nullptr;
  }
}

// Forgive one GPU process crash after this many minutes.
constexpr int kForgiveGpuCrashMinutes = 60;

// Forgive one GPU process crash, when the GPU process is launched to run only
// the display compositor, after this many minutes.
constexpr int kForgiveDisplayCompositorCrashMinutes = 10;

int GetForgiveMinutes(gpu::GpuMode gpu_mode) {
  return gpu_mode == gpu::GpuMode::DISPLAY_COMPOSITOR
             ? kForgiveDisplayCompositorCrashMinutes
             : kForgiveGpuCrashMinutes;
}

// This matches base::TerminationStatus.
// These values are persisted to logs. Entries (except MAX_ENUM) should not be
// renumbered and numeric values should never be reused. Should also avoid
// OS-defines in this enum to keep the values consistent on all platforms.
enum class GpuTerminationStatus {
  NORMAL_TERMINATION = 0,
  ABNORMAL_TERMINATION = 1,
  PROCESS_WAS_KILLED = 2,
  PROCESS_CRASHED = 3,
  STILL_RUNNING = 4,
  PROCESS_WAS_KILLED_BY_OOM = 5,
  OOM_PROTECTED = 6,
  LAUNCH_FAILED = 7,
  OOM = 8,
  MAX_ENUM = 9,
};

GpuTerminationStatus ConvertToGpuTerminationStatus(
    base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return GpuTerminationStatus::NORMAL_TERMINATION;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return GpuTerminationStatus::ABNORMAL_TERMINATION;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return GpuTerminationStatus::PROCESS_WAS_KILLED;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
#if defined(OS_WIN)
    // Treat integrity failure as a crash on Windows.
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
#endif
      return GpuTerminationStatus::PROCESS_CRASHED;
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return GpuTerminationStatus::STILL_RUNNING;
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return GpuTerminationStatus::PROCESS_WAS_KILLED_BY_OOM;
#endif
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      return GpuTerminationStatus::OOM_PROTECTED;
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return GpuTerminationStatus::LAUNCH_FAILED;
    case base::TERMINATION_STATUS_OOM:
      return GpuTerminationStatus::OOM;
    case base::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED();
      return GpuTerminationStatus::MAX_ENUM;
      // Do not add default.
  }
  NOTREACHED();
  return GpuTerminationStatus::ABNORMAL_TERMINATION;
}

// Command-line switches to propagate to the GPU process.
static const char* const kSwitchNames[] = {
    service_manager::switches::kDisableSeccompFilterSandbox,
    service_manager::switches::kGpuSandboxAllowSysVShm,
    service_manager::switches::kGpuSandboxFailuresFatal,
    service_manager::switches::kDisableGpuSandbox,
    service_manager::switches::kNoSandbox,
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    switches::kDisableDevShmUsage,
#endif
#if defined(OS_WIN)
    switches::kDisableHighResTimer,
#endif  // defined(OS_WIN)
    switches::kEnableANGLEFeatures,
    switches::kDisableANGLEFeatures,
    switches::kDisableBreakpad,
    switches::kDisableGpuRasterization,
    switches::kDisableGLExtensions,
    switches::kDisableLogging,
    switches::kDisableShaderNameHashing,
    switches::kDisableSkiaRuntimeOpts,
    switches::kDisableWebRtcHWEncoding,
    switches::kEnableGpuRasterization,
    switches::kEnableLogging,
    switches::kEnableDeJelly,
    switches::kDeJellyScreenWidth,
    switches::kDoubleBufferCompositing,
    switches::kEnableVizDevTools,
    switches::kHeadless,
    switches::kLoggingLevel,
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kProfilingAtStart,
    switches::kProfilingFile,
    switches::kProfilingFlush,
    switches::kRunAllCompositorStagesBeforeDraw,
    switches::kSkiaFontCacheLimitMb,
    switches::kSkiaResourceCacheLimitMb,
    switches::kTestGLLib,
    switches::kTraceToConsole,
    switches::kUseFakeMjpegDecodeAccelerator,
    switches::kUseGpuInTests,
    switches::kV,
    switches::kVModule,
#if defined(OS_MACOSX)
    service_manager::switches::kEnableSandboxLogging,
    switches::kDisableAVFoundationOverlays,
    switches::kDisableMacOverlays,
    switches::kDisableRemoteCoreAnimation,
    switches::kShowMacOverlayBorders,
    switches::kUseHighGPUThreadPriorityForPerfTests,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
    switches::kDisableExplicitDmaFences,
    switches::kOzoneDumpFile,
#endif
#if defined(USE_X11)
    switches::kX11Display,
    switches::kNoXshm,
#endif
    switches::kGpuBlacklistTestGroup,
    switches::kGpuDriverBugListTestGroup,
    switches::kUseCmdDecoder,
    switches::kForceVideoOverlays,
#if defined(OS_ANDROID)
    switches::kEnableReachedCodeProfiler,
#endif
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum GPUProcessLifetimeEvent {
  LAUNCHED = 0,
  // When the GPU process crashes the (DIED_FIRST_TIME + recent_crash_count - 1)
  // bucket in the appropriate UMA histogram will be incremented. The first
  // crash will be DIED_FIRST_TIME, the second DIED_FIRST_TIME+1, etc.
  DIED_FIRST_TIME = 1,
  GPU_PROCESS_LIFETIME_EVENT_MAX = 100,
};

// Indexed by GpuProcessKind. There is one of each kind maximum. This array may
// only be accessed from the IO thread.
GpuProcessHost* g_gpu_process_hosts[GPU_PROCESS_KIND_COUNT];

void RunCallbackOnIO(GpuProcessKind kind,
                     bool force_create,
                     base::OnceCallback<void(GpuProcessHost*)> callback) {
  GpuProcessHost* host = GpuProcessHost::Get(kind, force_create);
  std::move(callback).Run(host);
}

void OnGpuProcessHostDestroyedOnUI(int host_id, const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GpuDataManagerImpl::GetInstance()->AddLogMessage(logging::LOG_ERROR,
                                                   "GpuProcessHost", message);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelDestroyed(host_id);
#endif
}

// NOTE: changes to this class need to be reviewed by the security team.
class GpuSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  explicit GpuSandboxedProcessLauncherDelegate(
      const base::CommandLine& cmd_line)
      :
#if defined(OS_WIN)
        enable_appcontainer_(true),
#endif
        cmd_line_(cmd_line) {
  }

  ~GpuSandboxedProcessLauncherDelegate() override = default;

#if defined(OS_WIN)
  bool DisableDefaultPolicy() override { return true; }

  enum GPUAppContainerEnableState{
      AC_ENABLED = 0,
      AC_DISABLED_GL = 1,
      AC_DISABLED_FORCE = 2,
      MAX_ENABLE_STATE = 3,
  };

  bool GetAppContainerId(std::string* appcontainer_id) override {
    if (UseOpenGLRenderer()) {
      base::UmaHistogramEnumeration("GPU.AppContainer.EnableState",
                                    AC_DISABLED_GL, MAX_ENABLE_STATE);
      return false;
    }

    if (!enable_appcontainer_) {
      base::UmaHistogramEnumeration("GPU.AppContainer.EnableState",
                                    AC_DISABLED_FORCE, MAX_ENABLE_STATE);
      return false;
    }

    *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
    base::UmaHistogramEnumeration("GPU.AppContainer.EnableState", AC_ENABLED,
                                  MAX_ENABLE_STATE);
    return true;
  }

  // For the GPU process we gotten as far as USER_LIMITED. The next level
  // which is USER_RESTRICTED breaks both the DirectX backend and the OpenGL
  // backend. Note that the GPU process is connected to the interactive
  // desktop.
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (UseOpenGLRenderer()) {
      // Open GL path.
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);
      service_manager::SandboxWin::SetJobLevel(
          cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
    } else {
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);

      // UI restrictions break when we access Windows from outside our job.
      // However, we don't want a proxy window in this process because it can
      // introduce deadlocks where the renderer blocks on the gpu, which in
      // turn blocks on the browser UI thread. So, instead we forgo a window
      // message pump entirely and just add job restrictions to prevent child
      // processes.
      service_manager::SandboxWin::SetJobLevel(
          cmd_line_, sandbox::JOB_LIMITED_USER,
          JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS | JOB_OBJECT_UILIMIT_DESKTOP |
              JOB_OBJECT_UILIMIT_EXITWINDOWS |
              JOB_OBJECT_UILIMIT_DISPLAYSETTINGS,
          policy);
    }

    // Check if we are running on the winlogon desktop and set a delayed
    // integrity in this case. This is needed because a low integrity gpu
    // process will not be allowed to access the winlogon desktop (gpu process
    // integrity has to be at least medium in order to be able to access the
    // winlogon desktop normally). So instead, let the gpu process start with
    // the normal integrity and delay the switch to low integrity until after
    // the gpu process has started and has access to the desktop.
    if (ShouldSetDelayedIntegrity())
      policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    else
      policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

    // Block this DLL even if it is not loaded by the browser process.
    policy->AddDllToUnload(L"cmsetac.dll");

    if (cmd_line_.HasSwitch(switches::kEnableLogging)) {
      base::string16 log_file_path = logging::GetLogFileFullPath();
      if (!log_file_path.empty()) {
        sandbox::ResultCode result = policy->AddRule(
            sandbox::TargetPolicy::SUBSYS_FILES,
            sandbox::TargetPolicy::FILES_ALLOW_ANY, log_file_path.c_str());
        if (result != sandbox::SBOX_ALL_OK)
          return false;
      }
    }

    return true;
  }

  // TODO: Remove this once AppContainer sandbox is enabled by default.
  void DisableAppContainer() { enable_appcontainer_ = false; }
#endif  // OS_WIN

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  service_manager::ZygoteHandle GetZygote() override {
    if (service_manager::IsUnsandboxedSandboxType(GetSandboxType()))
      return nullptr;

    // The GPU process needs a specialized sandbox, so fork from the unsandboxed
    // zygote and then apply the actual sandboxes in the forked process.
    return service_manager::GetUnsandboxedZygote();
  }
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

  service_manager::SandboxType GetSandboxType() override {
    if (cmd_line_.HasSwitch(service_manager::switches::kDisableGpuSandbox)) {
      DVLOG(1) << "GPU sandbox is disabled";
      return service_manager::SandboxType::kNoSandbox;
    }
    return service_manager::SandboxType::kGpu;
  }

 private:
#if defined(OS_WIN)
  bool UseOpenGLRenderer() {
    return cmd_line_.GetSwitchValueASCII(switches::kUseGL) ==
           gl::kGLImplementationDesktopName;
  }

  bool ShouldSetDelayedIntegrity() {
    if (UseOpenGLRenderer())
      return true;

    return base::win::IsRunningUnderDesktopName(STRING16_LITERAL("winlogon"));
  }

  bool enable_appcontainer_;
#endif

  base::CommandLine cmd_line_;
};

#if defined(OS_WIN)
void RecordAppContainerStatus(int error_code, bool crashed_before) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!crashed_before &&
      service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
          *command_line, service_manager::SandboxType::kGpu)) {
    base::UmaHistogramSparse("GPU.AppContainer.Status", error_code);
  }
}
#endif  // defined(OS_WIN)

void BindDiscardableMemoryReceiverOnIO(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver,
    discardable_memory::DiscardableSharedMemoryManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  manager->Bind(std::move(receiver));
}

void BindDiscardableMemoryReceiverOnUI(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BindDiscardableMemoryReceiverOnIO, std::move(receiver),
          discardable_memory::DiscardableSharedMemoryManager::Get()));
}

}  // anonymous namespace

// static
bool GpuProcessHost::ValidateHost(GpuProcessHost* host) {
  // The Gpu process is invalid if it's not using SwiftShader, the card is
  // blacklisted, and we can kill it and start over.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU) ||
      host->valid_) {
    return true;
  }

  host->ForceShutdown();
  return false;
}

// static
GpuProcessHost* GpuProcessHost::Get(GpuProcessKind kind, bool force_create) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  if (!gpu_data_manager->GpuProcessStartAllowed()) {
    DLOG(ERROR) << "!GpuDataManagerImpl::GpuProcessStartAllowed()";
    return nullptr;
  }

  // Do not launch the unsandboxed GPU info collection process if GPU is
  // disabled
  if (kind == GPU_PROCESS_KIND_INFO_COLLECTION) {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kDisableGpu) ||
        command_line->HasSwitch(switches::kSingleProcess) ||
        command_line->HasSwitch(switches::kInProcessGPU))
      return nullptr;
  }

  if (g_gpu_process_hosts[kind] && ValidateHost(g_gpu_process_hosts[kind]))
    return g_gpu_process_hosts[kind];

  if (!force_create)
    return nullptr;

  // Do not create a new process if browser is shutting down.
  if (BrowserMainRunner::ExitedMainMessageLoop()) {
    DLOG(ERROR) << "BrowserMainRunner::ExitedMainMessageLoop()";
    return nullptr;
  }

  static int last_host_id = 0;
  int host_id;
  host_id = ++last_host_id;

  GpuProcessHost* host = new GpuProcessHost(host_id, kind);
  if (host->Init())
    return host;

  // TODO(sievers): Revisit this behavior. It's not really a crash, but we also
  // want the fallback-to-sw behavior if we cannot initialize the GPU.
  host->RecordProcessCrash();

  delete host;
  DLOG(ERROR) << "GpuProcessHost::Init() failed";
  return nullptr;
}

// static
void GpuProcessHost::GetHasGpuProcess(base::OnceCallback<void(bool)> callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&GpuProcessHost::GetHasGpuProcess, std::move(callback)));
    return;
  }
  bool has_gpu = false;
  for (size_t i = 0; i < base::size(g_gpu_process_hosts); ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && ValidateHost(host)) {
      has_gpu = true;
      break;
    }
  }
  std::move(callback).Run(has_gpu);
}

// static
void GpuProcessHost::CallOnIO(
    GpuProcessKind kind,
    bool force_create,
    base::OnceCallback<void(GpuProcessHost*)> callback) {
#if !defined(OS_WIN)
  DCHECK_NE(kind, GPU_PROCESS_KIND_INFO_COLLECTION);
#endif
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&RunCallbackOnIO, kind, force_create,
                                std::move(callback)));
}

void GpuProcessHost::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (interface_name ==
      discardable_memory::mojom::DiscardableSharedMemoryManager::Name_) {
    BindDiscardableMemoryReceiver(
        mojo::PendingReceiver<
            discardable_memory::mojom::DiscardableSharedMemoryManager>(
            std::move(interface_pipe)));
    return;
  }
  process_->child_process()->BindReceiver(
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe)));
}

void GpuProcessHost::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  process_->GetHost()->RunService(service_name, std::move(receiver));
}

#if defined(USE_OZONE)
void GpuProcessHost::TerminateGpuProcess(const std::string& message) {
  // At the moment, this path is only used by Ozone/Wayland. Once others start
  // to use this, start to distinguish the origin of termination. By default,
  // it's unknown.
  termination_origin_ = GpuTerminationOrigin::kOzoneWaylandProxy;
  process_->TerminateOnBadMessageReceived(message);
}
#endif  // defined(USE_OZONE)

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (int i = 0; i < GPU_PROCESS_KIND_COUNT; ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && host->host_id_ == host_id && ValidateHost(host))
      return host;
  }

  return nullptr;
}

// static
int GpuProcessHost::GetGpuCrashCount() {
  return static_cast<int>(base::subtle::NoBarrier_Load(&gpu_crash_count_));
}

// static
void GpuProcessHost::IncrementCrashCount(gpu::GpuMode gpu_mode) {
  int forgive_minutes = GetForgiveMinutes(gpu_mode);
  DCHECK_GT(forgive_minutes, 0);

  // Last time the process crashed.
  static base::TimeTicks last_crash_time;

  base::TimeTicks current_time = base::TimeTicks::Now();
  if (gpu_mode != last_crash_mode_) {
    // Reset the crash count when the GPU starts crashing in a different mode.
    recent_crash_count_ = 0;
  } else if (crashed_before_) {
    // Remove one crash per |forgive_minutes| from the crash count, so
    // occasional crashes won't add up and eventually prevent using the GPU
    // process.
    int minutes_delta = (current_time - last_crash_time).InMinutes();
    int crashes_to_forgive = minutes_delta / forgive_minutes;
    recent_crash_count_ = std::max(0, recent_crash_count_ - crashes_to_forgive);
  }
  recent_crash_count_ =
      std::min(recent_crash_count_ + 1,
               static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX) - 1);

  crashed_before_ = true;
  last_crash_mode_ = gpu_mode;
  last_crash_time = current_time;
}

GpuProcessHost::GpuProcessHost(int host_id, GpuProcessKind kind)
    : host_id_(host_id),
      valid_(true),
      in_process_(false),
      kind_(kind),
      process_launched_(false) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    in_process_ = true;
  }
#if !defined(OS_ANDROID)
  if (!in_process_ && kind != GPU_PROCESS_KIND_INFO_COLLECTION &&
      base::FeatureList::IsEnabled(
          features::kForwardMemoryPressureEventsToGpuProcess)) {
    memory_pressure_listener_ =
        std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
            &GpuProcessHost::OnMemoryPressure, base::Unretained(this)));
  }
#endif

  // If the 'single GPU process' policy ever changes, we still want to maintain
  // it for 'gpu thread' mode and only create one instance of host and thread.
  DCHECK(!in_process_ || g_gpu_process_hosts[kind] == nullptr);

  g_gpu_process_hosts[kind] = this;

  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_GPU, this, ChildProcessHost::IpcMode::kNormal));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_process_gpu_thread_)
    DCHECK(process_);

  SendOutstandingReplies();

#if defined(OS_MACOSX)
  if (ca_transaction_gpu_coordinator_) {
    ca_transaction_gpu_coordinator_->HostWillBeDestroyed();
    ca_transaction_gpu_coordinator_ = nullptr;
  }
#endif

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = nullptr;

#if defined(OS_ANDROID)
  UMA_HISTOGRAM_COUNTS_100("GPU.AtExitSurfaceCount",
                           gpu::GpuSurfaceTracker::Get()->GetSurfaceCount());
#endif

  bool block_offscreen_contexts = true;
  if (!in_process_ && process_launched_) {
    ChildProcessTerminationInfo info =
        process_->GetTerminationInfo(false /* known_dead */);
    std::string message;
    if (kind_ == GPU_PROCESS_KIND_SANDBOXED) {
      UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus2",
                                ConvertToGpuTerminationStatus(info.status),
                                GpuTerminationStatus::MAX_ENUM);

      if (info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
          info.status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
          info.status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
        // Windows always returns PROCESS_CRASHED on abnormal termination, as it
        // doesn't have a way to distinguish the two.
        base::UmaHistogramSparse("GPU.GPUProcessExitCode",
                                 base::ClampToRange(info.exit_code, 0, 100));
      }

      message = "The GPU process ";
    } else {
      message = "The info collection GPU process ";
    }

    switch (info.status) {
      case base::TERMINATION_STATUS_NORMAL_TERMINATION:
        // Don't block offscreen contexts (and force page reload for webgl)
        // if this was an intentional shutdown or the OOM killer on Android
        // killed us while Chrome was in the background.
        block_offscreen_contexts = false;
        message += "exited normally. Everything is okay.";
        break;
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
        message += base::StringPrintf("exited with code %d.", info.exit_code);
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
        UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationOrigin",
                                  termination_origin_,
                                  GpuTerminationOrigin::kMax);
        message += "was killed by you! Why?";
        break;
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
        message += "crashed!";
        break;
      case base::TERMINATION_STATUS_STILL_RUNNING:
        message += "hasn't exited yet.";
        break;
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
        message += "was killed due to out of memory.";
        break;
#endif  // OS_CHROMEOS
#if defined(OS_ANDROID)
      case base::TERMINATION_STATUS_OOM_PROTECTED:
        message += "was protected from out of memory kill.";
        break;
#endif  // OS_ANDROID
      case base::TERMINATION_STATUS_LAUNCH_FAILED:
        message += "failed to start!";
        break;
      case base::TERMINATION_STATUS_OOM:
        message += "died due to out of memory.";
        break;
#if defined(OS_WIN)
      case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
        message += "failed integrity checks.";
        break;
#endif
      case base::TERMINATION_STATUS_MAX_ENUM:
        NOTREACHED();
        break;
    }
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&OnGpuProcessHostDestroyedOnUI, host_id_, message));
  }

  // If there are any remaining offscreen contexts at the point the GPU process
  // exits, assume something went wrong, and block their URLs from accessing
  // client 3D APIs without prompting.
  if (block_offscreen_contexts && gpu_host_)
    gpu_host_->BlockLiveOffscreenContexts();
}

bool GpuProcessHost::Init() {
  init_start_time_ = base::TimeTicks::Now();

  TRACE_EVENT_INSTANT0("gpu", "LaunchGpuProcess", TRACE_EVENT_SCOPE_THREAD);

  process_->GetHost()->CreateChannelMojo();

  mode_ = GpuDataManagerImpl::GetInstance()->GetGpuMode();
  DCHECK_NE(mode_, gpu::GpuMode::DISABLED);

  if (in_process_) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(GetGpuMainThreadFactory());
    gpu::GpuPreferences gpu_preferences = GetGpuPreferencesFromCommandLine();
    GpuDataManagerImpl::GetInstance()->UpdateGpuPreferences(
        &gpu_preferences, GPU_PROCESS_KIND_SANDBOXED);
    in_process_gpu_thread_.reset(GetGpuMainThreadFactory()(
        InProcessChildThreadParams(base::ThreadTaskRunnerHandle::Get(),
                                   process_->GetInProcessMojoInvitation()),
        gpu_preferences));
    base::Thread::Options options;
#if defined(OS_WIN) || defined(OS_MACOSX)
    // WGL needs to create its own window and pump messages on it.
    options.message_pump_type = base::MessagePumpType::UI;
#endif
    if (base::FeatureList::IsEnabled(features::kGpuUseDisplayThreadPriority))
      options.priority = base::ThreadPriority::DISPLAY;
    in_process_gpu_thread_->StartWithOptions(options);
  } else if (!LaunchGpuProcess()) {
    return false;
  }

  mojo::PendingAssociatedRemote<viz::mojom::VizMain> viz_main_pending_remote;
  process_->child_channel()
      ->GetAssociatedInterfaceSupport()
      ->GetRemoteAssociatedInterface(
          viz_main_pending_remote.InitWithNewEndpointAndPassReceiver());
  viz::GpuHostImpl::InitParams params;
  params.restart_id = host_id_;
  params.disable_gpu_shader_disk_cache =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache);
  params.product = GetContentClient()->browser()->GetProduct();
  params.deadline_to_synchronize_surfaces =
      switches::GetDeadlineToSynchronizeSurfaces();
  params.main_thread_task_runner =
      base::CreateSingleThreadTaskRunner({BrowserThread::UI});
  params.info_collection_gpu_process =
      kind_ == GPU_PROCESS_KIND_INFO_COLLECTION;
  gpu_host_ = std::make_unique<viz::GpuHostImpl>(
      this, std::move(viz_main_pending_remote), std::move(params));

  if (in_process_) {
    // Fake a callback that the process is ready.
    OnProcessLaunched();
  }

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableVizDevTools)) {
    // Start creating a socket for the Viz DevTools server. If it is created
    // before the FrameSinkManager, the socket will be stashed.
    devtools_connector_ = std::make_unique<VizDevToolsConnector>();
    devtools_connector_->ConnectVizDevTools();
  }
#endif

#if defined(OS_MACOSX)
  ca_transaction_gpu_coordinator_ = CATransactionGPUCoordinator::Create(this);
#endif

  return true;
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_->GetHost()->IsChannelOpening()) {
    queued_messages_.push(msg);
    return true;
  }

  bool result = process_->Send(msg);
  if (!result) {
    // Channel is hosed, but we may not get destroyed for a while. Send
    // outstanding channel creation failures now so that the caller can restart
    // with a new process/channel without waiting.
    SendOutstandingReplies();
  }
  return result;
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void GpuProcessHost::OnChannelConnected(int32_t peer_pid) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnChannelConnected");

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void GpuProcessHost::OnProcessLaunched() {
  UMA_HISTOGRAM_TIMES("GPU.GPUProcessLaunchTime",
                      base::TimeTicks::Now() - init_start_time_);
#if defined(OS_WIN)
  if (kind_ == GPU_PROCESS_KIND_SANDBOXED)
    RecordAppContainerStatus(sandbox::SBOX_ALL_OK, crashed_before_);
#endif  // defined(OS_WIN)

  DCHECK(gpu_host_);
  if (in_process_) {
    // Don't set |process_id_| as it is publicly available through process_id().
    gpu_host_->SetProcessId(base::GetCurrentProcId());
  } else {
    process_id_ = process_->GetProcess().Pid();
    DCHECK_NE(base::kNullProcessId, process_id_);
    gpu_host_->SetProcessId(process_id_);
  }
}

void GpuProcessHost::OnProcessLaunchFailed(int error_code) {
#if defined(OS_WIN)
  if (kind_ == GPU_PROCESS_KIND_SANDBOXED)
    RecordAppContainerStatus(error_code, crashed_before_);
#endif  // defined(OS_WIN)
  RecordProcessCrash();
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  // Record crash before doing anything that could start a new GPU process.
  RecordProcessCrash();

  gpu_host_->OnProcessCrashed();

  SendOutstandingReplies();

  ChildProcessTerminationInfo info =
      process_->GetTerminationInfo(true /* known_dead */);
  GpuDataManagerImpl::GetInstance()->ProcessCrashed(info.status);
}

gpu::GPUInfo GpuProcessHost::GetGPUInfo() const {
  return GpuDataManagerImpl::GetInstance()->GetGPUInfo();
}

gpu::GpuFeatureInfo GpuProcessHost::GetGpuFeatureInfo() const {
  return GpuDataManagerImpl::GetInstance()->GetGpuFeatureInfo();
}

void GpuProcessHost::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu,
    const gpu::GpuExtraInfo& gpu_extra_info) {
  if (GetGpuCrashCount() > 0) {
    LOG(WARNING) << "Reinitialized the GPU process after a crash. The reported "
                    "initialization time was "
                 << gpu_info.initialization_time.InMilliseconds() << " ms";
  }
  if (kind_ != GPU_PROCESS_KIND_INFO_COLLECTION) {
    auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
    // Update GpuFeatureInfo first, because UpdateGpuInfo() will notify all
    // listeners.
    gpu_data_manager->UpdateGpuFeatureInfo(gpu_feature_info,
                                           gpu_feature_info_for_hardware_gpu);
    gpu_data_manager->UpdateGpuInfo(gpu_info, gpu_info_for_hardware_gpu);
    gpu_data_manager->UpdateGpuExtraInfo(gpu_extra_info);
  }

#if defined(OS_ANDROID)
  // Android may kill the GPU process to free memory, especially when the app
  // is the background, so Android cannot have a hard limit on GPU starts.
  // Reset crash count on Android when context creation succeeds.
  recent_crash_count_ = 0;
#endif
}

void GpuProcessHost::DidFailInitialize() {
  did_fail_initialize_ = true;
  if (kind_ == GPU_PROCESS_KIND_SANDBOXED)
    GpuDataManagerImpl::GetInstance()->FallBackToNextGpuMode();
}

void GpuProcessHost::DidCreateContextSuccessfully() {
#if defined(OS_ANDROID)
  // Android may kill the GPU process to free memory, especially when the app
  // is the background, so Android cannot have a hard limit on GPU starts.
  // Reset crash count on Android when context creation succeeds.
  recent_crash_count_ = 0;
#endif
}

void GpuProcessHost::MaybeShutdownGpuProcess() {
  if (!in_process_ &&
      GetContentClient()->browser()->CanShutdownGpuProcessNowOnIOThread()) {
    delete this;
  }
}

#if defined(OS_WIN)
void GpuProcessHost::DidUpdateOverlayInfo(
    const gpu::OverlayInfo& overlay_info) {
  GpuDataManagerImpl::GetInstance()->UpdateOverlayInfo(overlay_info);
}
#endif

void GpuProcessHost::BlockDomainFrom3DAPIs(const GURL& url,
                                           gpu::DomainGuilt guilt) {
  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(url, guilt);
}

bool GpuProcessHost::GpuAccessAllowed() const {
  return GpuDataManagerImpl::GetInstance()->GpuAccessAllowed(nullptr);
}

void GpuProcessHost::DisableGpuCompositing() {
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  DLOG(ERROR) << "Can't disable GPU compositing";
#else
  // TODO(crbug.com/819474): The switch from GPU to software compositing should
  // be handled here instead of by ImageTransportFactory.
  base::PostTask(FROM_HERE, {BrowserThread::UI}, base::BindOnce([]() {
                   if (auto* factory = ImageTransportFactory::GetInstance())
                     factory->DisableGpuCompositing();
                 }));
#endif
}

gpu::ShaderCacheFactory* GpuProcessHost::GetShaderCacheFactory() {
  return GetShaderCacheFactorySingleton();
}

void GpuProcessHost::RecordLogMessage(int32_t severity,
                                      const std::string& header,
                                      const std::string& message) {
  GpuDataManagerImpl::GetInstance()->AddLogMessage(severity, header, message);
}

void GpuProcessHost::BindDiscardableMemoryReceiver(
    mojo::PendingReceiver<
        discardable_memory::mojom::DiscardableSharedMemoryManager> receiver) {
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BindDiscardableMemoryReceiverOnUI, std::move(receiver)));
}

GpuProcessKind GpuProcessHost::kind() {
  return kind_;
}

void GpuProcessHost::ForceShutdown() {
  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = nullptr;

  process_->ForceShutdown();
}

void GpuProcessHost::RunService(mojo::GenericPendingReceiver receiver) {
  process_->child_process()->BindServiceInterface(std::move(receiver));
}

bool GpuProcessHost::LaunchGpuProcess() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

#if defined(OS_ANDROID)
  // crbug.com/447735. readlink("self/proc/exe") sometimes fails on Android
  // at startup with EACCES. As a workaround ignore this here, since the
  // executable name is actually not used or useful anyways.
  std::unique_ptr<base::CommandLine> cmd_line =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
#else
#if defined(OS_LINUX)
  int child_flags = gpu_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF
                                         : ChildProcessHost::CHILD_NORMAL;
#elif defined(OS_MACOSX)
  int child_flags = ChildProcessHost::CHILD_GPU;
#else
  int child_flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
  if (exe_path.empty())
    return false;

  std::unique_ptr<base::CommandLine> cmd_line =
      std::make_unique<base::CommandLine>(exe_path);
#endif

  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());
  BrowserChildProcessHostImpl::CopyTraceStartupFlags(cmd_line.get());

#if defined(OS_WIN)
  cmd_line->AppendArg(switches::kPrefetchArgumentGpu);
#endif  // defined(OS_WIN)

  if (kind_ == GPU_PROCESS_KIND_INFO_COLLECTION) {
    cmd_line->AppendSwitch(service_manager::switches::kDisableGpuSandbox);
    cmd_line->AppendSwitchASCII(switches::kUseGL,
                                gl::kGLImplementationDisabledName);

    // Pass the current device info to the info-collection GPU process for
    // crash key logging.
    const gpu::GPUInfo::GPUDevice device_info = GetGPUInfo().active_gpu();
    cmd_line->AppendSwitchASCII(
        switches::kGpuVendorId,
        base::StringPrintf("%u", device_info.vendor_id));
    cmd_line->AppendSwitchASCII(
        switches::kGpuDeviceId,
        base::StringPrintf("%u", device_info.device_id));
#if defined(OS_WIN)
    cmd_line->AppendSwitchASCII(
        switches::kGpuSubSystemId,
        base::StringPrintf("%u", device_info.sub_sys_id));
    cmd_line->AppendSwitchASCII(switches::kGpuRevision,
                                base::StringPrintf("%u", device_info.revision));
#endif
    if (device_info.driver_version.length()) {
      cmd_line->AppendSwitchASCII(switches::kGpuDriverVersion,
                                  device_info.driver_version);
    }
  }

  // TODO(penghuang): Replace all GPU related switches with GpuPreferences.
  // https://crbug.com/590825
  // If you want a browser command-line switch passed to the GPU process
  // you need to add it to |kSwitchNames| at the beginning of this file.
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             base::size(kSwitchNames));
  cmd_line->CopySwitchesFrom(
      browser_command_line, switches::kGLSwitchesCopiedFromGpuProcessHost,
      switches::kGLSwitchesCopiedFromGpuProcessHostNumSwitches);

  if (browser_command_line.HasSwitch(switches::kDisableFrameRateLimit))
    cmd_line->AppendSwitch(switches::kDisableGpuVsync);

  std::vector<const char*> gpu_workarounds;
  gpu::GpuDriverBugList::AppendAllWorkarounds(&gpu_workarounds);
  cmd_line->CopySwitchesFrom(browser_command_line, gpu_workarounds.data(),
                             gpu_workarounds.size());

  // Because AppendExtraCommandLineSwitches is called here, we should call
  // LaunchWithoutExtraCommandLineSwitches() instead of Launch for gpu process
  // launch below.
  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line.get(), process_->GetData().id);

  // TODO(kylechar): The command line flags added here should be based on
  // |mode_|.
  GpuDataManagerImpl::GetInstance()->AppendGpuCommandLine(cmd_line.get(),
                                                          kind_);

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  std::unique_ptr<GpuSandboxedProcessLauncherDelegate> delegate =
      std::make_unique<GpuSandboxedProcessLauncherDelegate>(*cmd_line);
#if defined(OS_WIN)
  if (crashed_before_)
    delegate->DisableAppContainer();
#endif  // defined(OS_WIN)

  // Do not call process_->Launch() here.
  // AppendExtraCommandLineSwitches will be called again in process_->Launch(),
  // Call LaunchWithoutExtraCommandLineSwitches() so the command line switches
  // will not be appended twice.
  process_->LaunchWithoutExtraCommandLineSwitches(
      std::move(delegate), std::move(cmd_line), /*files_to_preload=*/{}, true);
  process_launched_ = true;

  if (kind_ == GPU_PROCESS_KIND_SANDBOXED) {
    base::UmaHistogramEnumeration(GetProcessLifetimeUmaName(mode_), LAUNCHED,
                                  GPU_PROCESS_LIFETIME_EVENT_MAX);
  }

  return true;
}

void GpuProcessHost::SendOutstandingReplies() {
  valid_ = false;

  if (gpu_host_)
    gpu_host_->SendOutstandingReplies();
}

void GpuProcessHost::RecordProcessCrash() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Maximum number of times the GPU process can crash before we try something
  // different, like disabling hardware acceleration or all GL.
  constexpr int kGpuFallbackCrashCount = 3;
#else
  // Android and Chrome OS switch to software compositing and fallback crashes
  // the browser process. For Android the OS can also kill the GPU process
  // arbitrarily. Use a larger maximum crash count here.
  constexpr int kGpuFallbackCrashCount = 6;
#endif

  // Ending only acts as a failure if the GPU process was actually started and
  // was intended for actual rendering (and not just checking caps or other
  // options).
  if (!process_launched_ || kind_ != GPU_PROCESS_KIND_SANDBOXED)
    return;

  // Keep track of the total number of GPU crashes.
  base::subtle::NoBarrier_AtomicIncrement(&gpu_crash_count_, 1);
  LOG(WARNING) << "The GPU process has crashed " << GetGpuCrashCount()
               << " time(s)";

  // It's possible GPU mode fallback has already happened. In this case, |mode_|
  // will still be the mode of the failed process.
  IncrementCrashCount(mode_);
  base::UmaHistogramExactLinear(
      GetProcessLifetimeUmaName(mode_),
      DIED_FIRST_TIME + recent_crash_count_ - 1,
      static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX));

  // GPU process initialization failed and fallback already happened.
  if (did_fail_initialize_)
    return;

  bool disable_crash_limit = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuProcessCrashLimit);

  // GPU process crashed too many times, fallback on a different GPU process
  // mode.
  if (recent_crash_count_ >= kGpuFallbackCrashCount && !disable_crash_limit)
    GpuDataManagerImpl::GetInstance()->FallBackToNextGpuMode();
}

viz::mojom::GpuService* GpuProcessHost::gpu_service() {
  DCHECK(gpu_host_);
  return gpu_host_->gpu_service();
}

#if defined(OS_WIN)
viz::mojom::InfoCollectionGpuService*
GpuProcessHost::info_collection_gpu_service() {
  DCHECK(gpu_host_);
  return gpu_host_->info_collection_gpu_service();
}
#endif

int GpuProcessHost::GetIDForTesting() const {
  return process_->GetData().id;
}

#if !defined(OS_ANDROID)
void GpuProcessHost::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  gpu_host_->gpu_service()->OnMemoryPressure(level);
}
#endif

}  // namespace content
