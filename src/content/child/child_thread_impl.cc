// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_thread_impl.h"

#include <signal.h>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/profiler.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/message_loop/timer_slack.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/child/browser_exposed_child_interfaces.h"
#include "content/child/child_process.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/child_process.mojom.h"
#include "content/common/field_trial_recorder.mojom.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/device/public/cpp/power_monitor/power_monitor_broadcast_source.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_impl.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_agent_provider_impl.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mach_port_rendezvous.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include <stdio.h>
#if defined(OS_WIN)
#include <io.h>
#endif
// Function provided by libclang_rt.profile-*.a, declared and documented at:
// https://github.com/llvm/llvm-project/blob/master/compiler-rt/lib/profile/InstrProfiling.h
extern "C" void __llvm_profile_set_file_object(FILE* File, int EnableMerge);
#endif

namespace content {
namespace {

// How long to wait for a connection to the browser process before giving up.
const int kConnectionTimeoutS = 15;

base::LazyInstance<base::ThreadLocalPointer<ChildThreadImpl>>::DestructorAtExit
    g_lazy_child_thread_impl_tls = LAZY_INSTANCE_INITIALIZER;

// This isn't needed on Windows because there the sandbox's job object
// terminates child processes automatically. For unsandboxed processes (i.e.
// plugins), PluginThread has EnsureTerminateMessageFilter.
#if defined(OS_POSIX)

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
// A thread delegate that waits for |duration| and then exits the process
// immediately, without executing finalizers.
class WaitAndExitDelegate : public base::PlatformThread::Delegate {
 public:
  explicit WaitAndExitDelegate(base::TimeDelta duration)
      : duration_(duration) {}

  void ThreadMain() override {
    base::PlatformThread::Sleep(duration_);
    base::Process::TerminateCurrentProcessImmediately(0);
  }

 private:
  const base::TimeDelta duration_;
  DISALLOW_COPY_AND_ASSIGN(WaitAndExitDelegate);
};

bool CreateWaitAndExitThread(base::TimeDelta duration) {
  std::unique_ptr<WaitAndExitDelegate> delegate(
      new WaitAndExitDelegate(duration));

  const bool thread_created =
      base::PlatformThread::CreateNonJoinable(0, delegate.get());
  if (!thread_created)
    return false;

  // A non joinable thread has been created. The thread will either terminate
  // the process or will be terminated by the process. Therefore, keep the
  // delegate object alive for the lifetime of the process.
  WaitAndExitDelegate* leaking_delegate = delegate.release();
  ANNOTATE_LEAKING_OBJECT_PTR(leaking_delegate);
  ignore_result(leaking_delegate);
  return true;
}
#endif

class SuicideOnChannelErrorFilter : public IPC::MessageFilter {
 public:
  // IPC::MessageFilter
  void OnChannelError() override {
    // For renderer/worker processes:
    // On POSIX, at least, one can install an unload handler which loops
    // forever and leave behind a renderer process which eats 100% CPU forever.
    //
    // This is because the terminate signals (FrameMsg_BeforeUnload and the
    // error from the IPC sender) are routed to the main message loop but never
    // processed (because that message loop is stuck in V8).
    //
    // One could make the browser SIGKILL the renderers, but that leaves open a
    // large window where a browser failure (or a user, manually terminating
    // the browser because "it's stuck") will leave behind a process eating all
    // the CPU.
    //
    // So, we install a filter on the sender so that we can process this event
    // here and kill the process.
    base::debug::StopProfiling();
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) || \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
    // Some sanitizer tools rely on exit handlers (e.g. to run leak detection,
    // or dump code coverage data to disk). Instead of exiting the process
    // immediately, we give it 60 seconds to run exit handlers.
    CHECK(CreateWaitAndExitThread(base::TimeDelta::FromSeconds(60)));
#if defined(LEAK_SANITIZER)
    // Invoke LeakSanitizer early to avoid detecting shutdown-only leaks. If
    // leaks are found, the process will exit here.
    __lsan_do_leak_check();
#endif
#else
    base::Process::TerminateCurrentProcessImmediately(0);
#endif
  }

