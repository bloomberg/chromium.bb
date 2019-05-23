// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include "base/strings/stringize_macros.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_channel.h"
#include "remoting/base/grpc_support/grpc_util.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_details.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_signal_strategy.h"

namespace remoting {

namespace {

protocol::ErrorCode MapError(grpc::StatusCode status_code) {
  switch (status_code) {
    case grpc::StatusCode::OK:
      return protocol::ErrorCode::OK;
    case grpc::StatusCode::DEADLINE_EXCEEDED:
      return protocol::ErrorCode::SIGNALING_TIMEOUT;
    case grpc::StatusCode::PERMISSION_DENIED:
    case grpc::StatusCode::UNAUTHENTICATED:
      return protocol::ErrorCode::AUTHENTICATION_FAILED;
    default:
      return protocol::ErrorCode::SIGNALING_ERROR;
  }
}

}  // namespace

RemotingRegisterSupportHostRequest::RemotingRegisterSupportHostRequest(
    std::unique_ptr<OAuthTokenGetter> token_getter)
    : token_getter_(std::move(token_getter)),
      grpc_executor_(token_getter_.get()) {
  remote_support_ = RemoteSupportService::NewStub(CreateSslChannelForEndpoint(
      ServiceUrls::GetInstance()->remoting_server_endpoint()));
}

RemotingRegisterSupportHostRequest::~RemotingRegisterSupportHostRequest() {
  if (signal_strategy_) {
    signal_strategy_->RemoveListener(this);
  }
}

void RemotingRegisterSupportHostRequest::StartRequest(
    SignalStrategy* signal_strategy,
    scoped_refptr<RsaKeyPair> key_pair,
    RegisterCallback callback) {
  DCHECK(signal_strategy);
  DCHECK(key_pair);
  DCHECK(callback);
  signal_strategy_ = static_cast<MuxingSignalStrategy*>(signal_strategy);
  key_pair_ = key_pair;
  callback_ = std::move(callback);

  signal_strategy_->AddListener(this);
}

void RemotingRegisterSupportHostRequest::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      RegisterHost();
      break;
    case SignalStrategy::State::DISCONNECTED:
      RunCallback({}, {}, protocol::ErrorCode::SIGNALING_ERROR);
      break;
    default:
      // Do nothing.
      break;
  }
}

bool RemotingRegisterSupportHostRequest::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void RemotingRegisterSupportHostRequest::RegisterHost() {
  if (state_ != State::NOT_STARTED) {
    return;
  }
  state_ = State::REGISTERING;

  apis::v1::RegisterSupportHostRequest request;
  request.set_public_key(key_pair_->GetPublicKey());
  if (signal_strategy_->ftl_signal_strategy()->GetState() ==
      SignalStrategy::State::CONNECTED) {
    request.set_tachyon_id(
        signal_strategy_->ftl_signal_strategy()->GetLocalAddress().jid());
  }
  if (signal_strategy_->xmpp_signal_strategy()->GetState() ==
      SignalStrategy::State::CONNECTED) {
    request.set_jabber_id(
        signal_strategy_->xmpp_signal_strategy()->GetLocalAddress().jid());
  }
  request.set_host_version(STRINGIZE(VERSION));
  request.set_host_os_name(GetHostOperatingSystemName());
  request.set_host_os_version(GetHostOperatingSystemVersion());

  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&RemoteSupportService::Stub::AsyncRegisterSupportHost,
                     base::Unretained(remote_support_.get())),
      request,
      base::BindOnce(&RemotingRegisterSupportHostRequest::OnRegisterHostResult,
                     base::Unretained(this)));
  grpc_executor_.ExecuteRpc(std::move(grpc_request));
}

void RemotingRegisterSupportHostRequest::OnRegisterHostResult(
    const grpc::Status& status,
    const apis::v1::RegisterSupportHostResponse& response) {
  if (!status.ok()) {
    state_ = State::NOT_STARTED;
    RunCallback({}, {}, MapError(status.error_code()));
    return;
  }
  state_ = State::REGISTERED;
  base::TimeDelta lifetime =
      base::TimeDelta::FromSeconds(response.support_id_lifetime_seconds());
  RunCallback(response.support_id(), lifetime, protocol::ErrorCode::OK);
}

void RemotingRegisterSupportHostRequest::RunCallback(
    const std::string& support_id,
    base::TimeDelta lifetime,
    protocol::ErrorCode error_code) {
  // Cleanup state before calling the callback.
  grpc_executor_.CancelPendingRequests();
  signal_strategy_->RemoveListener(this);
  signal_strategy_ = nullptr;

  std::move(callback_).Run(support_id, lifetime, error_code);
}

}  // namespace remoting
