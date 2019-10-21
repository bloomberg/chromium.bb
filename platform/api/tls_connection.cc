// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_connection.h"

#include "platform/api/task_runner.h"

namespace openscreen {
namespace platform {

void TlsConnection::OnWriteBlocked() {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([this]() {
    // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
    this->client_->OnWriteBlocked(this);
  });
}

void TlsConnection::OnWriteUnblocked() {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([this]() {
    // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
    this->client_->OnWriteUnblocked(this);
  });
}

void TlsConnection::OnError(Error error) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([e = std::move(error), this]() mutable {
    // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
    this->client_->OnError(this, std::move(e));
  });
}

void TlsConnection::OnRead(std::vector<uint8_t> block) {
  if (!client_) {
    return;
  }

  task_runner_->PostTask([b = std::move(block), this]() mutable {
    // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
    this->client_->OnRead(this, std::move(b));
  });
}

}  // namespace platform
}  // namespace openscreen
