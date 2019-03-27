// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_
#define REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_support/grpc_async_executor.h"
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
  void ExecuteRpc(
      GrpcAsyncExecutor::AsyncRpcFunction<RequestType, ResponseType> rpc,
      const RequestType& request,
      RpcCallback<ResponseType> callback) {
    auto execute_rpc_with_context = base::BindOnce(
        &FtlGrpcContext::ExecuteRpcWithContext<RequestType, ResponseType>,
        weak_factory_.GetWeakPtr(), std::move(rpc), request,
        std::move(callback));
    GetOAuthTokenAndExecuteRpc(std::move(execute_rpc_with_context));
  }

  // |request| doesn't need to set the header field since this class will set
  // it for you.
  template <typename RequestType, typename ResponseType>
  void ExecuteServerStreamingRpc(
      GrpcAsyncExecutor::AsyncServerStreamingRpcFunction<RequestType,
                                                         ResponseType> rpc,
      const RequestType& request,
      StreamStartedCallback on_stream_started,
      const GrpcAsyncExecutor::RpcStreamCallback<ResponseType>& on_incoming_msg,
      GrpcAsyncExecutor::RpcChannelClosedCallback on_channel_closed) {
    auto execute_rpc_with_context = base::BindOnce(
        &FtlGrpcContext::ExecuteServerStreamingRpcWithContext<RequestType,
                                                              ResponseType>,
        weak_factory_.GetWeakPtr(), std::move(rpc), request,
        std::move(on_stream_started), on_incoming_msg,
        std::move(on_channel_closed));
    GetOAuthTokenAndExecuteRpc(std::move(execute_rpc_with_context));
  }

  std::shared_ptr<grpc::ChannelInterface> channel() { return channel_; }

  void SetChannelForTesting(std::shared_ptr<grpc::ChannelInterface> channel);

 private:
  using ExecuteRpcWithContextCallback =
      base::OnceCallback<void(std::unique_ptr<grpc::ClientContext>)>;

  template <typename RequestType, typename ResponseType>
  void ExecuteRpcWithContext(
      GrpcAsyncExecutor::AsyncRpcFunction<RequestType, ResponseType> rpc,
      RequestType request,
      RpcCallback<ResponseType> callback,
      std::unique_ptr<grpc::ClientContext> context) {
    request.set_allocated_header(BuildRequestHeader().release());
    dispatcher_.ExecuteAsyncRpc(std::move(rpc), std::move(context), request,
                                std::move(callback));
  }

  template <typename RequestType, typename ResponseType>
  void ExecuteServerStreamingRpcWithContext(
      GrpcAsyncExecutor::AsyncServerStreamingRpcFunction<RequestType,
                                                         ResponseType> rpc,
      RequestType request,
      StreamStartedCallback on_stream_started,
      const GrpcAsyncExecutor::RpcStreamCallback<ResponseType>& on_incoming_msg,
      GrpcAsyncExecutor::RpcChannelClosedCallback on_channel_closed,
      std::unique_ptr<grpc::ClientContext> context) {
    request.set_allocated_header(BuildRequestHeader().release());
    auto scoped_stream = dispatcher_.ExecuteAsyncServerStreamingRpc(
        std::move(rpc), std::move(context), request, on_incoming_msg,
        std::move(on_channel_closed));
    std::move(on_stream_started).Run(std::move(scoped_stream));
  }

  void GetOAuthTokenAndExecuteRpc(
      ExecuteRpcWithContextCallback execute_rpc_with_context);

  void ExecuteRpcWithFetchedOAuthToken(
      ExecuteRpcWithContextCallback execute_rpc_with_context,
      OAuthTokenGetter::Status status,
      const std::string& user_email,
      const std::string& access_token);

  static std::unique_ptr<grpc::ClientContext> CreateClientContext();
  std::unique_ptr<ftl::RequestHeader> BuildRequestHeader();

  std::shared_ptr<grpc::ChannelInterface> channel_;
  OAuthTokenGetter* token_getter_;
  GrpcAsyncExecutor dispatcher_;
  std::string auth_token_;

  base::WeakPtrFactory<FtlGrpcContext> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FtlGrpcContext);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_GRPC_CONTEXT_H_
