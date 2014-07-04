// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_win.h"

#include <limits>
#include <sddl.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/base/auto_thread_task_runner.h"
// MIDL-generated declarations and definitions.
#include "remoting/host/chromoting_lib.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/win/host_service.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_session_process_delegate.h"
#include "remoting/host/win/wts_terminal_monitor.h"
#include "remoting/host/win/wts_terminal_observer.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::win::ScopedHandle;

namespace remoting {

namespace {

// The security descriptor of the daemon IPC endpoint. It gives full access
// to SYSTEM and denies access by anyone else.
const wchar_t kDaemonIpcSecurityDescriptor[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":("
        SDDL_ACCESS_ALLOWED L";;" SDDL_GENERIC_ALL L";;;" SDDL_LOCAL_SYSTEM
    L")";

// The command line parameters that should be copied from the service's command
// line to the desktop process.
const char* kCopiedSwitchNames[] = { switches::kV, switches::kVModule };

// The default screen dimensions for an RDP session.
const int kDefaultRdpScreenWidth = 1280;
const int kDefaultRdpScreenHeight = 768;

// RDC 6.1 (W2K8) supports dimensions of up to 4096x2048.
const int kMaxRdpScreenWidth = 4096;
const int kMaxRdpScreenHeight = 2048;

// The minimum effective screen dimensions supported by Windows are 800x600.
const int kMinRdpScreenWidth = 800;
const int kMinRdpScreenHeight = 600;

// Default dots per inch used by RDP is 96 DPI.
const int kDefaultRdpDpi = 96;

// The session attach notification should arrive within 30 seconds.
const int kSessionAttachTimeoutSeconds = 30;

// DesktopSession implementation which attaches to the host's physical console.
// Receives IPC messages from the desktop process, running in the console
// session, via |WorkerProcessIpcDelegate|, and monitors console session
// attach/detach events via |WtsConsoleObserer|.
class ConsoleSession : public DesktopSessionWin {
 public:
  // Same as DesktopSessionWin().
  ConsoleSession(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor);
  virtual ~ConsoleSession();

 protected:
  // DesktopSession overrides.
  virtual void SetScreenResolution(const ScreenResolution& resolution) OVERRIDE;

  // DesktopSessionWin overrides.
  virtual void InjectSas() OVERRIDE;

 private:
  scoped_ptr<SasInjector> sas_injector_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleSession);
};

// DesktopSession implementation which attaches to virtual RDP console.
// Receives IPC messages from the desktop process, running in the console
// session, via |WorkerProcessIpcDelegate|, and monitors console session
// attach/detach events via |WtsConsoleObserer|.
class RdpSession : public DesktopSessionWin {
 public:
  // Same as DesktopSessionWin().
  RdpSession(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor);
  virtual ~RdpSession();

  // Performs the part of initialization that can fail.
  bool Initialize(const ScreenResolution& resolution);

  // Mirrors IRdpDesktopSessionEventHandler.
  void OnRdpConnected();
  void OnRdpClosed();

 protected:
  // DesktopSession overrides.
  virtual void SetScreenResolution(const ScreenResolution& resolution) OVERRIDE;

  // DesktopSessionWin overrides.
  virtual void InjectSas() OVERRIDE;

 private:
  // An implementation of IRdpDesktopSessionEventHandler interface that forwards
  // notifications to the owning desktop session.
  class EventHandler : public IRdpDesktopSessionEventHandler {
   public:
    explicit EventHandler(base::WeakPtr<RdpSession> desktop_session);
    virtual ~EventHandler();

    // IUnknown interface.
    STDMETHOD_(ULONG, AddRef)() OVERRIDE;
    STDMETHOD_(ULONG, Release)() OVERRIDE;
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) OVERRIDE;

    // IRdpDesktopSessionEventHandler interface.
    STDMETHOD(OnRdpConnected)() OVERRIDE;
    STDMETHOD(OnRdpClosed)() OVERRIDE;

