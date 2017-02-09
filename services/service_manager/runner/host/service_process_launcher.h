// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_RUNNER_HOST_SERVICE_PROCESS_LAUNCHER_H_
#define SERVICES_SERVICE_MANAGER_RUNNER_HOST_SERVICE_PROCESS_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

namespace base {
class CommandLine;
class TaskRunner;
}

namespace service_manager {

class Identity;

// This class represents a "child process host". Handles launching and
// connecting a platform-specific "pipe" to the child, and supports joining the
// child process. Currently runs a single service, loaded from a standalone
// service executable on the file system.
//
// This class is not thread-safe. It should be created/used/destroyed on a
// single thread.
//
// Note: Does not currently work on Windows before Vista.
class ServiceProcessLauncher {
 public:
  using ProcessReadyCallback = base::Callback<void(base::ProcessId)>;

  class Delegate {
   public:
    // Called to adjust the commandline for launching the specified app.
    // WARNING: this is called on a background thread.
    virtual void AdjustCommandLineArgumentsForTarget(
        const Identity& target,
        base::CommandLine* command_line) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |name| is just for debugging ease. We will spawn off a process so that it
  // can be sandboxed if |start_sandboxed| is true. |service_path| is a path to
  // the service executable we wish to start.
  ServiceProcessLauncher(base::TaskRunner* launch_process_runner,
                         Delegate* delegate,
                         const base::FilePath& service_path);
  ~ServiceProcessLauncher();

  // |Start()|s the child process; calls |DidStart()| (on the thread on which
  // |Start()| was called) when the child has been started (or failed to start).
  mojom::ServicePtr Start(const Identity& target,
                          bool start_sandboxed,
                          const ProcessReadyCallback& callback);

  // Waits for the child process to terminate.
  void Join();

 private:
  void DidStart(const ProcessReadyCallback& callback);
  void DoLaunch(std::unique_ptr<base::CommandLine> child_command_line);

  scoped_refptr<base::TaskRunner> launch_process_runner_;
  Delegate* delegate_ = nullptr;
  bool start_sandboxed_ = false;
  Identity target_;
  base::FilePath service_path_;
  base::Process child_process_;

  // Used to initialize the Mojo IPC channel between parent and child.
  std::unique_ptr<mojo::edk::PlatformChannelPair> mojo_ipc_channel_;
  mojo::edk::HandlePassingInformation handle_passing_info_;
  mojo::edk::PendingProcessConnection process_connection_;

  // Since Start() calls a method on another thread, we use an event to block
  // the main thread if it tries to destruct |this| while launching the process.
  base::WaitableEvent start_child_process_event_;

  base::WeakPtrFactory<ServiceProcessLauncher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncher);
};

class ServiceProcessLauncherFactory {
 public:
  virtual ~ServiceProcessLauncherFactory() {}
  virtual std::unique_ptr<ServiceProcessLauncher> Create(
      const base::FilePath& service_path) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_RUNNER_HOST_SERVICE_PROCESS_LAUNCHER_H_
