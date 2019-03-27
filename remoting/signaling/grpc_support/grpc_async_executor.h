// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_EXECUTOR_H_
#define REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_EXECUTOR_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "remoting/signaling/grpc_support/grpc_async_request.h"
#include "remoting/signaling/grpc_support/grpc_async_server_streaming_request.h"
#include "remoting/signaling/grpc_support/grpc_async_unary_request.h"
#include "remoting/signaling/grpc_support/scoped_grpc_server_stream.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"

namespace remoting {

// This class helps adapting the gRPC async completion queue handling logic into
// Chromium's callback paradigm. See README.md for detailed usage.
class GrpcAsyncExecutor {
 public:
  template <typename ResponseType>
  using RpcCallback =
      base::OnceCallback<void(const grpc::Status&, const ResponseType&)>;

  template <typename RequestType, typename ResponseType>
  using AsyncRpcFunction = base::OnceCallback<std::unique_ptr<
      grpc::ClientAsyncResponseReader<ResponseType>>(grpc::ClientContext*,
                                                     const RequestType&,
                                                     grpc::CompletionQueue*)>;

  template <typename ResponseType>
  using RpcStreamCallback = base::RepeatingCallback<void(const ResponseType&)>;

  using RpcChannelClosedCallback =
      base::OnceCallback<void(const grpc::Status&)>;

  template <typename RequestType, typename ResponseType>
  using AsyncServerStreamingRpcFunction =
      base::OnceCallback<std::unique_ptr<grpc::ClientAsyncReader<ResponseType>>(
          grpc::ClientContext*,
          const RequestType&,
          grpc::CompletionQueue*,
          void*)>;

  GrpcAsyncExecutor();
  ~GrpcAsyncExecutor();

  // Executes |rpc_function| before this function returns, and runs |callback|
  // once the server responds. Callback will be silently dropped if the
  // dispatcher is destroyed before the response is received.
  template <typename RequestType, typename ResponseType>
  void ExecuteAsyncRpc(AsyncRpcFunction<RequestType, ResponseType> rpc_function,
                       std::unique_ptr<grpc::ClientContext> context,
                       const RequestType& request,
                       RpcCallback<ResponseType> callback) {
    auto response_reader =
        std::move(rpc_function).Run(context.get(), request, &completion_queue_);
    auto data = std::make_unique<internal::GrpcAsyncUnaryRequest<ResponseType>>(
        std::move(context), std::move(response_reader), std::move(callback));
    RegisterRpcData(std::move(data));
  }

  // Starts a server streaming RPC. Delete or reset the returned stream holder
  // if you need to stop the stream.
  //
  // |on_incoming_msg| is called whenever a new message is received from the
  // server.
  // |on_channel_closed| will be called if the channel is closed by the server
  // or the connection is dropped.
  // All callbacks will be silently dropped if the dispatcher is destroyed
  // before the response is received.
  template <typename RequestType, typename ResponseType>
  std::unique_ptr<ScopedGrpcServerStream> ExecuteAsyncServerStreamingRpc(
      AsyncServerStreamingRpcFunction<RequestType, ResponseType> rpc_function,
      std::unique_ptr<grpc::ClientContext> context,
      const RequestType& request,
      const RpcStreamCallback<ResponseType>& on_incoming_msg,
      RpcChannelClosedCallback on_channel_closed) WARN_UNUSED_RESULT {
    auto start_and_create_reader_cb = base::BindOnce(
        std::move(rpc_function), context.get(), request, &completion_queue_);
    auto data = std::make_unique<
        internal::GrpcAsyncServerStreamingRequest<ResponseType>>(
        std::move(context), std::move(start_and_create_reader_cb),
        on_incoming_msg, std::move(on_channel_closed));
    std::unique_ptr<ScopedGrpcServerStream> stream_holder =
        data->CreateStreamHolder();
    RegisterRpcData(std::move(data));
    return stream_holder;
  }

 private:
  void RunQueueOnDispatcherThread();
  void RegisterRpcData(std::unique_ptr<internal::GrpcAsyncRequest> rpc_data);

  // We need a dedicated thread because getting response from the completion
  // queue will block until any response is received. Note that the RPC call
  // itself is still async, meaning any new RPC call when the queue is blocked
  // can still be made, and can unblock the queue once the response is ready.
  base::Thread dispatcher_thread_{"grpc_async_executor"};

  // Note that the gRPC library is thread-safe.
  grpc::CompletionQueue completion_queue_;

  // Keep the list of pending RPCs so that we can cancel them at destruction.
  base::flat_set<internal::GrpcAsyncRequest*> pending_rpcs_
      GUARDED_BY(pending_rpcs_lock_);
  base::Lock pending_rpcs_lock_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncExecutor);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_SUPPORT_GRPC_ASYNC_EXECUTOR_H_