   private:
    ULONG ref_count_;

    // Points to the desktop session object receiving OnRdpXxx() notifications.
    base::WeakPtr<RdpSession> desktop_session_;

    // This class must be used on a single thread.
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(EventHandler);
  };

  // Used to create an RDP desktop session.
  base::win::ScopedComPtr<IRdpDesktopSession> rdp_desktop_session_;

  // Used to match |rdp_desktop_session_| with the session it is attached to.
  std::string terminal_id_;

  base::WeakPtrFactory<RdpSession> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RdpSession);
};

ConsoleSession::ConsoleSession(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor)
    : DesktopSessionWin(caller_task_runner, io_task_runner, daemon_process, id,
                        monitor) {
  StartMonitoring(WtsTerminalMonitor::kConsole);
}

ConsoleSession::~ConsoleSession() {
}

void ConsoleSession::SetScreenResolution(const ScreenResolution& resolution) {
  // Do nothing. The screen resolution of the console session is controlled by
  // the DesktopSessionAgent instance running in that session.
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
}

void ConsoleSession::InjectSas() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (!sas_injector_)
    sas_injector_ = SasInjector::Create();
  if (!sas_injector_->InjectSas())
    LOG(ERROR) << "Failed to inject Secure Attention Sequence.";
}

RdpSession::RdpSession(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor)
    : DesktopSessionWin(caller_task_runner, io_task_runner, daemon_process, id,
                        monitor),
      weak_factory_(this) {
}

RdpSession::~RdpSession() {
}

bool RdpSession::Initialize(const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Create the RDP wrapper object.
  HRESULT result = rdp_desktop_session_.CreateInstance(
      __uuidof(RdpDesktopSession));
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to create RdpSession object, 0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  ScreenResolution local_resolution = resolution;

  // If the screen resolution is not specified, use the default screen
  // resolution.
  if (local_resolution.IsEmpty()) {
    local_resolution = ScreenResolution(
        webrtc::DesktopSize(kDefaultRdpScreenWidth, kDefaultRdpScreenHeight),
        webrtc::DesktopVector(kDefaultRdpDpi, kDefaultRdpDpi));
  }

  // Get the screen dimensions assuming the default DPI.
  webrtc::DesktopSize host_size = local_resolution.ScaleDimensionsToDpi(
      webrtc::DesktopVector(kDefaultRdpDpi, kDefaultRdpDpi));

  // Make sure that the host resolution is within the limits supported by RDP.
  host_size = webrtc::DesktopSize(
      std::min(kMaxRdpScreenWidth,
               std::max(kMinRdpScreenWidth, host_size.width())),
      std::min(kMaxRdpScreenHeight,
               std::max(kMinRdpScreenHeight, host_size.height())));

  // Create an RDP session.
  base::win::ScopedComPtr<IRdpDesktopSessionEventHandler> event_handler(
      new EventHandler(weak_factory_.GetWeakPtr()));
  terminal_id_ = base::GenerateGUID();
  base::win::ScopedBstr terminal_id(base::UTF8ToUTF16(terminal_id_).c_str());
  result = rdp_desktop_session_->Connect(host_size.width(),
                                         host_size.height(),
                                         terminal_id,
                                         event_handler);
  if (FAILED(result)) {
    LOG(ERROR) << "RdpSession::Create() failed, 0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  return true;
}

void RdpSession::OnRdpConnected() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  StopMonitoring();
  StartMonitoring(terminal_id_);
}

void RdpSession::OnRdpClosed() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  TerminateSession();
}

void RdpSession::SetScreenResolution(const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // TODO(alexeypa): implement resize-to-client for RDP sessions here.
  // See http://crbug.com/137696.
  NOTIMPLEMENTED();
}

void RdpSession::InjectSas() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  rdp_desktop_session_->InjectSas();
}

