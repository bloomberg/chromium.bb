// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/unprivileged_process_delegate.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/ipc_consts.h"
#include "remoting/host/win/launch_process_with_token.h"

using base::win::ScopedHandle;

namespace {

// The security descriptor used to protect the named pipe in between
// CreateNamedPipe() and CreateFile() calls before it will be passed to
// the network process. It gives full access to LocalSystem and denies access by
// anyone else.
const char kDaemonIpcSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = {
    "host-config", switches::kV, switches::kVModule };

// Creates an already connected IPC channel. The server end of the channel
// is wrapped into a channel proxy that will invoke methods of |delegate|
// on the caller's thread while using |io_task_runner| to send and receive
// messages in the background. The client end is returned as an inheritable NT
// handle.
bool CreateConnectedIpcChannel(
    const std::string& channel_name,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Listener* delegate,
    base::win::ScopedHandle* client_out,
    scoped_ptr<IPC::ChannelProxy>* server_out,
    base::win::ScopedHandle* pipe_out) {
  // Create the server end of the channel.
  ScopedHandle pipe;
  if (!remoting::CreateIpcChannel(channel_name, kDaemonIpcSecurityDescriptor,
                                  &pipe)) {
    return false;
  }

  // Wrap the pipe into an IPC channel.
  scoped_ptr<IPC::ChannelProxy> server(new IPC::ChannelProxy(
      IPC::ChannelHandle(pipe),
      IPC::Channel::MODE_SERVER,
      delegate,
      io_task_runner));

  // Convert the channel name to the pipe name.
  std::string pipe_name(remoting::kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = NULL;
  security_attributes.bInheritHandle = TRUE;

  // Create the client end of the channel. This code should match the code in
  // IPC::Channel.
  ScopedHandle client;
  client.Set(CreateFile(UTF8ToUTF16(pipe_name).c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        &security_attributes,
                        OPEN_EXISTING,
                        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                            FILE_FLAG_OVERLAPPED,
                        NULL));
  if (!client.IsValid())
    return false;

  *client_out = client.Pass();
  *server_out = server.Pass();
  *pipe_out = pipe.Pass();
  return true;
}

} // namespace

namespace remoting {

UnprivilegedProcessDelegate::UnprivilegedProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const FilePath& binary_path)
    : main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner),
      binary_path_(binary_path) {
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
  // Generate a unique name for the channel.
  std::string channel_name = GenerateIpcChannelName(this);

  // Create a connected IPC channel.
  ScopedHandle client;
  scoped_ptr<IPC::ChannelProxy> server;
  ScopedHandle pipe;
  if (!CreateConnectedIpcChannel(channel_name, io_task_runner_, delegate,
                                 &client, &server, &pipe)) {
    return false;
  }

  // Convert the handle value into a decimal integer. Handle values are 32bit
  // even on 64bit platforms.
  std::string pipe_handle = base::StringPrintf(
      "%d", reinterpret_cast<ULONG_PTR>(client.Get()));

  // Create the command line passing the name of the IPC channel to use and
  // copying known switches from the caller's command line.
  CommandLine command_line(binary_path_);
  command_line.AppendSwitchASCII(kDaemonPipeSwitchName, pipe_handle);
  command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kCopiedSwitchNames,
                                arraysize(kCopiedSwitchNames));

  // Try to launch the process.
  // TODO(alexeypa): Pass a restricted process token.
  // See http://crbug.com/134694.
  ScopedHandle worker_thread;
  worker_process_.Close();
  if (!LaunchProcessWithToken(command_line.GetProgram(),
                              command_line.GetCommandLineString(),
                              NULL,
                              true,
                              0,
                              &worker_process_,
                              &worker_thread)) {
    return false;
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
