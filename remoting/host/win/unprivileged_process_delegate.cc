
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/unprivileged_process_delegate.h"

#include <sddl.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "remoting/base/typed_buffer.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/security_descriptor.h"
#include "remoting/host/win/window_station_and_desktop.h"
#include "sandbox/win/src/restricted_token.h"

using base::win::ScopedHandle;

namespace remoting {

namespace {

// The security descriptors below are used to lock down access to the worker
// process launched by UnprivilegedProcessDelegate. UnprivilegedProcessDelegate
// assumes that it runs under SYSTEM. The worker process is launched under
// a different account and attaches to a newly created window station. If UAC is
// supported by the OS, the worker process is started at low integrity level.
// UnprivilegedProcessDelegate replaces the first printf parameter in
// the strings below by the logon SID assigned to the worker process.

// Security descriptor used to protect the named pipe in between
// CreateNamedPipe() and CreateFile() calls before it is passed to the network
// process. It gives full access to LocalSystem and denies access by anyone
// else.
const char kDaemonIpcSd[] = "O:SYG:SYD:(A;;GA;;;SY)";

// Security descriptor of the desktop the worker process attaches to. It gives
// SYSTEM and the logon SID full access to the desktop.
const char kDesktopSdFormat[] = "O:SYG:SYD:(A;;0xf01ff;;;SY)(A;;0xf01ff;;;%s)";

// Mandatory label specifying low integrity level.
const char kLowIntegrityMandatoryLabel[] = "S:(ML;CIOI;NW;;;LW)";

// Security descriptor of the window station the worker process attaches to. It
// gives SYSTEM and the logon SID full access the window station. The child
// containers and objects inherit ACE giving SYSTEM and the logon SID full
// access to them as well.
const char kWindowStationSdFormat[] = "O:SYG:SYD:(A;CIOIIO;GA;;;SY)"
    "(A;CIOIIO;GA;;;%1$s)(A;NP;0xf037f;;;SY)(A;NP;0xf037f;;;%1$s)";

// Security descriptor of the worker process. It gives access SYSTEM full access
// to the process. It gives READ_CONTROL, SYNCHRONIZE, PROCESS_QUERY_INFORMATION
// and PROCESS_TERMINATE rights to the built-in administrators group.
const char kWorkerProcessSd[] = "O:SYG:SYD:(A;;GA;;;SY)(A;;0x120401;;;BA)";

// Security descriptor of the worker process threads. It gives access SYSTEM
// full access to the threads. It gives READ_CONTROL, SYNCHRONIZE,
// THREAD_QUERY_INFORMATION and THREAD_TERMINATE rights to the built-in
// administrators group.
const char kWorkerThreadSd[] = "O:SYG:SYD:(A;;GA;;;SY)(A;;0x120801;;;BA)";

// Creates a token with limited access that will be used to run the worker
// process.
bool CreateRestrictedToken(ScopedHandle* token_out) {
  // Create a token representing LocalService account.
  ScopedHandle token;
  if (!LogonUser(L"LocalService", L"NT AUTHORITY", NULL, LOGON32_LOGON_SERVICE,
                 LOGON32_PROVIDER_DEFAULT, token.Receive())) {
    return false;
  }

  sandbox::RestrictedToken restricted_token;
  if (restricted_token.Init(token) != ERROR_SUCCESS)
    return false;

  // Remove all privileges in the token.
  if (restricted_token.DeleteAllPrivileges(NULL) != ERROR_SUCCESS)
    return false;

  // Set low integrity level if supported by the OS.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    if (restricted_token.SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW)
        != ERROR_SUCCESS) {
      return false;
    }
  }

  // Return the resulting token.
  return restricted_token.GetRestrictedTokenHandle(token_out->Receive()) ==
      ERROR_SUCCESS;
}

