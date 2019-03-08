// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_CLIENT_H_
#define REMOTING_SIGNALING_FTL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_async_dispatcher.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {

// This is the class that makes RPC calls to the FTL signaling backend.
class FtlClient {
 public:
  template <typename ResponseType>
  using RpcCallback =
      base::OnceCallback<void(const grpc::Status&, const ResponseType&)>;

  explicit FtlClient(OAuthTokenGetter* token_getter);
  ~FtlClient();

  void SetAuthToken(const std::string& auth_token);

  // Retrieves the ice server configs.
  void GetIceServer(RpcCallback<ftl::GetICEServerResponse> callback);

  // Performs a SignInGaia call for this device. This does not set the auth
  // token.
  void SignInGaia(const std::string& device_id,
                  ftl::SignInGaiaMode::Value sign_in_gaia_mode,
                  RpcCallback<ftl::SignInGaiaResponse> callback);

  void PullMessages(RpcCallback<ftl::PullMessagesResponse> callback);

  void AckMessages(const std::vector<ftl::ReceiverMessage>& messages,
                   RpcCallback<ftl::AckMessagesResponse> callback);

 private:
  using PeerToPeer =
      google::internal::communications::instantmessaging::v1::PeerToPeer;
  using Registration =
      google::internal::communications::instantmessaging::v1::Registration;
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  template <typename RequestType, typename ResponseType>
  void GetOAuthTokenAndExecuteRpc(
      GrpcAsyncDispatcher::AsyncRpcFunction<RequestType, ResponseType> rpc,
      const RequestType& request,
      RpcCallback<ResponseType> callback);

  template <typename RequestType, typename ResponseType>
  void ExecuteRpcWithFetchedOAuthToken(
      GrpcAsyncDispatcher::AsyncRpcFunction<RequestType, ResponseType> rpc,
      const RequestType& request,
      RpcCallback<ResponseType> callback,
      OAuthTokenGetter::Status status,
      const std::string& user_email,
      const std::string& access_token);

  static std::unique_ptr<grpc::ClientContext> CreateClientContext();
  std::unique_ptr<ftl::RequestHeader> BuildRequestHeader();

  OAuthTokenGetter* token_getter_;
  std::unique_ptr<PeerToPeer::Stub> peer_to_peer_stub_;
  std::unique_ptr<Registration::Stub> registration_stub_;
  std::unique_ptr<Messaging::Stub> messaging_stub_;
  GrpcAsyncDispatcher dispatcher_;
  std::string auth_token_;

  base::WeakPtrFactory<FtlClient> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FtlClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_CLIENT_H_
