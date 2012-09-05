// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_messages.h"

namespace remoting {

DaemonProcess::~DaemonProcess() {
  CHECK(!config_watcher_.get());
}

void DaemonProcess::OnConfigUpdated(const std::string& serialized_config) {
  if (serialized_config_ != serialized_config) {
    serialized_config_ = serialized_config;
    Send(new ChromotingDaemonNetworkMsg_Configuration(serialized_config_));
  }
}

void DaemonProcess::OnConfigWatcherError() {
  Stop();
}

void DaemonProcess::OnChannelConnected() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());

  // Send the configuration to the network process.
  Send(new ChromotingDaemonNetworkMsg_Configuration(serialized_config_));
}

bool DaemonProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(main_task_runner()->BelongsToCurrentThread());

  return false;
}

DaemonProcess::DaemonProcess(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : Stoppable(main_task_runner, stopped_callback),
      main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner) {
  // Initialize on the same thread that will be used for shutting down.
  main_task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&DaemonProcess::Initialize, base::Unretained(this)));
}

void DaemonProcess::Initialize() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());

  // Get the name of the host configuration file.
  FilePath default_config_dir = remoting::GetConfigDir();
  FilePath config_path = default_config_dir.Append(kDefaultHostConfigFile);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHostConfigSwitchName)) {
    config_path = command_line->GetSwitchValuePath(kHostConfigSwitchName);
  }

  // Start watching the host configuration file.
  config_watcher_.reset(new ConfigFileWatcher(main_task_runner(),
                                              io_task_runner(),
                                              this));
  config_watcher_->Watch(config_path);

  // Launch the process.
  LaunchNetworkProcess();
}

void DaemonProcess::DoStop() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());

  config_watcher_.reset();
  CompleteStopping();
}

}  // namespace remoting