// Creates a window station with a given name and the default desktop giving
// the complete access to |logon_sid|.
bool CreateWindowStationAndDesktop(ScopedSid logon_sid,
                                   WindowStationAndDesktop* handles_out) {
  // Convert the logon SID into a string.
  std::string logon_sid_string = ConvertSidToString(logon_sid.get());
  if (logon_sid_string.empty()) {
    LOG_GETLASTERROR(ERROR) << "Failed to convert a SID to string";
    return false;
  }

  // Format the security descriptors in SDDL form.
  std::string desktop_sddl =
      base::StringPrintf(kDesktopSdFormat, logon_sid_string.c_str());
  std::string window_station_sddl =
      base::StringPrintf(kWindowStationSdFormat, logon_sid_string.c_str());

  // The worker runs at low integrity level. Make sure it will be able to attach
  // to the window station and desktop.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    desktop_sddl += kLowIntegrityMandatoryLabel;
    window_station_sddl += kLowIntegrityMandatoryLabel;
  }

  // Create the desktop and window station security descriptors.
  ScopedSd desktop_sd = ConvertSddlToSd(desktop_sddl);
  ScopedSd window_station_sd = ConvertSddlToSd(window_station_sddl);
  if (!desktop_sd || !window_station_sd) {
    LOG_GETLASTERROR(ERROR) << "Failed to create a security descriptor.";
    return false;
  }

  // GetProcessWindowStation() returns the current handle which does not need to
  // be freed.
  HWINSTA current_window_station = GetProcessWindowStation();

  // Generate a unique window station name.
  std::string window_station_name = base::StringPrintf(
      "chromoting-%d-%d",
      base::GetCurrentProcId(),
      base::RandInt(1, std::numeric_limits<int>::max()));

  // Make sure that a new window station will be created instead of opening
  // an existing one.
  DWORD window_station_flags = 0;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    window_station_flags = CWF_CREATE_ONLY;

  // Request full access because this handle will be inherited by the worker
  // process which needs full access in order to attach to the window station.
  DWORD desired_access =
      WINSTA_ALL_ACCESS | DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER;

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = window_station_sd.get();
  security_attributes.bInheritHandle = TRUE;

  WindowStationAndDesktop handles;
  handles.SetWindowStation(CreateWindowStation(
      UTF8ToUTF16(window_station_name).c_str(), window_station_flags,
      desired_access, &security_attributes));
  if (!handles.window_station()) {
    LOG_GETLASTERROR(ERROR) << "CreateWindowStation() failed";
    return false;
  }

  // Switch to the new window station and create a desktop on it.
  if (!SetProcessWindowStation(handles.window_station())) {
    LOG_GETLASTERROR(ERROR) << "SetProcessWindowStation() failed";
    return false;
  }

  desired_access = DESKTOP_READOBJECTS | DESKTOP_CREATEWINDOW |
      DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD |
      DESKTOP_JOURNALPLAYBACK | DESKTOP_ENUMERATE | DESKTOP_WRITEOBJECTS |
      DESKTOP_SWITCHDESKTOP | DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER;

  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = desktop_sd.get();
  security_attributes.bInheritHandle = TRUE;

  // The default desktop of the interactive window station is called "Default".
  // Name the created desktop the same way in case any code relies on that.
  // The desktop name should not make any difference though.
  handles.SetDesktop(CreateDesktop(L"Default", NULL, NULL, 0, desired_access,
                                   &security_attributes));

  // Switch back to the original window station.
  if (!SetProcessWindowStation(current_window_station)) {
    LOG_GETLASTERROR(ERROR) << "SetProcessWindowStation() failed";
    return false;
  }

  if (!handles.desktop()) {
    LOG_GETLASTERROR(ERROR) << "CreateDesktop() failed";
    return false;
  }

  handles.Swap(*handles_out);
  return true;
}

}  // namespace

UnprivilegedProcessDelegate::UnprivilegedProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<CommandLine> target_command)
    : main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner),
      target_command_(target_command.Pass()) {
}

UnprivilegedProcessDelegate::~UnprivilegedProcessDelegate() {
  KillProcess(CONTROL_C_EXIT);
}

bool UnprivilegedProcessDelegate::Send(IPC::Message* message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  return channel_->Send(message);
}

DWORD UnprivilegedProcessDelegate::GetProcessId() const {
  if (worker_process_.IsValid()) {
    return ::GetProcessId(worker_process_);
  } else {
    return 0;
  }
}

