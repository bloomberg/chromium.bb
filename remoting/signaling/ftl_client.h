// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_CLIENT_H_
#define REMOTING_SIGNALING_FTL_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "remoting/signaling/ftl_services.grpc.pb.h"
#include "remoting/signaling/grpc_async_dispatcher.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {

// This is the class that makes RPC calls to the FTL signaling backend.
class FtlClient {
 public:
  template <typename ResponseType>
  using RpcCallback =
      base::OnceCallback<void(grpc::Status, const ResponseType&)>;

  FtlClient();
  ~FtlClient();

  // Retrieves the ice server configs.
  void GetIceServer(RpcCallback<ftl::GetICEServerResponse> callback);

 private:
  using PeerToPeer =
      google::internal::communications::instantmessaging::v1::PeerToPeer;

  static std::unique_ptr<grpc::ClientContext> CreateClientContext();
  static std::unique_ptr<ftl::RequestHeader> BuildRequestHeader();

  std::unique_ptr<PeerToPeer::Stub> peer_to_peer_stub_;
  GrpcAsyncDispatcher dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(FtlClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_CLIENT_H_
