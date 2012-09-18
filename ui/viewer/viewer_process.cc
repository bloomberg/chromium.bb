// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/viewer/viewer_process.h"

#include "ui/viewer/viewer_ipc_server.h"
#include "ui/viewer/viewer_host_win.h"

ViewerProcess* g_viewer_process = NULL;

ViewerProcess::ViewerProcess()
  : shutdown_event_(true, false),
    main_message_loop_(NULL) {
  DCHECK(!g_viewer_process);
  g_viewer_process = this;
}

ViewerProcess::~ViewerProcess() {
  Teardown();
  g_viewer_process = NULL;
}

bool ViewerProcess::Initialize(MessageLoop* message_loop,
                               const CommandLine& command_line) {
  main_message_loop_ = message_loop;

  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_.reset(new base::Thread("ViewerProcess_IO"));
  if (!io_thread_->StartWithOptions(options)) {
    NOTREACHED();
    Teardown();
    return false;
  }

  VLOG(1) << "Starting Viewer Process IPC Server";
  // TODO(scottmg): Channel name should be per user-data-dir.
  ipc_server_.reset(new ViewerIPCServer("viewer_ipc"));
  ipc_server_->Init();

  // TODO(scottmg): I guess the whole thing should be made Windows-only at
  // some point.
#if defined(OS_WIN)
  viewer_host_win_.reset(new ViewerHostWin);
#endif

  // TODO(scottmg): Ask for a handle here, maybe start a browser, all that
  // jazz.

  return true;
}

bool ViewerProcess::Teardown() {
  ipc_server_.reset();
  io_thread_.reset();
  return true;
}

bool ViewerProcess::HandleClientDisconnect() {
  Terminate();
  return false;
}

void ViewerProcess::Terminate() {
  main_message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}
