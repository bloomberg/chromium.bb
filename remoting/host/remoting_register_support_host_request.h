// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "remoting/base/grpc_support/grpc_authenticated_executor.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/proto/remoting/v1/remote_support_host_service.grpc.pb.h"
#include "remoting/signaling/muxing_signal_strategy.h"

namespace remoting {

class OAuthTokenGetter;

// A RegisterSupportHostRequest implementation that uses Remoting API to
// register the host.
//
// Note that IT ONLY WORKS WITH MuxingSignalStrategy.
class RemotingRegisterSupportHostRequest final
    : public RegisterSupportHostRequest,
      public SignalStrategy::Listener {
 public:
  explicit RemotingRegisterSupportHostRequest(
      std::unique_ptr<OAuthTokenGetter> token_getter);
  ~RemotingRegisterSupportHostRequest() override;

  // RegisterSupportHostRequest implementation.
  void StartRequest(SignalStrategy* signal_strategy,
                    scoped_refptr<RsaKeyPair> key_pair,
                    RegisterCallback callback) override;

 private:
  using RemoteSupportService = apis::v1::RemoteSupportService;

  // MuxingSignalStrategy might notify a CONNECTED state for more than once, so
  // this is necessary to prevent trying to register twice when a strategy is
  // connected after the timeout.
  enum class State {
    NOT_STARTED,
    REGISTERING,
    REGISTERED,
  };

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  void RegisterHost();
  void OnRegisterHostResult(
      const grpc::Status& status,
      const apis::v1::RegisterSupportHostResponse& response);

  void RunCallback(const std::string& support_id,
                   base::TimeDelta lifetime,
                   protocol::ErrorCode error_code);

  MuxingSignalStrategy* signal_strategy_ = nullptr;
  scoped_refptr<RsaKeyPair> key_pair_;
  RegisterCallback callback_;
  std::unique_ptr<OAuthTokenGetter> token_getter_;
  GrpcAuthenticatedExecutor grpc_executor_;

  State state_ = State::NOT_STARTED;

  std::unique_ptr<RemoteSupportService::Stub> remote_support_;

  DISALLOW_COPY_AND_ASSIGN(RemotingRegisterSupportHostRequest);
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTING_REGISTER_SUPPORT_HOST_REQUEST_H_
