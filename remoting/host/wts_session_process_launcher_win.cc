// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/wts_session_process_launcher_win.h"

#include <windows.h>
#include <sddl.h>
#include <limits>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/constants.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/wts_console_monitor_win.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// The minimum and maximum delays between attempts to inject host process into
// a session.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

// Name of the default session desktop.
wchar_t kDefaultDesktopName[] = L"winsta0\\default";

// Match the pipe name prefix used by Chrome IPC channels.
const wchar_t kChromePipeNamePrefix[] = L"\\\\.\\pipe\\chrome.";

// The IPC channel name is passed to the host in the command line.
const char kChromotingIpcSwitchName[] = "chromoting-ipc";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = {
    "auth-config", "host-config", switches::kV, switches::kVModule };

// The security descriptor of the Chromoting IPC channel. It gives full access
// to LocalSystem and denies access by anyone else.
const wchar_t kChromotingChannelSecurityDescriptor[] =
    L"O:SYG:SYD:(A;;GA;;;SY)";

// Takes the process token and makes a copy of it. The returned handle will have
// |desired_access| rights.
bool CopyProcessToken(DWORD desired_access,
                      ScopedHandle* token_out) {

  HANDLE handle;
  if (!OpenProcessToken(GetCurrentProcess(),
                        TOKEN_DUPLICATE | desired_access,
                        &handle)) {
    LOG_GETLASTERROR(ERROR) << "Failed to open process token";
    return false;
  }

  ScopedHandle process_token(handle);

  if (!DuplicateTokenEx(process_token,
                        desired_access,
                        NULL,
                        SecurityImpersonation,
                        TokenPrimary,
                        &handle)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate the process token";
    return false;
  }

  token_out->Set(handle);
  return true;
}

// Creates a copy of the current process with SE_TCB_NAME privilege enabled.
bool CreatePrivilegedToken(ScopedHandle* token_out) {
  ScopedHandle privileged_token;
  DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
                         TOKEN_DUPLICATE | TOKEN_QUERY;
  if (!CopyProcessToken(desired_access, &privileged_token)) {
    return false;
  }

  // Get the LUID for the SE_TCB_NAME privilege.
  TOKEN_PRIVILEGES state;
  state.PrivilegeCount = 1;
  state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (!LookupPrivilegeValue(NULL, SE_TCB_NAME, &state.Privileges[0].Luid)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to lookup the LUID for the SE_TCB_NAME privilege";
    return false;
  }

  // Enable the SE_TCB_NAME privilege.
  if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, NULL, 0)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to enable SE_TCB_NAME privilege in a token";
    return false;
  }

  token_out->Set(privileged_token.Take());
  return true;
}

// Creates a copy of the current process token for the given |session_id| so
// it can be used to launch a process in that session.
bool CreateSessionToken(uint32 session_id,
                        ScopedHandle* token_out) {

  ScopedHandle session_token;
  DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
                         TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;
  if (!CopyProcessToken(desired_access, &session_token)) {
    return false;
  }

  // Change the session ID of the token.
  DWORD new_session_id = session_id;
  if (!SetTokenInformation(session_token,
                           TokenSessionId,
                           &new_session_id,
                           sizeof(new_session_id))) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to change session ID of a token";
    return false;
  }

  token_out->Set(session_token.Take());
  return true;
}

// Generates random channel ID.
// N.B. Stolen from src/content/common/child_process_host_impl.cc
std::wstring GenerateRandomChannelId(void* instance) {
  return base::StringPrintf(L"%d.%p.%d",
                            base::GetCurrentProcId(), instance,
                            base::RandInt(0, std::numeric_limits<int>::max()));
}

// Creates the server end of the Chromoting IPC channel.
// N.B. This code is based on IPC::Channel's implementation.
bool CreatePipeForIpcChannel(void* instance,
                             std::wstring* channel_name_out,
                             ScopedHandle* pipe_out) {
  // Create security descriptor for the channel.
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = FALSE;

  ULONG security_descriptor_length = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
           kChromotingChannelSecurityDescriptor,
           SDDL_REVISION_1,
           reinterpret_cast<PSECURITY_DESCRIPTOR*>(
               &security_attributes.lpSecurityDescriptor),
           &security_descriptor_length)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create a security descriptor for the Chromoting IPC channel";
    return false;
  }

  // Generate a random channel name.
  std::wstring channel_name(GenerateRandomChannelId(instance));

  // Convert it to the pipe name.
  std::wstring pipe_name(kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  // Create the server end of the pipe. This code should match the code in
  // IPC::Channel with exception of passing a non-default security descriptor.
  HANDLE pipe = CreateNamedPipeW(pipe_name.c_str(),
                                 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                     FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                                 1,
                                 IPC::Channel::kReadBufferSize,
                                 IPC::Channel::kReadBufferSize,
                                 5000,
                                 &security_attributes);
  if (pipe == INVALID_HANDLE_VALUE) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create the server end of the Chromoting IPC channel";
    LocalFree(security_attributes.lpSecurityDescriptor);
    return false;
  }

  LocalFree(security_attributes.lpSecurityDescriptor);

  *channel_name_out = channel_name;
  pipe_out->Set(pipe);
  return true;
}

