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
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/worker_process_ipc_delegate.h"

class FilePath;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// This class implements core of the daemon process. It manages the networking
// process running at lower privileges and maintains the list of virtual
// terminals.
class DaemonProcess
    : public Stoppable,
      public ConfigFileWatcher::Delegate,
      public WorkerProcessIpcDelegate {
 public:
  virtual ~DaemonProcess();

  // Creates a platform-specific implementation of the daemon process object.
  static scoped_ptr<DaemonProcess> Create(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);

  // ConfigFileWatcher::Delegate
  virtual void OnConfigUpdated(const std::string& serialized_config) OVERRIDE;
  virtual void OnConfigWatcherError() OVERRIDE;

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Sends an IPC message to the network process. The message will be dropped
  // unless the network process is connected over the IPC channel.
  virtual void Send(IPC::Message* message) = 0;

 protected:
  DaemonProcess(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                const base::Closure& stopped_callback);

  // Reads the host configuration and launches the networking process.
  void Initialize();

  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // Launches the network process and establishes an IPC channel with it.
  virtual void LaunchNetworkProcess() = 0;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner() {
    return main_task_runner_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() {
    return io_task_runner_;
  }

 private:
  scoped_ptr<ConfigFileWatcher> config_watcher_;

  // The configuration file contents.
  std::string serialized_config_;

  // The main task runner. Typically it is the UI message loop.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Handles IPC and background I/O tasks.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcess);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_PROCESS_H_
