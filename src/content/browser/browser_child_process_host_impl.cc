// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_child_process_host_impl.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/token.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/histogram_controller.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_coordinator_service.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_channel.h"
#include "services/service_manager/embedder/switches.h"
#include "services/tracing/public/cpp/trace_startup.h"

#if defined(OS_MACOSX)
#include "content/browser/child_process_task_port_provider_mac.h"
#include "content/browser/sandbox_support_mac_impl.h"
#include "content/common/sandbox_support_mac.mojom.h"
#endif

#if defined(OS_WIN)
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/public/common/font_cache_dispatcher_win.h"
#include "content/public/common/font_cache_win.mojom.h"
#endif

namespace content {
namespace {

static base::LazyInstance<
    BrowserChildProcessHostImpl::BrowserChildProcessList>::DestructorAtExit
    g_child_process_list = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::ObserverList<BrowserChildProcessObserver>::Unchecked>::
    DestructorAtExit g_browser_child_process_observers =
        LAZY_INSTANCE_INITIALIZER;

void NotifyProcessLaunchedAndConnected(const ChildProcessData& data) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessLaunchedAndConnected(data);
}

void NotifyProcessHostConnected(const ChildProcessData& data) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessHostConnected(data);
}

void NotifyProcessHostDisconnected(const ChildProcessData& data) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessHostDisconnected(data);
}

#if !defined(OS_ANDROID)
void NotifyProcessCrashed(const ChildProcessData& data,
                          const ChildProcessTerminationInfo& info) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessCrashed(data, info);
}
#endif

void NotifyProcessKilled(const ChildProcessData& data,
                         const ChildProcessTerminationInfo& info) {
  for (auto& observer : g_browser_child_process_observers.Get())
    observer.BrowserChildProcessKilled(data, info);
}

memory_instrumentation::mojom::ProcessType GetCoordinatorClientProcessType(
    ProcessType process_type) {
  switch (process_type) {
    case PROCESS_TYPE_RENDERER:
      return memory_instrumentation::mojom::ProcessType::RENDERER;
    case PROCESS_TYPE_UTILITY:
      return memory_instrumentation::mojom::ProcessType::UTILITY;
    case PROCESS_TYPE_GPU:
      return memory_instrumentation::mojom::ProcessType::GPU;
    case PROCESS_TYPE_PPAPI_PLUGIN:
    case PROCESS_TYPE_PPAPI_BROKER:
      return memory_instrumentation::mojom::ProcessType::PLUGIN;
    default:
      NOTREACHED();
      return memory_instrumentation::mojom::ProcessType::OTHER;
  }
}

void BindTracedProcessOnIOThread(
    base::WeakPtr<BrowserChildProcessHostImpl> weak_host,
    mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver) {
  if (!weak_host)
    return;

  weak_host->GetHost()->BindReceiver(std::move(receiver));
}

void BindTracedProcessFromUIThread(
    base::WeakPtr<BrowserChildProcessHostImpl> weak_host,
    mojo::PendingReceiver<tracing::mojom::TracedProcess> receiver) {
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&BindTracedProcessOnIOThread,
                                std::move(weak_host), std::move(receiver)));
}

}  // namespace

// static
std::unique_ptr<BrowserChildProcessHost> BrowserChildProcessHost::Create(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    ChildProcessHost::IpcMode ipc_mode) {
  return std::make_unique<BrowserChildProcessHostImpl>(process_type, delegate,
                                                       ipc_mode);
}

BrowserChildProcessHost* BrowserChildProcessHost::FromID(int child_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserChildProcessHostImpl::BrowserChildProcessList* process_list =
      g_child_process_list.Pointer();
  for (BrowserChildProcessHostImpl* host : *process_list) {
    if (host->GetData().id == child_process_id)
      return host;
  }
  return nullptr;
}

#if defined(OS_MACOSX)
base::PortProvider* BrowserChildProcessHost::GetPortProvider() {
  return ChildProcessTaskPortProvider::GetInstance();
}
#endif

// static
BrowserChildProcessHostImpl::BrowserChildProcessList*
BrowserChildProcessHostImpl::GetIterator() {
  return g_child_process_list.Pointer();
}

// static
void BrowserChildProcessHostImpl::AddObserver(
    BrowserChildProcessObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_browser_child_process_observers.Get().AddObserver(observer);
}

