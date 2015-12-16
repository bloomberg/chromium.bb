// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_AGENT_H_
#define TOOLS_BATTOR_AGENT_BATTOR_AGENT_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "tools/battor_agent/battor_error.h"

namespace device {
class SerialIoHandler;
}

namespace battor {

// A BattOrAgent is a class used to asynchronously communicate with a BattOr for
// the purpose of collecting power samples. A BattOr is an external USB device
// that's capable of recording accurate, high-frequency (2000Hz) power samples.
//
// The serial connection is automatically opened when the first command
// (e.g. StartTracing(), StopTracing(), etc.) is issued, and automatically
// closed when either StopTracing() or the destructor is called. For Telemetry,
// this means that the connection must be reinitialized for every command that's
// issued because a new BattOrAgent is constructed. For Chromium, we use the
// same BattOrAgent for multiple commands and thus avoid having to reinitialize
// the serial connection.
//
// This class is NOT thread safe, and must be interacted with only from the IO
// thread. The IO thread must also have a running MessageLoop.
class BattOrAgent : public base::SupportsWeakPtr<BattOrAgent> {
 public:
  // The listener interface that must be implemented in order to interact with
  // the BattOrAgent.
  class Listener {
   public:
    virtual void OnStartTracingComplete(BattOrError error) = 0;
  };

  BattOrAgent(
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_rnuner,
      const std::string& path,
      Listener* listener);
  virtual ~BattOrAgent();

  // Tells the BattOr to start tracing.
  void StartTracing();

  // Returns whether the BattOr is able to record clock sync markers in its own
  // trace log.
  static bool SupportsExplicitClockSync() { return true; }

 private:
  // Initializes the serial connection (if not done already) and calls one of
  // the two callbacks depending its success. The callback will be invoked on
  // the same thread that this method is called on.
  void ConnectIfNeeded(const base::Closure& success_callback,
                       const base::Closure& failure_callback);
  void OnConnectComplete(const base::Closure& success_callback,
                         const base::Closure& failure_callback,
                         bool success);

  // StartTracing continuation called once the connection is initialized.
  void DoStartTracing();

  // Resets the connection to its unopened state.
  void ResetConnection();

  // Threads needed for serial communication.
  scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  // The path of the BattOr (e.g. "/dev/tty.battor_serial").
  std::string path_;

  // The listener that handles the commands' results. It must outlive the agent.
  Listener* listener_;

  // IO handler capable of reading and writing from the serial connection.
  scoped_refptr<device::SerialIoHandler> io_handler_;

  // Checker to make sure that this is only ever called on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BattOrAgent);
};

}  // namespace battor

#endif  // TOOLS_BATTOR_AGENT_BATTOR_AGENT_H_