 protected:
  ~SuicideOnChannelErrorFilter() override {}
};

#endif  // OS(POSIX)

mojo::IncomingInvitation InitializeMojoIPCChannel() {
  TRACE_EVENT0("startup", "InitializeMojoIPCChannel");
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_WIN)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          mojo::PlatformChannel::kHandleSwitch)) {
    endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
        *base::CommandLine::ForCurrentProcess());
  } else {
    // If this process is elevated, it will have a pipe path passed on the
    // command line.
    endpoint = mojo::NamedPlatformChannel::ConnectToServer(
        *base::CommandLine::ForCurrentProcess());
  }
#elif defined(OS_FUCHSIA)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif defined(OS_MACOSX)
  auto* client = base::MachPortRendezvousClient::GetInstance();
  if (!client) {
    LOG(ERROR) << "Mach rendezvous failed, terminating process (parent died?)";
    base::Process::TerminateCurrentProcessImmediately(0);
    return {};
  }
  auto receive = client->TakeReceiveRight('mojo');
  if (!receive.is_valid()) {
    LOG(ERROR) << "Invalid PlatformChannel receive right";
    return {};
  }
  endpoint =
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(receive)));
#elif defined(OS_POSIX)
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif

  return mojo::IncomingInvitation::Accept(
      std::move(endpoint), MOJO_ACCEPT_INVITATION_FLAG_LEAK_TRANSPORT_ENDPOINT);
}

}  // namespace

// Implements the mojom ChildProcess interface and lives on the IO thread.
class ChildThreadImpl::IOThreadState
    : public base::RefCountedThreadSafe<IOThreadState>,
      public mojom::ChildProcess {
 public:
  IOThreadState(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      base::WeakPtr<ChildThreadImpl> weak_main_thread,
      base::RepeatingClosure quit_closure,
      ChildThreadImpl::Options::ServiceBinder service_binder,
      mojo::PendingReceiver<mojom::ChildProcessHost> host_receiver)
      : main_thread_task_runner_(std::move(main_thread_task_runner)),
        weak_main_thread_(std::move(weak_main_thread)),
        quit_closure_(std::move(quit_closure)),
        service_binder_(std::move(service_binder)),
        host_receiver_(std::move(host_receiver)) {}

  // Used only in the deprecated Service Manager IPC mode.
  void BindChildProcessReceiver(
      mojo::PendingReceiver<mojom::ChildProcess> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // Used in non-Service Manager IPC mode.
  void BindChildProcessReceiverAndLegacyIpc(
      mojo::PendingReceiver<mojom::ChildProcess> receiver,
      mojo::PendingRemote<IPC::mojom::ChannelBootstrap> legacy_ipc_bootstrap) {
    legacy_ipc_bootstrap_ = std::move(legacy_ipc_bootstrap);
    receiver_.Bind(std::move(receiver));
  }

  void ExposeInterfacesToBrowser(mojo::BinderMap binders) {
    DCHECK(wait_for_interface_binders_);
    wait_for_interface_binders_ = false;
    interface_binders_ = std::move(binders);
    std::vector<mojo::GenericPendingReceiver> pending_requests;
    std::swap(pending_requests, pending_binding_requests_);
    for (auto& receiver : pending_requests)
      BindReceiver(std::move(receiver));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadState>;

  ~IOThreadState() override = default;

  // mojom::ChildProcess:
  void Initialize(mojo::PendingRemote<mojom::ChildProcessHostBootstrap>
                      bootstrap) override {
    // The browser only calls this method once.
    DCHECK(host_receiver_);
    mojo::Remote<mojom::ChildProcessHostBootstrap>(std::move(bootstrap))
        ->BindProcessHost(std::move(host_receiver_));
  }

  void ProcessShutdown() override {
    main_thread_task_runner_->PostTask(FROM_HERE,
                                       base::BindOnce(quit_closure_));
  }

#if defined(OS_MACOSX)
  void GetTaskPort(GetTaskPortCallback callback) override {
    mojo::PlatformHandle task_port(
        (base::mac::ScopedMachSendRight(task_self_trap())));
    std::move(callback).Run(std::move(task_port));
  }
#endif

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  void SetIPCLoggingEnabled(bool enable) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](bool enable) {
                         if (enable)
                           IPC::Logging::GetInstance()->Enable();
                         else
                           IPC::Logging::GetInstance()->Disable();
                       },
                       enable));
  }