// static
void BrowserChildProcessHostImpl::RemoveObserver(
    BrowserChildProcessObserver* observer) {
  // TODO(phajdan.jr): Check thread after fixing http://crbug.com/167126.
  g_browser_child_process_observers.Get().RemoveObserver(observer);
}

BrowserChildProcessHostImpl::BrowserChildProcessHostImpl(
    content::ProcessType process_type,
    BrowserChildProcessHostDelegate* delegate,
    ChildProcessHost::IpcMode ipc_mode)
    : data_(process_type),
      delegate_(delegate),
      channel_(nullptr),
      is_channel_connected_(false),
      notify_child_disconnected_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  data_.id = ChildProcessHostImpl::GenerateChildProcessUniqueId();

  child_process_host_ = ChildProcessHost::Create(this, ipc_mode);

  g_child_process_list.Get().push_back(this);
  GetContentClient()->browser()->BrowserChildProcessHostCreated(this);

  // Create a persistent memory segment for subprocess histograms.
  CreateMetricsAllocator();
}

BrowserChildProcessHostImpl::~BrowserChildProcessHostImpl() {
  g_child_process_list.Get().remove(this);

  if (notify_child_disconnected_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessHostDisconnected, data_.Duplicate()));
  }
}

// static
void BrowserChildProcessHostImpl::TerminateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Make a copy since the BrowserChildProcessHost dtor mutates the original
  // list.
  BrowserChildProcessList copy = g_child_process_list.Get();
  for (auto it = copy.begin(); it != copy.end(); ++it) {
    delete (*it)->delegate();  // ~*HostDelegate deletes *HostImpl.
  }
}

// static
void BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(
    base::CommandLine* cmd_line) {
  // If we run base::FieldTrials, we want to pass to their state to the
  // child process so that it can act in accordance with each state.
  base::FieldTrialList::CopyFieldTrialStateToFlags(
      switches::kFieldTrialHandle, switches::kEnableFeatures,
      switches::kDisableFeatures, cmd_line);
}

// static
void BrowserChildProcessHostImpl::CopyTraceStartupFlags(
    base::CommandLine* cmd_line) {
  tracing::PropagateTracingFlagsToChildProcessCmdLine(cmd_line);
}

void BrowserChildProcessHostImpl::Launch(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    bool terminate_on_shutdown) {
  LaunchWithPreloadedFiles(std::move(delegate), std::move(cmd_line),
                           /*files_to_preload=*/{}, terminate_on_shutdown);
}

void BrowserChildProcessHostImpl::LaunchWithPreloadedFiles(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    std::map<std::string, base::FilePath> files_to_preload,
    bool terminate_on_shutdown) {
  GetContentClient()->browser()->AppendExtraCommandLineSwitches(cmd_line.get(),
                                                                data_.id);
  LaunchWithoutExtraCommandLineSwitches(
      std::move(delegate), std::move(cmd_line), std::move(files_to_preload),
      terminate_on_shutdown);
}

const ChildProcessData& BrowserChildProcessHostImpl::GetData() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return data_;
}

ChildProcessHost* BrowserChildProcessHostImpl::GetHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return child_process_host_.get();
}

const base::Process& BrowserChildProcessHostImpl::GetProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(child_process_.get())
      << "Requesting a child process handle before launching.";
  DCHECK(child_process_->GetProcess().IsValid())
      << "Requesting a child process handle before launch has completed OK.";
  return child_process_->GetProcess();
}

std::unique_ptr<base::PersistentMemoryAllocator>
BrowserChildProcessHostImpl::TakeMetricsAllocator() {
  return std::move(metrics_allocator_);
}

void BrowserChildProcessHostImpl::SetName(const base::string16& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.name = name;
}

void BrowserChildProcessHostImpl::SetMetricsName(
    const std::string& metrics_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.metrics_name = metrics_name;
}

void BrowserChildProcessHostImpl::SetProcess(base::Process process) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  data_.SetProcess(std::move(process));
}

void BrowserChildProcessHostImpl::ForceShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_child_process_list.Get().remove(this);
  child_process_host_->ForceShutdown();
}

void BrowserChildProcessHostImpl::AddFilter(BrowserMessageFilter* filter) {
  child_process_host_->AddFilter(filter->GetFilter());
}

