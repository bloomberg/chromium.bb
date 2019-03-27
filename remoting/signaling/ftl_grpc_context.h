// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_
#define REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_async_executor.h"
#include "remoting/signaling/grpc_support/grpc_async_server_streaming_request.h"
#include "remoting/signaling/grpc_support/grpc_async_unary_request.h"
#include "remoting/signaling/grpc_support/scoped_grpc_server_stream.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {

// This is the class that makes RPC calls to the FTL signaling backend.
class FtlGrpcContext final {
 public:
  template <typename ResponseType>
  using RpcCallback =
      base::OnceCallback<void(const grpc::Status&, const ResponseType&)>;
  using StreamStartedCallback =
      base::OnceCallback<void(std::unique_ptr<ScopedGrpcServerStream>)>;

  static std::string GetChromotingAppIdentifier();
  static ftl::Id BuildIdFromString(const std::string& ftl_id);

  explicit FtlGrpcContext(OAuthTokenGetter* token_getter);
  ~FtlGrpcContext();

  void SetAuthToken(const std::string& auth_token);

  // |request| doesn't need to set the header field since this class will set
  // it for you.
  template <typename RequestType, typename ResponseType>
  void ExecuteRpc(GrpcAsyncUnaryRpcFunction<RequestType, ResponseType> rpc,
                  const RequestType& request,
                  GrpcAsyncUnaryRpcCallback<ResponseType> callback) {
    RequestType mutable_request = request;
    mutable_request.set_allocated_header(BuildRequestHeader().release());
    auto grpc_request =
        CreateGrpcAsyncUnaryRequest(std::move(rpc), CreateClientContext(),
                                    mutable_request, std::move(callback));
    GetOAuthTokenAndExecuteRpc(std::move(grpc_request), base::DoNothing());
  }

  // |request| doesn't need to set the header field since this class will set
  // it for you.
  // Note that |on_stream_started| will be removed in a future refactoring CL.
  template <typename RequestType, typename ResponseType>
  void ExecuteServerStreamingRpc(
      GrpcAsyncServerStreamingRpcFunction<RequestType, ResponseType> rpc,
      const RequestType& request,
      StreamStartedCallback on_stream_started,
      const base::RepeatingCallback<void(const ResponseType&)>& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed) {
    RequestType mutable_request = request;
    mutable_request.set_allocated_header(BuildRequestHeader().release());
    std::unique_ptr<ScopedGrpcServerStream> scoped_stream;
    auto grpc_request = CreateGrpcAsyncServerStreamingRequest(
        std::move(rpc), CreateClientContext(), mutable_request,
        std::move(on_incoming_msg), std::move(on_channel_closed),
        &scoped_stream);
    GetOAuthTokenAndExecuteRpc(
        std::move(grpc_request),
        base::BindOnce(std::move(on_stream_started), std::move(scoped_stream)));
  }

  std::shared_ptr<grpc::ChannelInterface> channel() { return channel_; }

  void SetChannelForTesting(std::shared_ptr<grpc::ChannelInterface> channel);

 private:
  void GetOAuthTokenAndExecuteRpc(std::unique_ptr<GrpcAsyncRequest> request,
                                  base::OnceClosure on_stream_started);

  void ExecuteRpcWithFetchedOAuthToken(
      std::unique_ptr<GrpcAsyncRequest> request,
      base::OnceClosure on_stream_started,
      OAuthTokenGetter::Status status,
      const std::string& user_email,
      const std::string& access_token);

  static std::unique_ptr<grpc::ClientContext> CreateClientContext();
  std::unique_ptr<ftl::RequestHeader> BuildRequestHeader();

  std::shared_ptr<grpc::ChannelInterface> channel_;
  OAuthTokenGetter* token_getter_;
  GrpcAsyncExecutor executor_;
  std::string auth_token_;

  base::WeakPtrFactory<FtlGrpcContext> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FtlGrpcContext);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_