#endif

  void GetBackgroundTracingAgentProvider(
      mojo::PendingReceiver<tracing::mojom::BackgroundTracingAgentProvider>
          receiver) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildThreadImpl::GetBackgroundTracingAgentProvider,
                       weak_main_thread_, std::move(receiver)));
  }

  // Make sure this isn't inlined so it shows up in stack traces, and also make
  // the function body unique by adding a log line, so it doesn't get merged
  // with other functions by link time optimizations (ICF).
  NOINLINE void CrashHungProcess() override {
    LOG(ERROR) << "Crashing because hung";
    IMMEDIATE_CRASH();
  }

  void BootstrapLegacyIpc(
      mojo::PendingReceiver<IPC::mojom::ChannelBootstrap> receiver) override {
    DCHECK(legacy_ipc_bootstrap_);
    mojo::FusePipes(std::move(receiver), std::move(legacy_ipc_bootstrap_));
  }

  void RunService(const std::string& service_name,
                  mojo::PendingReceiver<service_manager::mojom::Service>
                      receiver) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildThreadImpl::RunService, weak_main_thread_,
                       service_name, std::move(receiver)));
  }

  void BindServiceInterface(mojo::GenericPendingReceiver receiver) override {
    if (service_binder_)
      service_binder_.Run(&receiver);

    if (receiver) {
      main_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&ChildThreadImpl::BindServiceInterface,
                                    weak_main_thread_, std::move(receiver)));
    }
  }

  void BindReceiver(mojo::GenericPendingReceiver receiver) override {
    if (wait_for_interface_binders_) {
      pending_binding_requests_.push_back(std::move(receiver));
      return;
    }

    if (interface_binders_.TryBind(&receiver))
      return;

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChildThreadImpl::OnBindReceiver,
                                  weak_main_thread_, std::move(receiver)));
  }

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void SetProfilingFile(base::File file) override {
    // TODO(crbug.com/985574) Remove Android check when possible.
#if defined(OS_POSIX) && !defined(OS_ANDROID)
    // Take the file descriptor so that |file| does not close it.
    int fd = file.TakePlatformFile();
    FILE* f = fdopen(fd, "r+b");
    __llvm_profile_set_file_object(f, 1);
#elif defined(OS_WIN)
    HANDLE handle = file.TakePlatformFile();
    int fd = _open_osfhandle((intptr_t)handle, 0);
    FILE* f = _fdopen(fd, "r+b");
    __llvm_profile_set_file_object(f, 1);
#endif
  }
#endif

  const scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  const base::WeakPtr<ChildThreadImpl> weak_main_thread_;
  const base::RepeatingClosure quit_closure_;

  ChildThreadImpl::Options::ServiceBinder service_binder_;
  mojo::BinderMap interface_binders_;
  bool wait_for_interface_binders_ = true;
  mojo::Receiver<mojom::ChildProcess> receiver_{this};
  mojo::PendingReceiver<mojom::ChildProcessHost> host_receiver_;

  // The pending legacy IPC channel endpoint to fuse with one we will eventually
  // receiver on the ChildProcess interface. Only used when not in the
  // deprecated Service Manager IPC mode.
  mojo::PendingRemote<IPC::mojom::ChannelBootstrap> legacy_ipc_bootstrap_;

  // Binding requests which should be handled by |interface_binders|, but which
  // have been queued because |allow_interface_binders_| is still |false|.
  std::vector<mojo::GenericPendingReceiver> pending_binding_requests_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadState);
};

