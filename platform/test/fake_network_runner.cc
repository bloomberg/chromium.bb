// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/test/fake_network_runner.h"

#include <iostream>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {
class ReadCallbackExecutor {
 public:
  ReadCallbackExecutor(UdpPacket data, std::function<void(UdpPacket)> function)
      : data_(std::move(data)), function_(function) {}

  void operator()() { function_(std::move(data_)); }

 private:
  UdpPacket data_;
  std::function<void(UdpPacket)> function_;
};
}  // namespace

Error FakeNetworkRunner::ReadRepeatedly(UdpSocket* socket,
                                        UdpReadCallback* callback) {
  callbacks_[socket] = callback;
  return Error::Code::kNone;
}

Error FakeNetworkRunner::CancelRead(UdpSocket* socket) {
  callbacks_.erase(socket);
  return Error::Code::kNone;
}

void FakeNetworkRunner::PostNewPacket(UdpPacket packet) {
  auto it = callbacks_.find(packet.socket());
  if (it == callbacks_.end()) {
    return;
  }

  std::function<void(UdpPacket)> callback =
      [this, callback = it->second](UdpPacket packet) {
        callback->OnRead(std::move(packet), this);
      };
  PostTask(ReadCallbackExecutor(std::move(packet), std::move(callback)));
}

void FakeNetworkRunner::PostPackagedTask(Task task) {
  task_queue_.push_back(std::move(task));
}

void FakeNetworkRunner::PostPackagedTaskWithDelay(Task task,
                                                  Clock::duration delay) {
  task_queue_.push_back(std::move(task));
}

void FakeNetworkRunner::RunTasksUntilIdle() {
  while (task_queue_.size()) {
    task_queue_.front()();
    task_queue_.pop_front();
  }
}

}  // namespace platform
}  // namespace openscreen
