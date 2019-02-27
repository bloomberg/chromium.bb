// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_GRPC_ASYNC_DISPATCHER_H_
#define REMOTING_SIGNALING_GRPC_ASYNC_DISPATCHER_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "remoting/signaling/grpc_async_call_data.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"

namespace remoting {

// This class helps adopting the gRPC async completion queue handling logic into
// Chromium's callback paradigm.
//
// Basic usage looks like this:
//
//   class MyClass {
//    public:
//     MyClass() : weak_factory_(this) {}
//     ~MyClass() {}
//
//     void SayHello() {
//       HelloRequest request;
//       dispatcher_->ExecuteAsyncRpc(
//           // This is run immediately inside the call stack of
//           // |ExecuteAsyncRpc|.
//           base::BindOnce(&HelloService::Stub::AsyncSayHello,
//                          base::Unretained(stub_.get())),
//           std::make_unique<grpc::ClientContext>(), request,
//           // Callback might be called after the dispatcher is destroyed.
//           base::BindOnce(&MyClass::OnHelloResult,
//                          weak_factory_.GetWeakPtr()));
//     }
//
//    private:
//     void OnHelloResult(grpc::Status status,
//                        const HelloResponse& response) {
//       if (status.error_code() == grpc::StatusCode::CANCELLED) {
//         // The request has been canceled because |dispatcher_| is destroyed.
//         // If you need to access class members here, make sure to bind a weak
//         // pointer in the RpcCallback. Otherwise using base::Unretained() is
//         // fine.
//         return;
//       }
//
//       if (!status.ok()) {
//         // Handle other error here.
//         return;
//       }
//
//       // Response is received. Use the result here.
//     }
//
//     std::unique_ptr<HelloService::Stub> stub_;
//     GrpcAsyncDispatcher dispatcher_;
//     base::WeakPtrFactory<MyClass> weak_factory_;
//   };
class GrpcAsyncDispatcher {
 public:
  template <typename ResponseType>
  using RpcCallback =
      base::OnceCallback<void(grpc::Status, const ResponseType&)>;

  template <typename RequestType, typename ResponseType>
  using AsyncRpcFunction = base::OnceCallback<std::unique_ptr<
      grpc::ClientAsyncResponseReader<ResponseType>>(grpc::ClientContext*,
                                                     const RequestType&,
                                                     grpc::CompletionQueue*)>;

  GrpcAsyncDispatcher();
  ~GrpcAsyncDispatcher();

  // Immediately executes |rpc_function| inside the call stack of this function,
  // and runs |callback| once the server responses. If the dispatcher is
  // destroyed before the server responses, |callback| will be called with
  // a CANCELLED status *after* the dispatcher is destroyed.
  //
  // It is safe to bind raw pointer into |rpc_function|, but you might want to
  // bind weak pointer in |callback| if you need to access your bound object in
  // the cancel case.
  template <typename RequestType, typename ResponseType>
  void ExecuteAsyncRpc(AsyncRpcFunction<RequestType, ResponseType> rpc_function,
                       std::unique_ptr<grpc::ClientContext> context,
                       const RequestType& request,
                       RpcCallback<ResponseType> callback) {
    auto response_reader =
        std::move(rpc_function).Run(context.get(), request, &completion_queue_);
    auto data = std::make_unique<GrpcAsyncCallData<ResponseType>>(
        std::move(context), std::move(response_reader), std::move(callback));
    RegisterRpcData(std::move(data));
  }

 private:
  void RunQueueOnDispatcherThread();
  void RegisterRpcData(std::unique_ptr<GrpcAsyncCallDataBase> rpc_data);

  // We need a dedicated thread because getting response from the completion
  // queue will block until any response is received. Note that the RPC call
  // itself is still async, meaning any new RPC call when the queue is blocked
  // can still be made, and can unblock the queue once the response is ready.
  base::Thread dispatcher_thread_{"grpc_async_dispatcher"};

  // Note that the gRPC library is thread-safe.
  grpc::CompletionQueue completion_queue_;

  // Keep the list of pending RPCs so that we can cancel them at destruction.
  base::flat_set<GrpcAsyncCallDataBase*> pending_rpcs_;
  base::Lock pending_rpcs_lock_;

  DISALLOW_COPY_AND_ASSIGN(GrpcAsyncDispatcher);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_GRPC_ASYNC_DISPATCHER_H_
