// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/wts_session_process_launcher.h"

#include <windows.h>
#include <sddl.h>
#include <limits>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/constants.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/wts_console_monitor.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// The minimum and maximum delays between attempts to inject host process into
// a session.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

const FilePath::CharType kMe2meHostBinaryName[] =
    FILE_PATH_LITERAL("remoting_me2me_host.exe");

// Match the pipe name prefix used by Chrome IPC channels.
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome.";

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

// Generates random channel ID.
// N.B. Stolen from src/content/common/child_process_host_impl.cc
std::string GenerateRandomChannelId(void* instance) {
  return base::StringPrintf("%d.%p.%d",
                            base::GetCurrentProcId(), instance,
                            base::RandInt(0, std::numeric_limits<int>::max()));
}

// Creates the server end of the Chromoting IPC channel.
// N.B. This code is based on IPC::Channel's implementation.
bool CreatePipeForIpcChannel(void* instance,
                             std::string* channel_name_out,
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
  std::string channel_name(GenerateRandomChannelId(instance));

  // Convert it to the pipe name.
  std::string pipe_name(kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  // Create the server end of the pipe. This code should match the code in
  // IPC::Channel with exception of passing a non-default security descriptor.
  HANDLE pipe = CreateNamedPipeW(UTF8ToUTF16(pipe_name).c_str(),
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

} // namespace

namespace remoting {

WtsSessionProcessLauncher::WtsSessionProcessLauncher(
    const base::Closure& stopped_callback,
    WtsConsoleMonitor* monitor,
    scoped_refptr<base::SingleThreadTaskRunner> main_message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_message_loop)
    : Stoppable(main_message_loop, stopped_callback),
      main_message_loop_(main_message_loop),
      ipc_message_loop_(ipc_message_loop),
      monitor_(monitor),
      state_(StateDetached) {
  monitor_->AddWtsConsoleObserver(this);
}

WtsSessionProcessLauncher::~WtsSessionProcessLauncher() {
  monitor_->RemoveWtsConsoleObserver(this);
  if (state_ != StateDetached) {
    OnSessionDetached();
  }

  DCHECK(state_ == StateDetached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() == NULL);
}

void WtsSessionProcessLauncher::LaunchProcess() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == StateStarting);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() == NULL);

  launch_time_ = base::Time::Now();

  // Construct the host binary name.
  FilePath dir_path;
  if (!PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    Stop();
    return;
  }
  FilePath host_binary = dir_path.Append(kMe2meHostBinaryName);

  std::string channel_name;
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
    CommandLine command_line(host_binary);
    command_line.AppendSwitchASCII(kChromotingIpcSwitchName, channel_name);
    command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                  kCopiedSwitchNames,
                                  _countof(kCopiedSwitchNames));

    // Try to launch the process and attach an object watcher to the returned
    // handle so that we get notified when the process terminates.
    if (LaunchProcessWithToken(host_binary,
                               command_line.GetCommandLineString(),
                               session_token_,
                               &process_)) {
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
      base::WaitForExitCodeWithTimeout(
          process_.handle(), &exit_code, base::TimeDelta()) &&
      kMinPermanentErrorExitCode <= exit_code &&
      exit_code <= kMaxPermanentErrorExitCode;

  // The host process has been terminated for some reason. The handle can now be
  // closed.
  process_.Close();
  chromoting_channel_.reset();
  state_ = StateStarting;

  if (stop_trying) {
    Stop();
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

  if (stoppable_state() != Stoppable::kRunning) {
    return;
  }

  DCHECK(state_ == StateDetached);
  DCHECK(!timer_.IsRunning());
  DCHECK(process_.handle() == NULL);
  DCHECK(process_watcher_.GetWatchedObject() == NULL);
  DCHECK(chromoting_channel_.get() == NULL);

  // Create a session token for the launched process.
  if (!CreateSessionToken(session_id, &session_token_))
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

  session_token_.Close();
}

void WtsSessionProcessLauncher::DoStop() {
  if (state_ != StateDetached) {
    OnSessionDetached();
  }

  CompleteStopping();
}

} // namespace remoting
