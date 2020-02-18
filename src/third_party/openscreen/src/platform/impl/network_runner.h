// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_RUNNER_H_
#define PLATFORM_IMPL_NETWORK_RUNNER_H_

#include <mutex>  // NOLINT
#include <thread>

#include "platform/api/network_runner.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/api/udp_socket.h"
#include "platform/impl/network_reader.h"

namespace openscreen {
namespace platform {

// This class implements a 2-thread network runner. The first thread is the task
// runner thread, which is already running when the Task Runner is passed to the
// constructor of NetworkRunnerImpl. The second thread is the
// NetworkRunnerImpl's RunUntilStopped() method, which must be running for the
// object to work as expected.
// TODO(rwkeane): Update to NetworkRunnerImpl<T> : public NetworkRunner, T
// (for TaskRunner T) instead of taking a TaskRunner as a parameter to eliminate
// having to pass calls to the TaskRunner instance.
class NetworkRunnerImpl final : public NetworkRunner {
 public:
  // Creates a new NetworkRunnerImpl with the provided TaskRunner. Note that the
  // Task Runner is expected to be running at the time it is provided.
  explicit NetworkRunnerImpl(std::unique_ptr<TaskRunner> task_runner);

  Error ReadRepeatedly(UdpSocket* socket, UdpReadCallback* callback);

  bool CancelRead(UdpSocket* socket);

  void PostPackagedTask(Task task);

  void PostPackagedTaskWithDelay(Task task, Clock::duration delay);

  // This method will process Network Read Events until the RequestStopSoon(...)
  // method is called, and will block the current thread until this time.
  void RunUntilStopped();

  // Stops this instance from processing network events and causes the
  // RunUntilStopped(...) method to exit.
  void RequestStopSoon();

 protected:
  // Creates a new NetworkRunnerImpl with the provided NetworkLoop and
  // TaskRunner. Note that the Task Runner is expected to be running at the time
  // it is provided.
  NetworkRunnerImpl(std::unique_ptr<TaskRunner> task_runner,
                    std::unique_ptr<NetworkReader> network_loop);

  // Objects handling actual processing of this instance's calls.
  std::unique_ptr<NetworkReader> network_loop_;
  std::unique_ptr<TaskRunner> task_runner_;

 private:
  // Atomic so that we can perform atomic exchanges.
  std::atomic_bool is_running_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkRunnerImpl);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_RUNNER_H_