ChildThread* ChildThread::Get() {
  return ChildThreadImpl::current();
}

ChildThreadImpl::Options::Options() : connect_to_browser(false) {}

ChildThreadImpl::Options::Options(const Options& other) = default;

ChildThreadImpl::Options::~Options() {
}

ChildThreadImpl::Options::Builder::Builder() {
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::InBrowserProcess(
    const InProcessChildThreadParams& params) {
  options_.browser_process_io_runner = params.io_runner();
  options_.mojo_invitation = params.mojo_invitation();
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ConnectToBrowser(
    bool connect_to_browser_parms) {
  options_.connect_to_browser = connect_to_browser_parms;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::AddStartupFilter(
    IPC::MessageFilter* filter) {
  options_.startup_filters.push_back(filter);
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::IPCTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_parms) {
  options_.ipc_task_runner = ipc_task_runner_parms;
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ServiceBinder(
    ChildThreadImpl::Options::ServiceBinder binder) {
  options_.service_binder = std::move(binder);
  return *this;
}

ChildThreadImpl::Options::Builder&
ChildThreadImpl::Options::Builder::ExposesInterfacesToBrowser() {
  options_.exposes_interfaces_to_browser = true;
  return *this;
}

ChildThreadImpl::Options ChildThreadImpl::Options::Builder::Build() {
  return options_;
}

ChildThreadImpl::ChildThreadMessageRouter::ChildThreadMessageRouter(
    IPC::Sender* sender)
    : sender_(sender) {}

bool ChildThreadImpl::ChildThreadMessageRouter::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool ChildThreadImpl::ChildThreadMessageRouter::RouteMessage(
    const IPC::Message& msg) {
  bool handled = IPC::MessageRouter::RouteMessage(msg);
#if defined(OS_ANDROID)
  if (!handled && msg.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
  }
#endif
  return handled;
}

ChildThreadImpl::ChildThreadImpl(base::RepeatingClosure quit_closure)
    : ChildThreadImpl(std::move(quit_closure), Options::Builder().Build()) {}

ChildThreadImpl::ChildThreadImpl(base::RepeatingClosure quit_closure,
                                 const Options& options)
    : router_(this),
      quit_closure_(std::move(quit_closure)),
      browser_process_io_runner_(options.browser_process_io_runner),
      channel_connected_factory_(
          new base::WeakPtrFactory<ChildThreadImpl>(this)),
      ipc_task_runner_(options.ipc_task_runner) {
  mojo::PendingRemote<mojom::ChildProcessHost> remote_host;
  auto host_receiver = remote_host.InitWithNewPipeAndPassReceiver();
  child_process_host_ = mojo::SharedRemote<mojom::ChildProcessHost>(
      std::move(remote_host), GetIOTaskRunner());
  io_thread_state_ = base::MakeRefCounted<IOThreadState>(
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr(),
      quit_closure_, std::move(options.service_binder),
      std::move(host_receiver));

  // |ExposeInterfacesToBrowser()| must be called exactly once. Subclasses which
  // set |exposes_interfaces_to_browser| in Options signify that they take
  // responsibility for calling it.
  //
  // For other process types, we call it to expose only the basic set of
  // interfaces common to all child process types.
  if (!options.exposes_interfaces_to_browser)
    ExposeInterfacesToBrowser(mojo::BinderMap());

  Init(options);
}

scoped_refptr<base::SingleThreadTaskRunner> ChildThreadImpl::GetIOTaskRunner() {
  if (IsInBrowserProcess())
    return browser_process_io_runner_;
  return ChildProcess::current()->io_task_runner();
}

void ChildThreadImpl::SetFieldTrialGroup(const std::string& trial_name,
                                         const std::string& group_name) {
  if (field_trial_syncer_)
    field_trial_syncer_->OnSetFieldTrialGroup(trial_name, group_name);
}

void ChildThreadImpl::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  mojo::Remote<mojom::FieldTrialRecorder> field_trial_recorder;
  BindHostReceiver(field_trial_recorder.BindNewPipeAndPassReceiver());
  field_trial_recorder->FieldTrialActivated(trial_name);
}

void ChildThreadImpl::Init(const Options& options) {
  TRACE_EVENT0("startup", "ChildThreadImpl::Init");
  g_lazy_child_thread_impl_tls.Pointer()->Set(this);
  on_channel_error_called_ = false;
  main_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  // We must make sure to instantiate the IPC Logger *before* we create the
  // channel, otherwise we can get a callback on the IO thread which creates
  // the logger, and the logger does not like being created on the IO thread.
  IPC::Logging::GetInstance();
#endif

  channel_ = IPC::SyncChannel::Create(
      this, ChildProcess::current()->io_task_runner(),
      ipc_task_runner_ ? ipc_task_runner_ : base::ThreadTaskRunnerHandle::Get(),
      ChildProcess::current()->GetShutDownEvent());
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  if (!IsInBrowserProcess())
    IPC::Logging::GetInstance()->SetIPCSender(this);
#endif

  // Only one of these will be made valid by the block below. This determines
  // whether we were launched in normal IPC mode or deprecated Service Manager
  // IPC mode.
  mojo::ScopedMessagePipeHandle child_process_pipe;
  if (!IsInBrowserProcess()) {
    scoped_refptr<base::SingleThreadTaskRunner> mojo_ipc_task_runner =
        GetIOTaskRunner();
    if (base::FeatureList::IsEnabled(features::kMojoDedicatedThread)) {
      mojo_ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0));
      mojo_ipc_task_runner = mojo_ipc_thread_.task_runner();
    }
    mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
        mojo_ipc_task_runner,
        mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));
    mojo::IncomingInvitation invitation = InitializeMojoIPCChannel();
    child_process_pipe = invitation.ExtractMessagePipe(0);
  } else {
    child_process_pipe = options.mojo_invitation->ExtractMessagePipe(0);
  }

  sync_message_filter_ = channel_->CreateSyncMessageFilter();
  thread_safe_sender_ =
      new ThreadSafeSender(main_thread_runner_, sync_message_filter_.get());

  // In single process mode, browser-side tracing and memory will cover the
  // whole process including renderers.
  if (!IsInBrowserProcess()) {
    mojo::PendingRemote<memory_instrumentation::mojom::Coordinator> coordinator;
    mojo::PendingRemote<memory_instrumentation::mojom::ClientProcess> process;
    auto process_receiver = process.InitWithNewPipeAndPassReceiver();
    mojo::Remote<memory_instrumentation::mojom::CoordinatorConnector> connector;
    BindHostReceiver(connector.BindNewPipeAndPassReceiver());
    connector->RegisterCoordinatorClient(
        coordinator.InitWithNewPipeAndPassReceiver(), std::move(process));
    memory_instrumentation::ClientProcessImpl::CreateInstance(
        std::move(process_receiver), std::move(coordinator));
  }

  // In single process mode we may already have initialized the power monitor,
  if (!base::PowerMonitor::IsInitialized()) {
    auto power_monitor_source =
        std::make_unique<device::PowerMonitorBroadcastSource>(
            GetIOTaskRunner());
    auto* source_ptr = power_monitor_source.get();
    base::PowerMonitor::Initialize(std::move(power_monitor_source));
    // The two-phase init is necessary to ensure that the process-wide
    // PowerMonitor is set before the power monitor source receives incoming
    // communication from the browser process (see https://crbug.com/821790 for
    // details)
    mojo::PendingRemote<device::mojom::PowerMonitor> remote_power_monitor;
    BindHostReceiver(remote_power_monitor.InitWithNewPipeAndPassReceiver());
    source_ptr->Init(std::move(remote_power_monitor));
  }

