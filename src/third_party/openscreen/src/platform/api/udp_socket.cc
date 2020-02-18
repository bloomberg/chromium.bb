// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#include "platform/api/udp_socket.h"

#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

UdpSocket::UdpSocket(TaskRunner* task_runner, Client* client)
    : client_(client), task_runner_(task_runner) {
  OSP_CHECK(task_runner_);
  deletion_callback_ = [](UdpSocket* socket) {};
}

UdpSocket::~UdpSocket() {
  deletion_callback_(this);
}

void UdpSocket::SetDeletionCallback(std::function<void(UdpSocket*)> callback) {
  deletion_callback_ = callback;
}

void UdpSocket::OnError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    this->client_->OnError(this, std::move(e));
  });
}
void UdpSocket::OnSendError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    this->client_->OnSendError(this, std::move(e));
  });
}
void UdpSocket::OnRead(ErrorOr<UdpPacket> read_data) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([data = std::move(read_data), this]() mutable {
    this->client_->OnRead(this, std::move(data));
  });
}

}  // namespace platform
}  // namespace openscreen