// Launches |binary| in the security context of the supplied |user_token|.
bool LaunchProcessAsUser(const FilePath& binary,
                         const std::wstring& command_line,
                         HANDLE user_token,
                         base::Process* process_out) {
  std::wstring application_name = binary.value();

  base::win::ScopedProcessInformation process_info;
  STARTUPINFOW startup_info;

  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.lpDesktop = kDefaultDesktopName;

  if (!CreateProcessAsUserW(user_token,
                            application_name.c_str(),
                            const_cast<LPWSTR>(command_line.c_str()),
                            NULL,
                            NULL,
                            FALSE,
                            0,
                            NULL,
                            NULL,
                            &startup_info,
                            process_info.Receive())) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to launch a process with a user token";
    return false;
  }

  process_out->set_handle(process_info.TakeProcessHandle());
  return true;
}

} // namespace

namespace remoting {

WtsSessionProcessLauncher::WtsSessionProcessLauncher(
    WtsConsoleMonitor* monitor,
    const FilePath& host_binary,
    scoped_refptr<base::MessageLoopProxy> main_message_loop,
    scoped_refptr<base::MessageLoopProxy> ipc_message_loop)
    : host_binary_(host_binary),
      main_message_loop_(main_message_loop),
      ipc_message_loop_(ipc_message_loop),
      monitor_(monitor),
      state_(StateDetached) {
  monitor_->AddWtsConsoleObserver(this);
}

WtsSessionProcessLauncher::~WtsSessionProcessLauncher() {
  DCHECK(state_ == StateDetached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() == NULL);
  if (monitor_ != NULL) {
    monitor_->RemoveWtsConsoleObserver(this);
  }
}

void WtsSessionProcessLauncher::LaunchProcess() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == StateStarting);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() == NULL);

  launch_time_ = base::Time::Now();

  std::wstring channel_name;
  ScopedHandle pipe;
  if (CreatePipeForIpcChannel(this, &channel_name, &pipe)) {
    // Wrap the pipe into an IPC channel.
    chromoting_channel_.reset(new IPC::ChannelProxy(
        IPC::ChannelHandle(pipe.Get()),
        IPC::Channel::MODE_SERVER,
        this,
        ipc_message_loop_));

    // Create the host process command line passing the name of the IPC channel
    // to use and copying known switches from the service's command line.
    CommandLine command_line(host_binary_);
    command_line.AppendSwitchNative(kChromotingIpcSwitchName, channel_name);
    command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                  kCopiedSwitchNames,
                                  _countof(kCopiedSwitchNames));

    // Try to launch the process and attach an object watcher to the returned
    // handle so that we get notified when the process terminates.
    if (LaunchProcessAsUser(host_binary_, command_line.GetCommandLineString(),
                            session_token_, &process_)) {
      if (process_watcher_.StartWatching(process_.handle(), this)) {
        state_ = StateAttached;
        return;
      } else {
        LOG(ERROR) << "Failed to arm the process watcher.";
        process_.Terminate(0);
        process_.Close();
      }
    }

    chromoting_channel_.reset();
  }

  // Something went wrong. Try to launch the host again later. The attempts rate
  // is limited by exponential backoff.
  launch_backoff_ = std::max(launch_backoff_ * 2,
                             TimeDelta::FromSeconds(kMinLaunchDelaySeconds));
  launch_backoff_ = std::min(launch_backoff_,
                             TimeDelta::FromSeconds(kMaxLaunchDelaySeconds));
  timer_.Start(FROM_HERE, launch_backoff_,
               this, &WtsSessionProcessLauncher::LaunchProcess);
}

