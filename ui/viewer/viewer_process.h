// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWER_VIEWER_PROCESS_H_
#define UI_VIEWER_VIEWER_PROCESS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"

class CommandLine;
class ViewerHostWin;
class ViewerIPCServer;

// The ViewerProcess does not inherit from ChildProcess because this
// process can live independently of the browser process.
class ViewerProcess {
 public:
  ViewerProcess();
  virtual ~ViewerProcess();

  // Initialize the ViewerProcess with the message loop that it should run on.
  // ViewerProcess takes ownership of |state|.
  bool Initialize(MessageLoop* message_loop, const CommandLine& command_line);

  bool Teardown();

  // Returns the message loop that we perform I/O coordination on (network
  // requests, communication with renderers, etc.).
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy() {
    return io_thread_->message_loop_proxy();
  }

  // A global event object that is signalled when the main thread's message
  // loop exits. This gives background threads a way to observe the main
  // thread shutting down.
  base::WaitableEvent* shutdown_event() {
    return &shutdown_event_;
  }

  // Called by the IPC server when a client disconnects. A return value of
  // true indicates that the IPC server should continue listening for new
  // connections.
  bool HandleClientDisconnect();

  void Terminate();

 private:
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<ViewerIPCServer> ipc_server_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // Window that we draw into.
  scoped_ptr<ViewerHostWin> viewer_host_win_;

  // Pointer to the main message loop that host this object.
  MessageLoop* main_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ViewerProcess);
};

extern ViewerProcess* g_viewer_process;

#endif  // UI_VIEWER_VIEWER_PROCESS_H_
