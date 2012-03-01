// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/wts_session_process_launcher_win.h"

#include <windows.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"

#include "remoting/host/wts_console_monitor_win.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// The minimum and maximum delays between attempts to inject host process into
// a session.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

// Name of the default session desktop.
const char kDefaultDesktopName[] = "winsta0\\default";

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
                         TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY;
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

// Launches |binary| in the security context of the supplied |user_token|.
bool LaunchProcessAsUser(const FilePath& binary,
                         HANDLE user_token,
                         base::Process* process_out) {
  string16 command_line = binary.value();
  string16 desktop = ASCIIToUTF16(kDefaultDesktopName);

  PROCESS_INFORMATION process_info;
  STARTUPINFOW startup_info;

  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.lpDesktop = const_cast<LPWSTR>(desktop.c_str());

  if (!CreateProcessAsUserW(user_token,
                            command_line.c_str(),
                            const_cast<LPWSTR>(command_line.c_str()),
                            NULL,
                            NULL,
                            FALSE,
                            0,
                            NULL,
                            NULL,
                            &startup_info,
                            &process_info)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to launch a process with a user token";
    return false;
  }

  CloseHandle(process_info.hThread);
  process_out->set_handle(process_info.hProcess);
  return true;
}

} // namespace

namespace remoting {

WtsSessionProcessLauncher::WtsSessionProcessLauncher(
    WtsConsoleMonitor* monitor,
    const FilePath& host_binary)
    : host_binary_(host_binary),
      monitor_(monitor),
      state_(StateDetached) {
  monitor_->AddWtsConsoleObserver(this);
}

WtsSessionProcessLauncher::~WtsSessionProcessLauncher() {
  DCHECK(state_ == StateDetached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  monitor_->RemoveWtsConsoleObserver(this);
}

void WtsSessionProcessLauncher::LaunchProcess() {
  DCHECK(state_ == StateStarting);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  // Try to launch the process and attach an object watcher to the returned
  // handle so that we get notified when the process terminates.
  launch_time_ = base::Time::Now();
  if (LaunchProcessAsUser(host_binary_, session_token_, &process_)) {
    if (process_watcher_.StartWatching(process_.handle(), this)) {
      state_ = StateAttached;
      return;
    } else {
      LOG(ERROR) << "Failed to arm the process watcher.";
      process_.Terminate(0);
      process_.Close();
    }
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
  DCHECK(state_ == StateAttached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() != NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  // The host process has been terminated for some reason. The handle can now be
  // closed.
  process_.Close();

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
  state_ = StateStarting;
  timer_.Start(FROM_HERE, launch_backoff_,
               this, &WtsSessionProcessLauncher::LaunchProcess);
}

void WtsSessionProcessLauncher::OnSessionAttached(uint32 session_id) {
  DCHECK(state_ == StateDetached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

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

  // While the SE_TCB_NAME progolege is enabled, create a session token for
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
  DCHECK(state_ == StateDetached ||
         state_ == StateStarting ||
         state_ == StateAttached);

  switch (state_) {
    case StateDetached:
      DCHECK(!timer_.IsRunning());
      DCHECK(process_.handle() == NULL);
      DCHECK(process_watcher_.GetWatchedObject() == NULL);
      break;

    case StateStarting:
      DCHECK(timer_.IsRunning());
      DCHECK(process_.handle() == NULL);
      DCHECK(process_watcher_.GetWatchedObject() == NULL);

      timer_.Stop();
      launch_backoff_ = base::TimeDelta();
      state_ = StateDetached;
      break;

    case StateAttached:
      DCHECK(!timer_.IsRunning());
      DCHECK(process_.handle() != NULL);
      DCHECK(process_watcher_.GetWatchedObject() != NULL);

      process_watcher_.StopWatching();
      process_.Terminate(0);
      process_.Close();
      state_ = StateDetached;
      break;
  }
}

} // namespace remoting