#if defined(OS_POSIX)
  // Check that --process-type is specified so we don't do this in unit tests
  // and single-process mode.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessType))
    channel_->AddFilter(new SuicideOnChannelErrorFilter());
#endif

  // Add filters passed here via options.
  for (auto* startup_filter : options.startup_filters) {
    channel_->AddFilter(startup_filter);
  }

  DCHECK(child_process_pipe.is_valid());
  mojo::PendingRemote<IPC::mojom::ChannelBootstrap> legacy_ipc_bootstrap;
  mojo::ScopedMessagePipeHandle legacy_ipc_channel_handle =
      legacy_ipc_bootstrap.InitWithNewPipeAndPassReceiver().PassPipe();
  channel_->Init(IPC::ChannelMojo::CreateClientFactory(
                     std::move(legacy_ipc_channel_handle),
                     ChildProcess::current()->io_task_runner(),
                     ipc_task_runner_ ? ipc_task_runner_
                                      : base::ThreadTaskRunnerHandle::Get()),
                 /*create_pipe_now=*/true);

  ChildThreadImpl::GetIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IOThreadState::BindChildProcessReceiverAndLegacyIpc,
                     io_thread_state_,
                     mojo::PendingReceiver<mojom::ChildProcess>(
                         std::move(child_process_pipe)),
                     std::move(legacy_ipc_bootstrap)));

  int connection_timeout = kConnectionTimeoutS;
  std::string connection_override =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIPCConnectionTimeout);
  if (!connection_override.empty()) {
    int temp;
    if (base::StringToInt(connection_override, &temp))
      connection_timeout = temp;
  }

  main_thread_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChildThreadImpl::EnsureConnected,
                     channel_connected_factory_->GetWeakPtr()),
      base::TimeDelta::FromSeconds(connection_timeout));

  // In single-process mode, there is no need to synchronize trials to the
  // browser process (because it's the same process).
  if (!IsInBrowserProcess()) {
    field_trial_syncer_.reset(
        new variations::ChildProcessFieldTrialSyncer(this));
    field_trial_syncer_->InitFieldTrialObserving(
        *base::CommandLine::ForCurrentProcess());
  }
}

