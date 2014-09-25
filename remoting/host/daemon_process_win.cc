// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session_win.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/pairing_registry_delegate_win.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/unprivileged_process_delegate.h"
#include "remoting/host/win/worker_process_launcher.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// Duplicates |key| into |target_process| and returns the value that can be sent
// over IPC.
IPC::PlatformFileForTransit GetRegistryKeyForTransit(
    base::ProcessHandle target_process,
    const base::win::RegKey& key) {
  base::PlatformFile handle =
      reinterpret_cast<base::PlatformFile>(key.Handle());
  return IPC::GetFileHandleForProcess(handle, target_process, false);
}

}  // namespace

namespace remoting {

class WtsTerminalMonitor;

// The command line parameters that should be copied from the service's command
// line to the host process.
const char kEnableVp9SwitchName[] = "enable-vp9";
const char* kCopiedSwitchNames[] =
    { switches::kV, switches::kVModule, kEnableVp9SwitchName };

class DaemonProcessWin : public DaemonProcess {
 public:
  DaemonProcessWin(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);
  virtual ~DaemonProcessWin();

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnPermanentError(int exit_code) OVERRIDE;

  // DaemonProcess overrides.
  virtual void SendToNetwork(IPC::Message* message) OVERRIDE;
  virtual bool OnDesktopSessionAgentAttached(
      int terminal_id,
      base::ProcessHandle desktop_process,
      IPC::PlatformFileForTransit desktop_pipe) OVERRIDE;

 protected:
  // DaemonProcess implementation.
  virtual scoped_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) OVERRIDE;
  virtual void DoCrashNetworkProcess(
      const tracked_objects::Location& location) OVERRIDE;
  virtual void LaunchNetworkProcess() OVERRIDE;

  // Changes the service start type to 'manual'.
  void DisableAutoStart();

  // Initializes the pairing registry on the host side by sending
  // ChromotingDaemonNetworkMsg_InitializePairingRegistry message.
  bool InitializePairingRegistry();

  // Opens the pairing registry keys.
  bool OpenPairingRegistry();

 private:
  scoped_ptr<WorkerProcessLauncher> network_launcher_;

  // Handle of the network process.
  ScopedHandle network_process_;

  base::win::RegKey pairing_registry_privileged_key_;
  base::win::RegKey pairing_registry_unprivileged_key_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcessWin);
};

DaemonProcessWin::DaemonProcessWin(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(caller_task_runner, io_task_runner, stopped_callback) {
}

DaemonProcessWin::~DaemonProcessWin() {
}

void DaemonProcessWin::OnChannelConnected(int32 peer_pid) {
  // Obtain the handle of the network process.
  network_process_.Set(OpenProcess(PROCESS_DUP_HANDLE, false, peer_pid));
  if (!network_process_.IsValid()) {
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  if (!InitializePairingRegistry()) {
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DaemonProcess::OnChannelConnected(peer_pid);
}

void DaemonProcessWin::OnPermanentError(int exit_code) {
  // Change the service start type to 'manual' if the host has been deleted
  // remotely. This way the host will not be started every time the machine
  // boots until the user re-enable it again.
  if (exit_code == kInvalidHostIdExitCode)
    DisableAutoStart();

  DaemonProcess::OnPermanentError(exit_code);
}

void DaemonProcessWin::SendToNetwork(IPC::Message* message) {
  if (network_launcher_) {
    network_launcher_->Send(message);
  } else {
    delete message;
  }
}

bool DaemonProcessWin::OnDesktopSessionAgentAttached(
    int terminal_id,
    base::ProcessHandle desktop_process,
    IPC::PlatformFileForTransit desktop_pipe) {
  // Prepare |desktop_process| handle for sending over to the network process.
  base::ProcessHandle desktop_process_for_transit;
  if (!DuplicateHandle(GetCurrentProcess(),
                       desktop_process,
                       network_process_.Get(),
                       &desktop_process_for_transit,
                       0,
                       FALSE,
                       DUPLICATE_SAME_ACCESS)) {
    PLOG(ERROR) << "Failed to duplicate the desktop process handle";
    return false;
  }

  // |desktop_pipe| is a handle in the desktop process. It will be duplicated
  // by the network process directly from the desktop process.
  SendToNetwork(new ChromotingDaemonNetworkMsg_DesktopAttached(
      terminal_id, desktop_process_for_transit, desktop_pipe));
  return true;
}

scoped_ptr<DesktopSession> DaemonProcessWin::DoCreateDesktopSession(
    int terminal_id,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (virtual_terminal) {
    return DesktopSessionWin::CreateForVirtualTerminal(
        caller_task_runner(), io_task_runner(), this, terminal_id, resolution);
  } else {
    return DesktopSessionWin::CreateForConsole(
        caller_task_runner(), io_task_runner(), this, terminal_id, resolution);
  }
}

void DaemonProcessWin::DoCrashNetworkProcess(
    const tracked_objects::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  network_launcher_->Crash(location);
}

void DaemonProcessWin::LaunchNetworkProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!network_launcher_);

  // Construct the host binary name.
  base::FilePath host_binary;
  if (!GetInstalledBinaryPath(kHostBinaryName, &host_binary)) {
    Stop();
    return;
  }

  scoped_ptr<CommandLine> target(new CommandLine(host_binary));
  target->AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeHost);
  target->CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                           kCopiedSwitchNames,
                           arraysize(kCopiedSwitchNames));

  scoped_ptr<UnprivilegedProcessDelegate> delegate(
      new UnprivilegedProcessDelegate(io_task_runner(), target.Pass()));
  network_launcher_.reset(new WorkerProcessLauncher(
      delegate.PassAs<WorkerProcessLauncher::Delegate>(), this));
}

