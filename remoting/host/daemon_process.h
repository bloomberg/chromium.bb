// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_PROCESS_H_
#define REMOTING_HOST_DAEMON_PROCESS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/stoppable.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace IPC {
class Message;
} // namespace IPC

namespace remoting {

// This class implements core of the daemon process. It manages the networking
// process running at lower privileges and maintains the list of virtual
// terminals.
class DaemonProcess : public Stoppable, public IPC::Listener {
 public:
  virtual ~DaemonProcess();

  // Creates a platform-specific implementation of the daemon process object.
  static scoped_ptr<DaemonProcess> Create(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  DaemonProcess(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                const base::Closure& stopped_callback);

  // Reads the host configuration and launches the networking process.
  void Init();

  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // Launches the network process and establishes an IPC channel with it.
  virtual bool LaunchNetworkProcess() = 0;

 private:
  // The main task runner. Typically it is the UI message loop.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Handles IPC and background I/O tasks.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The IPC channel connecting the daemon process to the networking process.
  scoped_ptr<IPC::ChannelProxy> network_process_channel_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_PROCESS_H_