void BrowserChildProcessHostImpl::LaunchWithoutExtraCommandLineSwitches(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> cmd_line,
    std::map<std::string, base::FilePath> files_to_preload,
    bool terminate_on_shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  static const char* const kForwardSwitches[] = {
      net::kWebSocketReadBufferSize,
      net::kWebSocketReceiveQuotaThreshold,
      service_manager::switches::kDisableInProcessStackTraces,
      switches::kDisableBestEffortTasks,
      switches::kDisableLogging,
      switches::kEnableLogging,
      switches::kIPCConnectionTimeout,
      switches::kLogBestEffortTasks,
      switches::kLogFile,
      switches::kLoggingLevel,
      switches::kPerfettoDisableInterning,
      switches::kTraceToConsole,
      switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches,
                             base::size(kForwardSwitches));

  // All processes should have a non-empty metrics name.
  if (data_.metrics_name.empty())
    data_.metrics_name = GetProcessTypeNameInEnglish(data_.process_type);

  data_.sandbox_type = delegate->GetSandboxType();

  notify_child_disconnected_ = true;
  child_process_.reset(new ChildProcessLauncher(
      std::move(delegate), std::move(cmd_line), data_.id, this,
      std::move(*child_process_host_->GetMojoInvitation()),
      base::BindRepeating(&BrowserChildProcessHostImpl::OnMojoError,
                          weak_factory_.GetWeakPtr(),
                          base::ThreadTaskRunnerHandle::Get()),
      std::move(files_to_preload), terminate_on_shutdown));
  ShareMetricsAllocatorToProcess();
}

void BrowserChildProcessHostImpl::HistogramBadMessageTerminated(
    ProcessType process_type) {
  UMA_HISTOGRAM_ENUMERATION("ChildProcess.BadMessgeTerminated", process_type,
                            PROCESS_TYPE_MAX);
}

#if defined(OS_ANDROID)
void BrowserChildProcessHostImpl::EnableWarmUpConnection() {
  can_use_warm_up_connection_ = true;
}
#endif

ChildProcessTerminationInfo BrowserChildProcessHostImpl::GetTerminationInfo(
    bool known_dead) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!child_process_) {
    // If the delegate doesn't use Launch() helper.
    ChildProcessTerminationInfo info;
    info.status = base::GetTerminationStatus(data_.GetProcess().Handle(),
                                             &info.exit_code);
    return info;
  }
  return child_process_->GetChildTerminationInfo(known_dead);
}

bool BrowserChildProcessHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  return delegate_->OnMessageReceived(message);
}

void BrowserChildProcessHostImpl::OnChannelConnected(int32_t peer_pid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  is_channel_connected_ = true;
  notify_child_disconnected_ = true;

#if defined(OS_MACOSX)
  ChildProcessTaskPortProvider::GetInstance()->OnChildProcessLaunched(
      peer_pid, static_cast<ChildProcessHostImpl*>(child_process_host_.get())
                    ->child_process());
#endif

#if defined(OS_WIN)
  // From this point onward, the exit of the child process is detected by an
  // error on the IPC channel.
  early_exit_watcher_.StopWatching();
#endif

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyProcessHostConnected, data_.Duplicate()));

  delegate_->OnChannelConnected(peer_pid);

  if (IsProcessLaunched()) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessLaunchedAndConnected, data_.Duplicate()));
  }
}

void BrowserChildProcessHostImpl::OnChannelError() {
  delegate_->OnChannelError();
}

void BrowserChildProcessHostImpl::OnBadMessageReceived(
    const IPC::Message& message) {
  std::string log_message = "Bad message received of type: ";
  if (message.IsValid()) {
    log_message += std::to_string(message.type());
  } else {
    log_message += "unknown";
  }
  TerminateOnBadMessageReceived(log_message);
}

void BrowserChildProcessHostImpl::TerminateOnBadMessageReceived(
    const std::string& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Create a memory dump. This will contain enough stack frames to work out
  // what the bad message was.
  base::debug::DumpWithoutCrashing();

  TerminateProcessForBadMessage(weak_factory_.GetWeakPtr(), error);
}

void BrowserChildProcessHostImpl::OnChannelInitialized(IPC::Channel* channel) {
  channel_ = channel;
}

void BrowserChildProcessHostImpl::OnChildDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  tracing_registration_.reset();

#if defined(OS_WIN)
  // OnChildDisconnected may be called without OnChannelConnected, so stop the
  // early exit watcher so GetTerminationStatus can close the process handle.
  early_exit_watcher_.StopWatching();