void WtsSessionProcessLauncher::OnObjectSignaled(HANDLE object) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&WtsSessionProcessLauncher::OnObjectSignaled,
                              base::Unretained(this), object));
    return;
  }

  // It is possible that OnObjectSignaled() task will be queued by another
  // thread right before |process_watcher_| was stopped. It such a case it is
  // safe to ignore this notification.
  if (state_ != StateAttached) {
    return;
  }

  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() != NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() != NULL);

  // Stop trying to restart the host if its process exited due to
  // misconfiguration.
  int exit_code;
  bool stop_trying =
      base::WaitForExitCodeWithTimeout(process_.handle(), &exit_code, 0) &&
      kMinPermanentErrorExitCode <= exit_code &&
      exit_code <= kMaxPermanentErrorExitCode;

  // The host process has been terminated for some reason. The handle can now be
  // closed.
  process_.Close();
  chromoting_channel_.reset();
  state_ = StateStarting;

  if (stop_trying) {
    OnSessionDetached();

    // N.B. The service will stop once the last observer is removed from
    // the list.
    monitor_->RemoveWtsConsoleObserver(this);
    monitor_ = NULL;
    return;
  }

  // Expand the backoff interval if the process has died quickly or reset it if
  // it was up longer than the maximum backoff delay.
  base::TimeDelta delta = base::Time::Now() - launch_time_;
  if (delta < base::TimeDelta() ||
      delta >= base::TimeDelta::FromSeconds(kMaxLaunchDelaySeconds)) {
    launch_backoff_ = base::TimeDelta();
  } else {
    launch_backoff_ = std::max(launch_backoff_ * 2,
                               TimeDelta::FromSeconds(kMinLaunchDelaySeconds));
    launch_backoff_ = std::min(launch_backoff_,
                               TimeDelta::FromSeconds(kMaxLaunchDelaySeconds));
  }

  // Try to restart the host.
  timer_.Start(FROM_HERE, launch_backoff_,
               this, &WtsSessionProcessLauncher::LaunchProcess);
}

bool WtsSessionProcessLauncher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WtsSessionProcessLauncher, message)
      IPC_MESSAGE_HANDLER(ChromotingHostMsg_SendSasToConsole,
                          OnSendSasToConsole)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WtsSessionProcessLauncher::OnSendSasToConsole() {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&WtsSessionProcessLauncher::OnSendSasToConsole,
                              base::Unretained(this)));
    return;
  }

  if (state_ == StateAttached) {
    if (sas_injector_.get() == NULL) {
      sas_injector_ = SasInjector::Create();
    }

    if (sas_injector_.get() != NULL) {
      sas_injector_->InjectSas();
    }
  }
}

void WtsSessionProcessLauncher::OnSessionAttached(uint32 session_id) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == StateDetached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() == NULL);

  // Temporarily enable the SE_TCB_NAME privilege. The privileged token is
  // created as needed and kept for later reuse.
  if (privileged_token_.Get() == NULL) {
    if (!CreatePrivilegedToken(&privileged_token_)) {
      return;
    }
  }

  if (!ImpersonateLoggedOnUser(privileged_token_)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to impersonate the privileged token";
    return;
  }

  // While the SE_TCB_NAME privilege is enabled, create a session token for
  // the launched process.
  bool result = CreateSessionToken(session_id, &session_token_);

  // Revert to the default token. The default token is sufficient to call
  // CreateProcessAsUser() successfully.
  CHECK(RevertToSelf());

  if (!result)
    return;

  // Now try to launch the host.
  state_ = StateStarting;
  LaunchProcess();
}

void WtsSessionProcessLauncher::OnSessionDetached() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == StateDetached ||
         state_ == StateStarting ||
         state_ == StateAttached);

  switch (state_) {
    case StateDetached:
      DCHECK(!timer_.IsRunning());
      DCHECK(process_.handle() == NULL);
      DCHECK(process_watcher_.GetWatchedObject() == NULL);
      DCHECK(chromoting_channel_.get() == NULL);
      break;

    case StateStarting:
      DCHECK(process_.handle() == NULL);
      DCHECK(process_watcher_.GetWatchedObject() == NULL);
      DCHECK(chromoting_channel_.get() == NULL);

      timer_.Stop();
      launch_backoff_ = base::TimeDelta();
      state_ = StateDetached;
      break;

    case StateAttached:
      DCHECK(!timer_.IsRunning());
      DCHECK(process_.handle() != NULL);
      DCHECK(process_watcher_.GetWatchedObject() != NULL);
      DCHECK(chromoting_channel_.get() != NULL);

      process_watcher_.StopWatching();
      process_.Terminate(0);
      process_.Close();
      chromoting_channel_.reset();
      state_ = StateDetached;
      break;
  }
}

} // namespace remoting
