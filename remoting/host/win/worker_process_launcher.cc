// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/worker_process_launcher.h"

#include <windows.h>
#include <sddl.h>
#include <limits>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::win::ScopedHandle;

namespace {

// Match the pipe name prefix used by Chrome IPC channels so that the client
// could use Chrome IPC APIs instead of connecting manually.
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome.";

} // namespace

namespace remoting {

WorkerProcessLauncher::Delegate::~Delegate() {
}

WorkerProcessLauncher::WorkerProcessLauncher(
    Delegate* launcher_delegate,
    WorkerProcessIpcDelegate* worker_delegate,
    const base::Closure& stopped_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : Stoppable(main_task_runner, stopped_callback),
      launcher_delegate_(launcher_delegate),
      worker_delegate_(worker_delegate),
      main_task_runner_(main_task_runner),
      ipc_task_runner_(ipc_task_runner) {
}

WorkerProcessLauncher::~WorkerProcessLauncher() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void WorkerProcessLauncher::Start(const std::string& pipe_sddl) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() == NULL);
  DCHECK(!pipe_.IsValid());
  DCHECK(!process_exit_event_.IsValid());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  std::string channel_name = GenerateRandomChannelId();
  if (CreatePipeForIpcChannel(channel_name, pipe_sddl, &pipe_)) {
    // Wrap the pipe into an IPC channel.
    ipc_channel_.reset(new IPC::ChannelProxy(
        IPC::ChannelHandle(pipe_),
        IPC::Channel::MODE_SERVER,
        this,
        ipc_task_runner_));

    // Launch the process and attach an object watcher to the returned process
    // handle so that we get notified if the process terminates.
    if (launcher_delegate_->DoLaunchProcess(channel_name,
                                            &process_exit_event_)) {
      if (process_watcher_.StartWatching(process_exit_event_, this)) {
        return;
      }

      launcher_delegate_->DoKillProcess(CONTROL_C_EXIT);
    }
  }

  Stop();
}

void WorkerProcessLauncher::Send(IPC::Message* message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (ipc_channel_.get()) {
    ipc_channel_->Send(message);
  } else {
    delete message;
  }
}

void WorkerProcessLauncher::OnObjectSignaled(HANDLE object) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  Stop();
}

bool WorkerProcessLauncher::OnMessageReceived(const IPC::Message& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() != NULL);
  DCHECK(pipe_.IsValid());
  DCHECK(process_exit_event_.IsValid());

  return worker_delegate_->OnMessageReceived(message);
}

void WorkerProcessLauncher::OnChannelConnected(int32 peer_pid) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() != NULL);
  DCHECK(pipe_.IsValid());
  DCHECK(process_exit_event_.IsValid());

  // |peer_pid| is send by the client and cannot be trusted.
  // GetNamedPipeClientProcessId() is not available on XP. The pipe's security
  // descriptor is the only protection we currently have against malicious
  // clients.
  //
  // If we'd like to be able to launch low-privileged workers and let them
  // connect back, the pipe handle should be passed to the worker instead of
  // the pipe name.
  worker_delegate_->OnChannelConnected();
}

void WorkerProcessLauncher::OnChannelError() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() != NULL);
  DCHECK(pipe_.IsValid());
  DCHECK(process_exit_event_.IsValid());

  // Schedule a delayed termination of the worker process. Usually, the pipe is
  // disconnected when the worker process is about to exit. Waiting a little bit
  // here allows the worker to exit completely and so, notify
  // |process_watcher_|. As the result DoKillProcess() will not be called and
  // the original exit code reported by the worker process will be retrieved.
  ipc_error_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
                         this, &WorkerProcessLauncher::Stop);
}

void WorkerProcessLauncher::DoStop() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  ipc_channel_.reset();
  pipe_.Close();

  // Kill the process if it has been started already.
  if (process_watcher_.GetWatchedObject() != NULL) {
    launcher_delegate_->DoKillProcess(CONTROL_C_EXIT);
    return;
  }

  ipc_error_timer_.Stop();

  DCHECK(ipc_channel_.get() == NULL);
  DCHECK(!pipe_.IsValid());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  process_exit_event_.Close();
  CompleteStopping();
}

// Creates the server end of the Chromoting IPC channel.
bool WorkerProcessLauncher::CreatePipeForIpcChannel(
    const std::string& channel_name,
    const std::string& pipe_sddl,
    ScopedHandle* pipe_out) {
  // Create security descriptor for the channel.
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = FALSE;

  ULONG security_descriptor_length = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
          UTF8ToUTF16(pipe_sddl).c_str(),
          SDDL_REVISION_1,
          reinterpret_cast<PSECURITY_DESCRIPTOR*>(
              &security_attributes.lpSecurityDescriptor),
          &security_descriptor_length)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create a security descriptor for the Chromoting IPC channel";
    return false;
  }

  // Convert the channel name to the pipe name.
  std::string pipe_name(kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  // Create the server end of the pipe. This code should match the code in
  // IPC::Channel with exception of passing a non-default security descriptor.
  base::win::ScopedHandle pipe;
  pipe.Set(CreateNamedPipe(
      UTF8ToUTF16(pipe_name).c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
      1,
      IPC::Channel::kReadBufferSize,
      IPC::Channel::kReadBufferSize,
      5000,
      &security_attributes));
  if (!pipe.IsValid()) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create the server end of the Chromoting IPC channel";
    LocalFree(security_attributes.lpSecurityDescriptor);
    return false;
  }

  LocalFree(security_attributes.lpSecurityDescriptor);

  *pipe_out = pipe.Pass();
  return true;
}

// N.B. Copied from src/content/common/child_process_host_impl.cc
std::string WorkerProcessLauncher::GenerateRandomChannelId() {
  return base::StringPrintf("%d.%p.%d",
                            base::GetCurrentProcId(), this,
                            base::RandInt(0, std::numeric_limits<int>::max()));
}

} // namespace remoting