RdpSession::EventHandler::EventHandler(
    base::WeakPtr<RdpSession> desktop_session)
    : ref_count_(0),
      desktop_session_(desktop_session) {
}

RdpSession::EventHandler::~EventHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (desktop_session_)
    desktop_session_->OnRdpClosed();
}

ULONG STDMETHODCALLTYPE RdpSession::EventHandler::AddRef() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return ++ref_count_;
}

ULONG STDMETHODCALLTYPE RdpSession::EventHandler::Release() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (--ref_count_ == 0) {
    delete this;
    return 0;
  }

  return ref_count_;
}

STDMETHODIMP RdpSession::EventHandler::QueryInterface(REFIID riid, void** ppv) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (riid == IID_IUnknown ||
      riid == IID_IRdpDesktopSessionEventHandler) {
    *ppv = static_cast<IRdpDesktopSessionEventHandler*>(this);
    AddRef();
    return S_OK;
  }

  *ppv = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP RdpSession::EventHandler::OnRdpConnected() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (desktop_session_)
    desktop_session_->OnRdpConnected();

  return S_OK;
}

STDMETHODIMP RdpSession::EventHandler::OnRdpClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!desktop_session_)
    return S_OK;

  base::WeakPtr<RdpSession> desktop_session = desktop_session_;
  desktop_session_.reset();
  desktop_session->OnRdpClosed();
  return S_OK;
}

} // namespace

// static
scoped_ptr<DesktopSession> DesktopSessionWin::CreateForConsole(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    const ScreenResolution& resolution) {
  scoped_ptr<ConsoleSession> session(new ConsoleSession(
      caller_task_runner, io_task_runner, daemon_process, id,
      HostService::GetInstance()));

  return session.PassAs<DesktopSession>();
}

// static
scoped_ptr<DesktopSession> DesktopSessionWin::CreateForVirtualTerminal(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    const ScreenResolution& resolution) {
  scoped_ptr<RdpSession> session(new RdpSession(
      caller_task_runner, io_task_runner, daemon_process, id,
      HostService::GetInstance()));
  if (!session->Initialize(resolution))
    return scoped_ptr<DesktopSession>();

  return session.PassAs<DesktopSession>();
}

DesktopSessionWin::DesktopSessionWin(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor)
    : DesktopSession(daemon_process, id),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      monitor_(monitor),
      monitoring_notifications_(false) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ReportElapsedTime("created");
}

DesktopSessionWin::~DesktopSessionWin() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  StopMonitoring();
}

void DesktopSessionWin::OnSessionAttachTimeout() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(ERROR) << "Session attach notification didn't arrived within "
             << kSessionAttachTimeoutSeconds << " seconds.";
  TerminateSession();
}

void DesktopSessionWin::StartMonitoring(const std::string& terminal_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!monitoring_notifications_);
  DCHECK(!session_attach_timer_.IsRunning());

  ReportElapsedTime("started monitoring");

  session_attach_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSessionAttachTimeoutSeconds),
      this, &DesktopSessionWin::OnSessionAttachTimeout);

  monitoring_notifications_ = true;
  monitor_->AddWtsTerminalObserver(terminal_id, this);
}

void DesktopSessionWin::StopMonitoring() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (monitoring_notifications_) {
    ReportElapsedTime("stopped monitoring");

    monitoring_notifications_ = false;
    monitor_->RemoveWtsTerminalObserver(this);
  }

  session_attach_timer_.Stop();
  OnSessionDetached();
}

void DesktopSessionWin::TerminateSession() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  StopMonitoring();

  // This call will delete |this| so it should be at the very end of the method.
  daemon_process()->CloseDesktopSession(id());
}

