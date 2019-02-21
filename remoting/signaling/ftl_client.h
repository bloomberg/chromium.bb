// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_CLIENT_H_
#define REMOTING_SIGNALING_FTL_CLIENT_H_

#include <memory>

#include "base/macros.h"

// TODO(crbug.com/933949): Remove suppression once gRPC is fixed.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#endif

#include "remoting/signaling/ftl_services.grpc.pb.h"

#if defined(__clang__)
#pragma clang diagnostic pop  // -Wextra-semi
#endif

#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {

// This is the class that makes RPC calls to the FTL signaling backend.
class FtlClient {
 public:
  FtlClient();
  ~FtlClient();

  // Retrieves the ice server configs and write output to |response|.
  // TODO(yuweih): Change this to async interface.
  grpc::Status GetIceServer(ftl::GetICEServerResponse* response);

 private:
  using PeerToPeer =
      google::internal::communications::instantmessaging::v1::PeerToPeer;

  static void FillClientContextMetadata(grpc::ClientContext* context);
  static std::unique_ptr<ftl::RequestHeader> BuildRequestHeader();

  std::unique_ptr<PeerToPeer::Stub> peer_to_peer_stub_;

  DISALLOW_COPY_AND_ASSIGN(FtlClient);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_CLIENT_H_
