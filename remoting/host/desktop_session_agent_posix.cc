// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_agent.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/event_executor.h"

namespace remoting {

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgentPosix : public DesktopSessionAgent {
 public:
  DesktopSessionAgentPosix(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner);

 protected:
  virtual ~DesktopSessionAgentPosix();

  virtual bool CreateChannelForNetworkProcess(
      IPC::PlatformFileForTransit* client_out,
      scoped_ptr<IPC::ChannelProxy>* server_out) OVERRIDE;
  virtual scoped_ptr<EventExecutor> CreateEventExecutor() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgentPosix);
};

DesktopSessionAgentPosix::DesktopSessionAgentPosix(
    scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner)
    : DesktopSessionAgent(audio_capture_task_runner, caller_task_runner,
                          input_task_runner, io_task_runner,
                          video_capture_task_runner) {
}

DesktopSessionAgentPosix::~DesktopSessionAgentPosix() {
}

bool DesktopSessionAgentPosix::CreateChannelForNetworkProcess(
    IPC::PlatformFileForTransit* client_out,
    scoped_ptr<IPC::ChannelProxy>* server_out) {
  // Create a socket pair.
  int pipe_fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds) != 0) {
    PLOG(ERROR) << "socketpair()";
    return false;
  }

  // Set both ends to be non-blocking.
  if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1 ||
      fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK)";
    if (HANDLE_EINTR(close(pipe_fds[0])) < 0)
      PLOG(ERROR) << "close()";
    if (HANDLE_EINTR(close(pipe_fds[1])) < 0)
      PLOG(ERROR) << "close()";
    return false;
  }

  // Generate a unique name for the channel.
  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();
  std::string socket_name = "DesktopSessionAgent socket";

  // Wrap the pipe into an IPC channel.
  base::FileDescriptor fd(pipe_fds[0], false);
  IPC::ChannelHandle handle(socket_name, fd);
  server_out->reset(new IPC::ChannelProxy(
      IPC::ChannelHandle(socket_name, fd),
      IPC::Channel::MODE_SERVER,
      this,
      io_task_runner()));

  *client_out = base::FileDescriptor(pipe_fds[1], false);
  return true;
}

scoped_ptr<EventExecutor> DesktopSessionAgentPosix::CreateEventExecutor() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return EventExecutor::Create(input_task_runner(),
                               caller_task_runner()).Pass();
}

// static
scoped_refptr<DesktopSessionAgent> DesktopSessionAgent::Create(
    scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner) {
  return scoped_refptr<DesktopSessionAgent>(
      new DesktopSessionAgentPosix(audio_capture_task_runner,
                                   caller_task_runner, input_task_runner,
                                   io_task_runner, video_capture_task_runner));
}

}  // namespace remoting
