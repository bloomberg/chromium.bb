// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_single_client_message_proxy.h"

namespace chromeos {

namespace secure_channel {

FakeSingleClientMessageProxy::FakeSingleClientMessageProxy(
    Delegate* delegate,
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : SingleClientMessageProxy(delegate),
      proxy_id_(base::UnguessableToken::Create()),
      destructor_callback_(std::move(destructor_callback)) {}

FakeSingleClientMessageProxy::~FakeSingleClientMessageProxy() {
  if (destructor_callback_)
    std::move(destructor_callback_).Run(GetProxyId());
}

const base::UnguessableToken& FakeSingleClientMessageProxy::GetProxyId() {
  return proxy_id_;
}

void FakeSingleClientMessageProxy::HandleReceivedMessage(
    const std::string& feature,
    const std::string& payload) {
  processed_messages_.push_back(std::make_pair(feature, payload));
}

void FakeSingleClientMessageProxy::HandleRemoteDeviceDisconnection() {
  was_remote_device_disconnection_handled_ = true;
}

FakeSingleClientMessageProxyDelegate::FakeSingleClientMessageProxyDelegate() =
    default;

FakeSingleClientMessageProxyDelegate::~FakeSingleClientMessageProxyDelegate() =
    default;

void FakeSingleClientMessageProxyDelegate::OnSendMessageRequested(
    const std::string& message_feaure,
    const std::string& message_payload,
    base::OnceClosure on_sent_callback) {
  send_message_requests_.push_back(std::make_tuple(
      message_feaure, message_payload, std::move(on_sent_callback)));
}

void FakeSingleClientMessageProxyDelegate::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  return std::move(callback).Run(std::move(connection_metadata_for_next_call_));
}

void FakeSingleClientMessageProxyDelegate::OnClientDisconnected(
    const base::UnguessableToken& proxy_id) {
  disconnected_proxy_id_ = proxy_id;

  if (on_client_disconnected_closure_)
    std::move(on_client_disconnected_closure_).Run();
}

}  // namespace secure_channel

}  // namespace chromeos