#endif
  if (child_process_.get() || data_.GetProcess().IsValid()) {
    ChildProcessTerminationInfo info =
        GetTerminationInfo(true /* known_dead */);
#if defined(OS_ANDROID)
    // Do not treat clean_exit, ie when child process exited due to quitting
    // its main loop, as a crash.
    if (!info.clean_exit) {
      delegate_->OnProcessCrashed(info.exit_code);
    }
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessKilled, data_.Duplicate(), info));
#else  // OS_ANDROID
    switch (info.status) {
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION: {
        delegate_->OnProcessCrashed(info.exit_code);
        base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&NotifyProcessCrashed, data_.Duplicate(), info));
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Crashed2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED: {
        delegate_->OnProcessCrashed(info.exit_code);
        base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(&NotifyProcessKilled, data_.Duplicate(), info));
        // Report that this child process was killed.
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
      case base::TERMINATION_STATUS_STILL_RUNNING: {
        UMA_HISTOGRAM_ENUMERATION("ChildProcess.DisconnectedAlive2",
                                  static_cast<ProcessType>(data_.process_type),
                                  PROCESS_TYPE_MAX);
        break;
      }
      default:
        break;
    }
#endif  // OS_ANDROID
    UMA_HISTOGRAM_ENUMERATION("ChildProcess.Disconnected2",
                              static_cast<ProcessType>(data_.process_type),
                              PROCESS_TYPE_MAX);
#if defined(OS_CHROMEOS)
    if (info.status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM) {
      UMA_HISTOGRAM_ENUMERATION("ChildProcess.Killed2.OOM",
                                static_cast<ProcessType>(data_.process_type),
                                PROCESS_TYPE_MAX);
    }
#endif
  }
  channel_ = nullptr;
  delete delegate_;  // Will delete us
}

bool BrowserChildProcessHostImpl::Send(IPC::Message* message) {
  return child_process_host_->Send(message);
}

void BrowserChildProcessHostImpl::CreateMetricsAllocator() {
  // Create a persistent memory segment for subprocess histograms only if
  // they're active in the browser.
  // TODO(bcwhite): Remove this once persistence is always enabled.
  if (!base::GlobalHistogramAllocator::Get())
    return;

  // Determine the correct parameters based on the process type.
  size_t memory_size;
  base::StringPiece metrics_name;
  switch (data_.process_type) {
    case PROCESS_TYPE_UTILITY:
      // This needs to be larger for the network service.
      memory_size = 256 << 10;  // 256 KiB
      metrics_name = "UtilityMetrics";
      break;

    case PROCESS_TYPE_ZYGOTE:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "ZygoteMetrics";
      break;

    case PROCESS_TYPE_SANDBOX_HELPER:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "SandboxHelperMetrics";
      break;

    case PROCESS_TYPE_GPU:
      // This needs to be larger for the display-compositor in the gpu process.
      memory_size = 256 << 10;  // 256 KiB
      metrics_name = "GpuMetrics";
      break;

    case PROCESS_TYPE_PPAPI_PLUGIN:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "PpapiPluginMetrics";
      break;

    case PROCESS_TYPE_PPAPI_BROKER:
      memory_size = 64 << 10;  // 64 KiB
      metrics_name = "PpapiBrokerMetrics";
      break;

    default:
      return;
  }

  // Create the shared memory segment and attach an allocator to it.
  // Mapping the memory shouldn't fail but be safe if it does; everything
  // will continue to work but just as if persistence weren't available.
  base::WritableSharedMemoryRegion shm_region =
      base::WritableSharedMemoryRegion::Create(memory_size);
  base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
  if (!shm_region.IsValid() || !shm_mapping.IsValid())
    return;
  metrics_allocator_ =
      std::make_unique<base::WritableSharedPersistentMemoryAllocator>(
          std::move(shm_mapping), static_cast<uint64_t>(data_.id),
          metrics_name);
  metrics_shared_region_ = std::move(shm_region);
}

void BrowserChildProcessHostImpl::ShareMetricsAllocatorToProcess() {
  if (metrics_allocator_) {
    HistogramController::GetInstance()->SetHistogramMemory<ChildProcessHost>(
        GetHost(), std::move(metrics_shared_region_));
  } else {
    HistogramController::GetInstance()->SetHistogramMemory<ChildProcessHost>(
        GetHost(), base::WritableSharedMemoryRegion());
  }
}

