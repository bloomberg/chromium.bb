// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_util.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/single_thread_task_runner.h"
#include "ipc/attachment_broker.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"

namespace remoting {

bool CreateConnectedIpcChannel(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    IPC::Listener* listener,
    base::File* client_out,
    std::unique_ptr<IPC::ChannelProxy>* server_out) {
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
    if (IGNORE_EINTR(close(pipe_fds[0])) < 0)
      PLOG(ERROR) << "close()";
    if (IGNORE_EINTR(close(pipe_fds[1])) < 0)
      PLOG(ERROR) << "close()";
    return false;
  }

  std::string socket_name = "Chromoting socket";

  // Wrap the pipe into an IPC channel.
  base::FileDescriptor fd(pipe_fds[0], false);
  server_out->reset(new IPC::ChannelProxy(listener, io_task_runner));
  if (IPC::AttachmentBroker::GetGlobal()) {
    IPC::AttachmentBroker::GetGlobal()->RegisterCommunicationChannel(
        server_out->get(), io_task_runner);
  }
  (*server_out)
      ->Init(IPC::ChannelHandle(socket_name, fd), IPC::Channel::MODE_SERVER,
             true);

  *client_out = base::File(pipe_fds[1]);
  return true;
}

} // namespace remoting
