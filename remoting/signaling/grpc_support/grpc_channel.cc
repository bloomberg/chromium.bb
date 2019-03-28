// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/grpc_support/grpc_channel.h"

#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

GrpcChannelSharedPtr CreateSslChannelForEndpoint(const std::string& endpoint) {
  auto channel_creds = grpc::SslCredentials(grpc::SslCredentialsOptions());
  return grpc::CreateChannel(endpoint, channel_creds);
}

}  // namespace remoting