bool UnprivilegedProcessDelegate::IsPermanentError(int failure_count) const {
  // Get exit code of the worker process if it is available.
  DWORD exit_code = CONTROL_C_EXIT;
  if (worker_process_.IsValid()) {
    if (!::GetExitCodeProcess(worker_process_, &exit_code)) {
      LOG_GETLASTERROR(INFO)
          << "Failed to query the exit code of the worker process";
      exit_code = CONTROL_C_EXIT;
    }
  }

  // Stop trying to restart the worker process if it exited due to
  // misconfiguration.
  return (kMinPermanentErrorExitCode <= exit_code &&
      exit_code <= kMaxPermanentErrorExitCode);
}

void UnprivilegedProcessDelegate::KillProcess(DWORD exit_code) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  channel_.reset();

  if (worker_process_.IsValid()) {
    TerminateProcess(worker_process_, exit_code);
  }
}

bool UnprivilegedProcessDelegate::LaunchProcess(
    IPC::Listener* delegate,
    ScopedHandle* process_exit_event_out) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  scoped_ptr<IPC::ChannelProxy> server;

  // Generate a unique name for the channel.
  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();

  // Create a restricted token that will be used to run the worker process.
  ScopedHandle token;
  if (!CreateRestrictedToken(&token)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to create a restricted LocalService token";
    return false;
  }

  // Determine our logon SID, so we can grant it access to our window station
  // and desktop.
  ScopedSid logon_sid = GetLogonSid(token);
  if (!logon_sid) {
    LOG_GETLASTERROR(ERROR) << "Failed to retrieve the logon SID";
    return false;
  }

  // Create the process and thread security descriptors.
  ScopedSd process_sd = ConvertSddlToSd(kWorkerProcessSd);
  ScopedSd thread_sd = ConvertSddlToSd(kWorkerThreadSd);
  if (!process_sd || !thread_sd) {
    LOG_GETLASTERROR(ERROR) << "Failed to create a security descriptor";
    return false;
  }

  SECURITY_ATTRIBUTES process_attributes;
  process_attributes.nLength = sizeof(process_attributes);
  process_attributes.lpSecurityDescriptor = process_sd.get();
  process_attributes.bInheritHandle = FALSE;

  SECURITY_ATTRIBUTES thread_attributes;
  thread_attributes.nLength = sizeof(thread_attributes);
  thread_attributes.lpSecurityDescriptor = thread_sd.get();
  thread_attributes.bInheritHandle = FALSE;

  {
    // Take a lock why any inheritable handles are open to make sure that only
    // one process inherits them.
    base::AutoLock lock(g_inherit_handles_lock.Get());

    // Create a connected IPC channel.
    ScopedHandle client;
    if (!CreateConnectedIpcChannel(channel_name, kDaemonIpcSd, io_task_runner_,
                                   delegate, &client, &server)) {
      return false;
    }

    // Convert the handle value into a decimal integer. Handle values are 32bit
    // even on 64bit platforms.
    std::string pipe_handle = base::StringPrintf(
        "%d", reinterpret_cast<ULONG_PTR>(client.Get()));

    // Pass the IPC channel via the command line.
    CommandLine command_line(target_command_->argv());
    command_line.AppendSwitchASCII(kDaemonPipeSwitchName, pipe_handle);

    // Create our own window station and desktop accessible by |logon_sid|.
    WindowStationAndDesktop handles;
    if (!CreateWindowStationAndDesktop(logon_sid.Pass(), &handles)) {
      LOG_GETLASTERROR(ERROR)
          << "Failed to create a window station and desktop";
      return false;
    }

    // Try to launch the worker process. The launched process inherits
    // the window station, desktop and pipe handles, created above.
    ScopedHandle worker_thread;
    worker_process_.Close();
    if (!LaunchProcessWithToken(command_line.GetProgram(),
                                command_line.GetCommandLineString(),
                                token,
                                &process_attributes,
                                &thread_attributes,
                                true,
                                0,
                                NULL,
                                &worker_process_,
                                &worker_thread)) {
      return false;
    }
  }

  // Return a handle that the caller can wait on to get notified when
  // the process terminates.
  ScopedHandle process_exit_event;
  if (!DuplicateHandle(GetCurrentProcess(),
                       worker_process_,
                       GetCurrentProcess(),
                       process_exit_event.Receive(),
                       SYNCHRONIZE,
                       FALSE,
                       0)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate a handle";
    KillProcess(CONTROL_C_EXIT);
    return false;
  }

  channel_ = server.Pass();
  *process_exit_event_out = process_exit_event.Pass();
  return true;
}

} // namespace remoting