scoped_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback) {
  scoped_ptr<DaemonProcessWin> daemon_process(
      new DaemonProcessWin(caller_task_runner, io_task_runner,
                           stopped_callback));
  daemon_process->Initialize();
  return daemon_process.PassAs<DaemonProcess>();
}

void DaemonProcessWin::DisableAutoStart() {
  ScopedScHandle scmanager(
      OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE,
                    SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    PLOG(INFO) << "Failed to connect to the service control manager";
    return;
  }

  DWORD desired_access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS;
  ScopedScHandle service(
      OpenService(scmanager.Get(), kWindowsServiceName, desired_access));
  if (!service.IsValid()) {
    PLOG(INFO) << "Failed to open to the '" << kWindowsServiceName
               << "' service";
    return;
  }

  // Change the service start type to 'manual'. All |NULL| parameters below mean
  // that there is no change to the corresponding service parameter.
  if (!ChangeServiceConfig(service.Get(),
                           SERVICE_NO_CHANGE,
                           SERVICE_DEMAND_START,
                           SERVICE_NO_CHANGE,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL,
                           NULL)) {
    PLOG(INFO) << "Failed to change the '" << kWindowsServiceName
               << "'service start type to 'manual'";
  }
}

bool DaemonProcessWin::InitializePairingRegistry() {
  if (!pairing_registry_privileged_key_.Valid()) {
    if (!OpenPairingRegistry())
      return false;
  }

  // Duplicate handles to the network process.
  IPC::PlatformFileForTransit privileged_key = GetRegistryKeyForTransit(
      network_process_.Get(), pairing_registry_privileged_key_);
  IPC::PlatformFileForTransit unprivileged_key = GetRegistryKeyForTransit(
      network_process_.Get(), pairing_registry_unprivileged_key_);
  if (!(privileged_key && unprivileged_key))
    return false;

  // Initialize the pairing registry in the network process. This has to be done
  // before the host configuration is sent, otherwise the host will not use
  // the passed handles.
  SendToNetwork(new ChromotingDaemonNetworkMsg_InitializePairingRegistry(
      privileged_key, unprivileged_key));
  return true;
}

bool DaemonProcessWin::OpenPairingRegistry() {
  DCHECK(!pairing_registry_privileged_key_.Valid());
  DCHECK(!pairing_registry_unprivileged_key_.Valid());

  // Open the root of the pairing registry.
  base::win::RegKey root;
  LONG result = root.Open(HKEY_LOCAL_MACHINE, kPairingRegistryKeyName,
                          KEY_READ);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM\\" << kPairingRegistryKeyName;
    return false;
  }

  base::win::RegKey privileged;
  result = privileged.Open(root.Handle(), kPairingRegistryClientsKeyName,
                           KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM\\" << kPairingRegistryKeyName << "\\"
                << kPairingRegistryClientsKeyName;
    return false;
  }

  base::win::RegKey unprivileged;
  result = unprivileged.Open(root.Handle(), kPairingRegistrySecretsKeyName,
                             KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM\\" << kPairingRegistrySecretsKeyName
                << "\\" << kPairingRegistrySecretsKeyName;
    return false;
  }

  pairing_registry_privileged_key_.Set(privileged.Take());
  pairing_registry_unprivileged_key_.Set(unprivileged.Take());
  return true;
}

}  // namespace remoting