void BrowserChildProcessHostImpl::OnProcessLaunchFailed(int error_code) {
  delegate_->OnProcessLaunchFailed(error_code);
  notify_child_disconnected_ = false;
  delete delegate_;  // Will delete us
}

#if defined(OS_ANDROID)
bool BrowserChildProcessHostImpl::CanUseWarmUpConnection() {
  return can_use_warm_up_connection_;
}
#endif

void BrowserChildProcessHostImpl::OnProcessLaunched() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::Process& process = child_process_->GetProcess();
  DCHECK(process.IsValid());

#if defined(OS_WIN)
  // Start a WaitableEventWatcher that will invoke OnProcessExitedEarly if the
  // child process exits. This watcher is stopped once the IPC channel is
  // connected and the exit of the child process is detecter by an error on the
  // IPC channel thereafter.
  DCHECK(!early_exit_watcher_.GetWatchedObject());
  early_exit_watcher_.StartWatchingOnce(process.Handle(), this);
#endif

  data_.SetProcess(process.Duplicate());
  delegate_->OnProcessLaunched();

  if (is_channel_connected_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&NotifyProcessLaunchedAndConnected, data_.Duplicate()));
  }

  tracing_registration_ = TracingServiceController::Get().RegisterClient(
      process.Pid(), base::BindRepeating(&BindTracedProcessFromUIThread,
                                         weak_factory_.GetWeakPtr()));
}

void BrowserChildProcessHostImpl::RegisterCoordinatorClient(
    mojo::PendingReceiver<memory_instrumentation::mojom::Coordinator> receiver,
    mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess>
        client_process) {
  // The child process may have already terminated by the time this message is
  // dispatched. We do nothing in that case.
  if (!IsProcessLaunched())
    return;

  base::trace_event::MemoryDumpManager::GetInstance()
      ->GetDumpThreadTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](mojo::PendingReceiver<
                     memory_instrumentation::mojom::Coordinator> receiver,
                 mojo::PendingRemote<
                     memory_instrumentation::mojom::ClientProcess>
                     client_process,
                 memory_instrumentation::mojom::ProcessType process_type,
                 base::ProcessId process_id,
                 base::Optional<std::string> service_name) {
                GetMemoryInstrumentationCoordinatorController()
                    ->RegisterClientProcess(
                        std::move(receiver), std::move(client_process),
                        process_type, process_id, std::move(service_name));
              },
              std::move(receiver), std::move(client_process),
              GetCoordinatorClientProcessType(
                  static_cast<ProcessType>(data_.process_type)),
              child_process_->GetProcess().Pid(), delegate_->GetServiceName()));
}

bool BrowserChildProcessHostImpl::IsProcessLaunched() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return child_process_.get() && child_process_->GetProcess().IsValid();
}

// static
void BrowserChildProcessHostImpl::OnMojoError(
    base::WeakPtr<BrowserChildProcessHostImpl> process,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const std::string& error) {
  // Create a memory dump with the error message captured in a crash key value.
  // This will make it easy to determine details about what interface call
  // failed.
  //
  // It is important to call DumpWithoutCrashing synchronously - this will help
  // to preserve the callstack and the crash keys present when the bad mojo
  // message was received.
  mojo::debug::ScopedMessageErrorCrashKey scoped_error_key(error);
  base::debug::DumpWithoutCrashing();

  if (task_runner->BelongsToCurrentThread()) {
    TerminateProcessForBadMessage(process, error);
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserChildProcessHostImpl::TerminateProcessForBadMessage,
            process, error));
  }
}

// static
void BrowserChildProcessHostImpl::TerminateProcessForBadMessage(
    base::WeakPtr<BrowserChildProcessHostImpl> process,
    const std::string& error) {
  if (!process)
    return;
  HistogramBadMessageTerminated(
      static_cast<ProcessType>(process->data_.process_type));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableKillAfterBadIPC)) {
    return;
  }
  LOG(ERROR) << "Terminating child process for bad message: " << error;
  process->child_process_->Terminate(RESULT_CODE_KILLED_BAD_MESSAGE);
}

#if defined(OS_WIN)

void BrowserChildProcessHostImpl::OnObjectSignaled(HANDLE object) {
  OnChildDisconnected();
}

#endif

}  // namespace content