ChildThreadImpl::~ChildThreadImpl() {
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  channel_->RemoveFilter(sync_message_filter_.get());

  // The ChannelProxy object caches a pointer to the IPC thread, so need to
  // reset it as it's not guaranteed to outlive this object.
  // NOTE: this also has the side-effect of not closing the main IPC channel to
  // the browser process.  This is needed because this is the signal that the
  // browser uses to know that this process has died, so we need it to be alive
  // until this process is shut down, and the OS closes the handle
  // automatically.  We used to watch the object handle on Windows to do this,
  // but it wasn't possible to do so on POSIX.
  channel_->ClearIPCTaskRunner();
  g_lazy_child_thread_impl_tls.Pointer()->Set(nullptr);
}

void ChildThreadImpl::Shutdown() {
  // Ensure that our IOThreadState's last ref goes away on the IO thread.
  ChildThreadImpl::GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<IOThreadState>) {},
                                std::move(io_thread_state_)));
}

bool ChildThreadImpl::ShouldBeDestroyed() {
  return true;
}

void ChildThreadImpl::OnChannelConnected(int32_t peer_pid) {
  channel_connected_factory_.reset();
}

void ChildThreadImpl::OnChannelError() {
  on_channel_error_called_ = true;
  // If this thread runs in the browser process, only Thread::Stop should
  // stop its message loop. Otherwise, QuitWhenIdle could race Thread::Stop.
  if (!IsInBrowserProcess())
    quit_closure_.Run();
}

bool ChildThreadImpl::Send(IPC::Message* msg) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (!channel_) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

#if defined(OS_WIN)
void ChildThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  GetFontCacheWin()->PreCacheFont(log_font);
}

void ChildThreadImpl::ReleaseCachedFonts() {
  GetFontCacheWin()->ReleaseCachedFonts();
}

const mojo::Remote<mojom::FontCacheWin>& ChildThreadImpl::GetFontCacheWin() {
  if (!font_cache_win_)
    BindHostReceiver(font_cache_win_.BindNewPipeAndPassReceiver());
  return font_cache_win_;
}
#endif