void DesktopSessionWin::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ReportElapsedTime("channel connected");

  // Obtain the handle of the desktop process. It will be passed to the network
  // process to use to duplicate handles of shared memory objects from
  // the desktop process.
  desktop_process_.Set(OpenProcess(PROCESS_DUP_HANDLE, false, peer_pid));
  if (!desktop_process_.IsValid()) {
    CrashDesktopProcess(FROM_HERE);
    return;
  }

  VLOG(1) << "IPC: daemon <- desktop (" << peer_pid << ")";
}

bool DesktopSessionWin::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopSessionWin, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopDaemonMsg_DesktopAttached,
                        OnDesktopSessionAgentAttached)
    IPC_MESSAGE_HANDLER(ChromotingDesktopDaemonMsg_InjectSas,
                        InjectSas)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    LOG(ERROR) << "Received unexpected IPC type: " << message.type();
    CrashDesktopProcess(FROM_HERE);
  }

  return handled;
}

void DesktopSessionWin::OnPermanentError(int exit_code) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  TerminateSession();
}

void DesktopSessionWin::OnSessionAttached(uint32 session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!launcher_);
  DCHECK(monitoring_notifications_);

  ReportElapsedTime("attached");

  // Launch elevated on Win8 to be able to inject Alt+Tab.
  bool launch_elevated = base::win::GetVersion() >= base::win::VERSION_WIN8;

  // Get the name of the executable to run. |kDesktopBinaryName| specifies
  // uiAccess="true" in it's manifest.
  base::FilePath desktop_binary;
  bool result;
  if (launch_elevated) {
    result = GetInstalledBinaryPath(kDesktopBinaryName, &desktop_binary);
  } else {
    result = GetInstalledBinaryPath(kHostBinaryName, &desktop_binary);
  }

  if (!result) {
    TerminateSession();
    return;
  }

  session_attach_timer_.Stop();

  scoped_ptr<CommandLine> target(new CommandLine(desktop_binary));
  target->AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeDesktop);
  // Copy the command line switches enabling verbose logging.
  target->CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                           kCopiedSwitchNames,
                           arraysize(kCopiedSwitchNames));

  // Create a delegate capable of launching a process in a different session.
  scoped_ptr<WtsSessionProcessDelegate> delegate(
      new WtsSessionProcessDelegate(io_task_runner_,
                                    target.Pass(),
                                    launch_elevated,
                                    base::WideToUTF8(
                                        kDaemonIpcSecurityDescriptor)));
  if (!delegate->Initialize(session_id)) {
    TerminateSession();
    return;
  }

  // Create a launcher for the desktop process, using the per-session delegate.
  launcher_.reset(new WorkerProcessLauncher(
      delegate.PassAs<WorkerProcessLauncher::Delegate>(), this));
}

void DesktopSessionWin::OnSessionDetached() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launcher_.reset();

  if (monitoring_notifications_) {
    ReportElapsedTime("detached");

    session_attach_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kSessionAttachTimeoutSeconds),
        this, &DesktopSessionWin::OnSessionAttachTimeout);
  }
}

void DesktopSessionWin::OnDesktopSessionAgentAttached(
      IPC::PlatformFileForTransit desktop_pipe) {
  if (!daemon_process()->OnDesktopSessionAgentAttached(id(),
                                                       desktop_process_,
                                                       desktop_pipe)) {
    CrashDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionWin::CrashDesktopProcess(
    const tracked_objects::Location& location) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launcher_->Crash(location);
}

void DesktopSessionWin::ReportElapsedTime(const std::string& event) {
  base::Time now = base::Time::Now();

  std::string passed;
  if (!last_timestamp_.is_null()) {
    passed = base::StringPrintf(", %.2fs passed",
                                (now - last_timestamp_).InSecondsF());
  }

  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  VLOG(1) << base::StringPrintf("session(%d): %s at %02d:%02d:%02d.%03d%s",
                                id(),
                                event.c_str(),
                                exploded.hour,
                                exploded.minute,
                                exploded.second,
                                exploded.millisecond,
                                passed.c_str());

  last_timestamp_ = now;
}

}  // namespace remoting
