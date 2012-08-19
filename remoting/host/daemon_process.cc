// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"

namespace {

const char kIpcThreadName[] = "Daemon process IPC";

}  // namespace

namespace remoting {

DaemonProcess::~DaemonProcess() {
}

bool DaemonProcess::OnMessageReceived(const IPC::Message& message) {
  return true;
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
    base::Bind(&DaemonProcess::Init, base::Unretained(this)));
}

void DaemonProcess::Init() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!LaunchNetworkProcess()) {
    LOG(ERROR) << "Failed to launch the networking process.";
    Stop();
    return;
  }
}

void DaemonProcess::DoStop() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  CompleteStopping();
}

}  // namespace remoting
