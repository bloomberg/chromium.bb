// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_win.h"

#include <limits>
#include <sddl.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/ip_endpoint.h"
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
// line to the host process.
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
  void OnRdpConnected(const net::IPEndPoint& client_endpoint);
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
    STDMETHOD(OnRdpConnected)(byte* client_endpoint, long length) OVERRIDE;
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
  StartMonitoring(net::IPEndPoint());
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

  // DaemonProcess::CreateDesktopSession() verifies that the resolution is
  // valid.
  DCHECK(resolution.IsValid());

  ScreenResolution local_resolution = resolution;

  // If the screen resolution is not specified, use the default screen
  // resolution.
  if (local_resolution.IsEmpty()) {
    local_resolution.dimensions_.set(kDefaultRdpScreenWidth,
                                     kDefaultRdpScreenHeight);
    local_resolution.dpi_.set(kDefaultRdpDpi, kDefaultRdpDpi);
  }

  // Get the screen dimensions assuming the default DPI.
  SkISize host_size = local_resolution.ScaleDimensionsToDpi(
      SkIPoint::Make(kDefaultRdpDpi, kDefaultRdpDpi));

  // Make sure that the host resolution is within the limits supported by RDP.
  host_size = SkISize::Make(
      std::min(kMaxRdpScreenWidth,
               std::max(kMinRdpScreenWidth, host_size.width())),
      std::min(kMaxRdpScreenHeight,
               std::max(kMinRdpScreenHeight, host_size.height())));

  // Create an RDP session.
  base::win::ScopedComPtr<IRdpDesktopSessionEventHandler> event_handler(
      new EventHandler(weak_factory_.GetWeakPtr()));
  result = rdp_desktop_session_->Connect(host_size.width(),
                                         host_size.height(),
                                         event_handler);
  if (FAILED(result)) {
    LOG(ERROR) << "RdpSession::Create() failed, 0x"
               << std::hex << result << std::dec << ".";
    return false;
  }

  return true;
}

void RdpSession::OnRdpConnected(const net::IPEndPoint& client_endpoint) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  StopMonitoring();
  StartMonitoring(client_endpoint);
}

void RdpSession::OnRdpClosed() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  OnPermanentError();
}

void RdpSession::SetScreenResolution(const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // TODO(alexeypa): implement resize-to-client for RDP sessions here.
  // See http://crbug.com/137696.
  NOTIMPLEMENTED();
}

void RdpSession::InjectSas() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // TODO(alexeypa): implement SAS injection for RDP sessions here.
  // See http://crbug.com/137696.
  NOTIMPLEMENTED();
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

STDMETHODIMP RdpSession::EventHandler::OnRdpConnected(
    byte* client_endpoint,
    long length) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!desktop_session_)
    return S_OK;

  net::IPEndPoint endpoint;
  if (!endpoint.FromSockAddr(reinterpret_cast<sockaddr*>(client_endpoint),
                             length)) {
    LOG(ERROR) << "Failed to parse the endpoint passed to OnRdpConnected().";
    OnRdpClosed();
    return S_OK;
  }

  desktop_session_->OnRdpConnected(endpoint);
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
}

DesktopSessionWin::~DesktopSessionWin() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  StopMonitoring();
}

void DesktopSessionWin::OnSessionAttachTimeout() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  LOG(ERROR) << "Session attach notification didn't arrived within "
             << kSessionAttachTimeoutSeconds << " seconds.";
  OnPermanentError();
}

void DesktopSessionWin::StartMonitoring(
    const net::IPEndPoint& client_endpoint) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!monitoring_notifications_);
  DCHECK(!session_attach_timer_.IsRunning());

  session_attach_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kSessionAttachTimeoutSeconds),
      this, &DesktopSessionWin::OnSessionAttachTimeout);

  monitoring_notifications_ = true;
  monitor_->AddWtsTerminalObserver(client_endpoint, this);
}

void DesktopSessionWin::StopMonitoring() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (monitoring_notifications_) {
    monitoring_notifications_ = false;
    monitor_->RemoveWtsTerminalObserver(this);
  }

  session_attach_timer_.Stop();
  OnSessionDetached();
}

void DesktopSessionWin::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

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

void DesktopSessionWin::OnPermanentError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  StopMonitoring();

  // This call will delete |this| so it should be at the very end of the method.
  daemon_process()->CloseDesktopSession(id());
}

void DesktopSessionWin::OnSessionAttached(uint32 session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!launcher_);
  DCHECK(monitoring_notifications_);

  // Get the name of the executable the desktop process will run.
  base::FilePath desktop_binary;
  if (!GetInstalledBinaryPath(kDesktopBinaryName, &desktop_binary)) {
    OnPermanentError();
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
      new WtsSessionProcessDelegate(
          caller_task_runner_, io_task_runner_, target.Pass(), session_id, true,
          WideToUTF8(kDaemonIpcSecurityDescriptor)));

  // Create a launcher for the desktop process, using the per-session delegate.
  launcher_.reset(new WorkerProcessLauncher(
      caller_task_runner_, delegate.Pass(), this));
}

void DesktopSessionWin::OnSessionDetached() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launcher_.reset();

  if (monitoring_notifications_) {
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

}  // namespace remoting
