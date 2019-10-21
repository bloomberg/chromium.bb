// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_connection_factory.h"

namespace openscreen {
namespace platform {

void TlsConnectionFactory::OnAccepted(
    X509* peer_cert,
    std::unique_ptr<TlsConnection> connection) {
  task_runner_->PostTask(
      [peer_cert, c = std::move(connection), this]() mutable {
        // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
        this->client_->OnAccepted(this, peer_cert, std::move(c));
      });
}

void TlsConnectionFactory::OnConnected(
    X509* peer_cert,
    std::unique_ptr<TlsConnection> connection) {
  task_runner_->PostTask(
      [peer_cert, c = std::move(connection), this]() mutable {
        // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
        this->client_->OnConnected(this, peer_cert, std::move(c));
      });
}

void TlsConnectionFactory::OnConnectionFailed(
    const IPEndpoint& remote_address) {
  task_runner_->PostTask([remote_address, this]() {
    // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
    this->client_->OnConnectionFailed(this, remote_address);
  });
}

void TlsConnectionFactory::OnError(Error error) {
  task_runner_->PostTask([e = std::move(error), this]() mutable {
    // TODO(crbug.com/openscreen/71): |this| may be invalid at this point.
    this->client_->OnError(this, std::move(e));
  });
}

}  // namespace platform
}  // namespace openscreen
