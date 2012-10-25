// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
#define REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace remoting {

class AutoThreadTaskRunner;

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgent : public IPC::Listener {
 public:
  static scoped_ptr<DesktopSessionAgent> Create(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner);

  virtual ~DesktopSessionAgent();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Creates the screen/audio recorders, input stubs and the IPC channel to be
  // used to access them. Returns a handle of the client end of the IPC channel
  // pipe to be forwarder to the corresponding desktop environment.
  bool Start(const base::Closure& done_task,
             IPC::PlatformFileForTransit* desktop_pipe_out);

 protected:
  DesktopSessionAgent(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner);

  // Creates a pre-connected IPC channel to be used to access the screen/audio
  // recorders and input stubs.
  virtual bool DoCreateNetworkChannel(
      IPC::PlatformFileForTransit* client_out,
      scoped_ptr<IPC::ChannelProxy>* server_out) = 0;

  scoped_refptr<AutoThreadTaskRunner> caller_task_runner() const {
    return caller_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> io_task_runner() const {
    return io_task_runner_;
  }

 private:
  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Message loop used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Run on |caller_task_runner_| to notify the caller that |this| has been
  // stopped.
  base::Closure done_task_;

  // IPC channel connecting the desktop process with the network process.
  scoped_ptr<IPC::ChannelProxy> network_channel_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
