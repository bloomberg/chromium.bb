// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WORKER_PROCESS_IPC_DELEGATE_H_
#define REMOTING_HOST_WORKER_PROCESS_IPC_DELEGATE_H_

#include <stdint.h>

#include "base/compiler_specific.h"

namespace IPC {
class Message;
} // namespace IPC

namespace remoting {

// An interface representing the object receiving IPC messages from a worker
// process.
class WorkerProcessIpcDelegate {
 public:
  virtual ~WorkerProcessIpcDelegate() {}

  // Notifies that a client has been connected to the channel.
  virtual void OnChannelConnected(int32_t peer_pid) = 0;

  // Processes messages sent by the client.
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

  // Notifies that a permanent error was encountered.
  virtual void OnPermanentError(int exit_code) = 0;

  // Notifies that the worker process stops for any reason.
  virtual void OnWorkerProcessStopped() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WORKER_PROCESS_IPC_DELEGATE_H_