void ChildThreadImpl::RecordAction(const base::UserMetricsAction& action) {
    NOTREACHED();
}

void ChildThreadImpl::RecordComputedAction(const std::string& action) {
    NOTREACHED();
}

void ChildThreadImpl::BindHostReceiver(mojo::GenericPendingReceiver receiver) {
  child_process_host_->BindHostReceiver(std::move(receiver));
}

IPC::MessageRouter* ChildThreadImpl::GetRouter() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  return &router_;
}

mojom::RouteProvider* ChildThreadImpl::GetRemoteRouteProvider() {
  if (!remote_route_provider_) {
    DCHECK(channel_);
    channel_->GetRemoteAssociatedInterface(&remote_route_provider_);
  }
  return remote_route_provider_.get();
}

bool ChildThreadImpl::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return router_.OnMessageReceived(msg);
}

void ChildThreadImpl::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (interface_name == mojom::RouteProvider::Name_) {
    DCHECK(!route_provider_receiver_.is_bound());
    route_provider_receiver_.Bind(
        mojo::PendingAssociatedReceiver<mojom::RouteProvider>(
            std::move(handle)),
        ipc_task_runner_ ? ipc_task_runner_
                         : base::ThreadTaskRunnerHandle::Get());
  } else {
    LOG(ERROR) << "Receiver for unknown Channel-associated interface: "
               << interface_name;
  }
}

void ChildThreadImpl::ExposeInterfacesToBrowser(mojo::BinderMap binders) {
  // NOTE: Do not add new binders directly within this method. Instead, modify
  // the definition of |ExposeChildInterfacesToBrowser()|, ensuring security
  // review coverage.
  ExposeChildInterfacesToBrowser(GetIOTaskRunner(), &binders);

  ChildThreadImpl::GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&IOThreadState::ExposeInterfacesToBrowser,
                                io_thread_state_, std::move(binders)));
}

bool ChildThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  return false;
}

void ChildThreadImpl::GetBackgroundTracingAgentProvider(
    mojo::PendingReceiver<tracing::mojom::BackgroundTracingAgentProvider>
        receiver) {
  if (!background_tracing_agent_provider_) {
    background_tracing_agent_provider_ =
        std::make_unique<tracing::BackgroundTracingAgentProviderImpl>();
  }
  background_tracing_agent_provider_->AddBinding(std::move(receiver));
}

void ChildThreadImpl::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  DLOG(ERROR) << "Ignoring unhandled request to run service: " << service_name;
}

void ChildThreadImpl::BindServiceInterface(
    mojo::GenericPendingReceiver receiver) {
  DLOG(ERROR) << "Ignoring unhandled request to bind service interface: "
              << *receiver.interface_name();
}

void ChildThreadImpl::OnBindReceiver(mojo::GenericPendingReceiver receiver) {}

ChildThreadImpl* ChildThreadImpl::current() {
  return g_lazy_child_thread_impl_tls.Pointer()->Get();
}

void ChildThreadImpl::OnProcessFinalRelease() {
  if (on_channel_error_called_)
    return;

  quit_closure_.Run();
}

void ChildThreadImpl::EnsureConnected() {
  VLOG(0) << "ChildThreadImpl::EnsureConnected()";
  base::Process::TerminateCurrentProcessImmediately(0);
}

void ChildThreadImpl::GetRoute(
    int32_t routing_id,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
        receiver) {
  associated_interface_provider_receivers_.Add(this, std::move(receiver),
                                               routing_id);
}

void ChildThreadImpl::GetAssociatedInterface(
    const std::string& name,
    mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
        receiver) {
  int32_t routing_id =
      associated_interface_provider_receivers_.current_context();
  Listener* route = router_.GetRoute(routing_id);
  if (route)
    route->OnAssociatedInterfaceRequest(name, receiver.PassHandle());
}

bool ChildThreadImpl::IsInBrowserProcess() const {
  return static_cast<bool>(browser_process_io_runner_);
}

}  // namespace content
