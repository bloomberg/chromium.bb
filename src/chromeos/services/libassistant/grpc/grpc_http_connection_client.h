// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_HTTP_CONNECTION_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_HTTP_CONNECTION_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/grpc_transport/streaming/bidi_streaming_rpc_call.h"
#include "chromeos/assistant/internal/grpc_transport/streaming/streaming_write_queue.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/http_connection_service.grpc.pb.h"
#include "chromeos/services/libassistant/grpc/grpc_client_thread.h"
#include "chromeos/services/libassistant/grpc/grpc_state.h"
#include "chromeos/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"

namespace chromeos {
namespace libassistant {

class GrpcHttpConnectionClient {
 public:
  GrpcHttpConnectionClient(
      assistant_client::HttpConnectionFactory* http_connection_factory,
      const std::string& server_address);
  GrpcHttpConnectionClient(const GrpcHttpConnectionClient&) = delete;
  GrpcHttpConnectionClient& operator=(const GrpcHttpConnectionClient&) = delete;
  ~GrpcHttpConnectionClient();

  void set_http_connection_factory(
      assistant_client::HttpConnectionFactory* http_connection_factory) {
    http_connection_factory_ = http_connection_factory;
  }

  void ScheduleRequest(::assistant::api::StreamHttpConnectionRequest request);

  // Starts to register itself as a client of Libassistant gRPC http connection
  // service.
  void Start();

 private:
  friend class TestGrpcHttpConnectionService;

  void CleanUp();

  void OnRpcWriteAvailable(
      grpc::ClientContext* context,
      StreamingWriter<::assistant::api::StreamHttpConnectionRequest>* writer);
  void OnRpcReadAvailable(
      grpc::ClientContext* context,
      const ::assistant::api::StreamHttpConnectionResponse& request);
  void OnRpcExited(grpc::ClientContext* context, const grpc::Status& status);

  // `http_connection_factory_` must outlive this class.
  base::raw_ptr<assistant_client::HttpConnectionFactory>
      http_connection_factory_;

  // The following section is only accessed by the constructor thread.
  // Thread running the completion queue.  CQ has to be shutdown before we
  // destroy |call_|.
  // NOTE: All http connections share the same CQ. If there is any problem, e.g.
  // latency, we probably can create one GrpcHttpConnectionClient for one http
  // connection.
  std::unique_ptr<GrpcClientThread> cq_thread_;
  // This channel will be shared between all http connections used to
  // communicate with server. All channels are reference counted and will be
  // freed automatically.
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<::assistant::api::HttpConnectionService::Stub> stub_;
  std::unique_ptr<
      BidiStreamingRpcCall<::assistant::api::StreamHttpConnectionRequest,
                           ::assistant::api::StreamHttpConnectionResponse>>
      call_;

  // The following section is only accessed by the CQ thread.
  // `init_request_sent_` is only modified by `OnRpcWriteAvailable()` which is
  // guaranteed to be called in sequence by the gRPC runtime, so there's no
  // concurrency issue. No lock needed.
  bool init_request_sent_ = false;

  // |write_queue_| methods are thread safe.
  std::unique_ptr<
      StreamingWriteQueue<::assistant::api::StreamHttpConnectionRequest>>
      write_queue_;

  // `http_connection` owns itself and will be deleted when `Close()` is called.
  // When clean up `http_connections_`, will call `Close()` on the elements.
  base::flat_map<int, assistant_client::HttpConnection*> http_connections_;
  // `delegate` owns itself.
  base::flat_map<int, assistant_client::HttpConnection::Delegate*> delegates_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<GrpcHttpConnectionClient> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_HTTP_CONNECTION_CLIENT_H_
